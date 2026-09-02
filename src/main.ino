// Flydar — a realtime flight radar for the LilyGO T-Display-S3.
//
// Fetches ADS-B positions from api.adsb.lol on a background task, draws a
// north-up radar centred on the selected city, and glides each aircraft along
// its own track between fetches so the picture is continuously live.
//
// BOOT (GPIO0) cycles the city. GPIO14 cycles the range.

#include <Arduino_GFX_Library.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "adsb.h"
#include "buttons.h"
#include "cert_roots.h"
#include "config.h"
#include "display_config.h"
#include "geo.h"
#include "radar_ui.h"
#include "routes.h"
#include "secrets.h"
#include "settings.h"
#include "weather.h"
#include "trails.h"

// ---- Display: built from display_config.h ----
#if defined(BUS_PARALLEL8)
Arduino_DataBus *bus = new Arduino_ESP32PAR8Q(
    PIN_DC, PIN_CS, PIN_WR, PIN_RD,
    PAR_D0, PAR_D1, PAR_D2, PAR_D3, PAR_D4, PAR_D5, PAR_D6, PAR_D7);
#elif defined(BUS_SPI)
Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_DC, PIN_CS, PIN_SCK, PIN_MOSI, PIN_MISO);
#else
  #error "display_config.h: pick a bus (BUS_PARALLEL8 or BUS_SPI)"
#endif

#if defined(DRIVER_ST7789)
Arduino_G *output = new Arduino_ST7789(
    bus, PIN_RST, TFT_ROTATION, TFT_IPS,
    TFT_WIDTH, TFT_HEIGHT, TFT_COL_OFFSET, TFT_ROW_OFFSET, TFT_COL_OFFSET, TFT_ROW_OFFSET);
#elif defined(DRIVER_ILI9341)
Arduino_G *output = new Arduino_ILI9341(bus, PIN_RST, TFT_ROTATION, TFT_IPS);
#else
  #error "display_config.h: pick a driver (DRIVER_ST7789 or DRIVER_ILI9341)"
#endif

#if (TFT_ROTATION == 1 || TFT_ROTATION == 3)
  #define SCREEN_W TFT_HEIGHT
  #define SCREEN_H TFT_WIDTH
#else
  #define SCREEN_W TFT_WIDTH
  #define SCREEN_H TFT_HEIGHT
#endif
// Off-screen framebuffer: every frame is composed in RAM and blitted in one
// pass, so the radar never tears.
Arduino_Canvas *gfx = new Arduino_Canvas(SCREEN_W, SCREEN_H, output);

// ---- Shared state between the fetch task and the render loop ----
static SemaphoreHandle_t gLock;
static adsb::Snapshot gPublished;      // most recent successful fetch
static volatile bool gHasNew = false;  // gPublished not yet picked up
static volatile uint32_t gLastGoodMs = 0;
static volatile bool gRefetchNow = false;

// Airport weather, refreshed far more slowly than the traffic.
static char gWeatherLine[48] = "";
static uint32_t gLastWeatherMs = 0;
static uint32_t gWeatherEveryMs = WEATHER_INTERVAL_MS;
static volatile bool gWeatherStale = true;

// Owned by the render loop: the snapshot actually on screen, dead-reckoned
// forward between fetches.
static adsb::Snapshot gWorking;
static uint32_t gLastMotionMs = 0;

static geo::LatLon currentCentre() {
    const City &c = CITIES[settings::cityIndex()];
    return {c.lat, c.lon};
}

static int currentRangeKm() { return RANGES_KM[settings::rangeIndex()]; }

// ---------------------------------------------------------------------------
// Networking
// ---------------------------------------------------------------------------

static void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(250);
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi: %s\n", WiFi.localIP().toString().c_str());
        configTzTime(TZ_STRING, NTP_SERVER);
    }
}

// One HTTPS GET, body into `out`. Shared by the aircraft feed and the route
// files, which live on different hosts under different roots.
static bool httpGetBody(const char *url, String &out) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setCACert(ADSB_ROOT_CAS);
    client.setTimeout(HTTP_TIMEOUT_MS / 1000);

    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setUserAgent("flydar/1.0 (+esp32)");
    if (!http.begin(client, url)) return false;

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }
    out = http.getString();
    http.end();
    return true;
}

static bool fetchAircraft(adsb::Snapshot &out) {
    geo::LatLon c = currentCentre();
    // adsb.lol takes a radius in nautical miles; round up so the outer ring is
    // always fully covered.
    int nmi = (int)ceil(currentRangeKm() / 1.852);
    if (nmi < 1) nmi = 1;
    if (nmi > 250) nmi = 250;  // API maximum

    char url[128];
    snprintf(url, sizeof(url), "https://api.adsb.lol/v2/lat/%.4f/lon/%.4f/dist/%d",
             c.lat, c.lon, nmi);

    String body;
    if (!httpGetBody(url, body)) {
        // Before the radio associates this is expected, not worth reporting.
        if (WiFi.status() == WL_CONNECTED) Serial.println("fetch: failed");
        return false;
    }
    if (!adsb::parse(body.c_str(), body.length(), out)) {
        Serial.println("fetch: bad JSON");
        return false;
    }
    return true;
}

// Looks up one callsign's origin/destination and remembers the answer, so a
// route costs one request per aircraft ever, not one per refresh. The static
// route files are laid out by the first two characters of the callsign.
static void fetchOneRoute(const char *callsign) {
    char url[160];
    snprintf(url, sizeof(url), "https://vrs-standing-data.adsb.lol/routes/%c%c/%s.json",
             callsign[0], callsign[1], callsign);

    String body;
    char route[routes::ROUTE_LEN];
    if (httpGetBody(url, body) &&
        routes::parseRouteBody(body.c_str(), body.length(), route, sizeof(route))) {
        routes::store(callsign, route);
        Serial.printf("route: %s %s\n", callsign, route);
    } else {
        // Remember the miss too, or every refresh would ask again.
        routes::store(callsign, "");
    }
}

// The airport's METAR. Issued about every half hour, so polled every ten
// minutes and re-read immediately when the city changes.
static void fetchWeather() {
    const City &city = CITIES[settings::cityIndex()];
    char url[160];
    snprintf(url, sizeof(url),
             "https://aviationweather.gov/api/data/metar?ids=%s&format=json", city.icao);

    String body;
    weather::Report report;
    bool ok = httpGetBody(url, body) && weather::parse(body.c_str(), body.length(), report);
    if (ok) {
        char line[48];
        weather::format(report, city.airport, line, sizeof(line));
        xSemaphoreTake(gLock, portMAX_DELAY);
        snprintf(gWeatherLine, sizeof(gWeatherLine), "%s", line);
        xSemaphoreGive(gLock);
        Serial.printf("metar: %s\n", line);
    } else {
        Serial.printf("metar: %s unavailable\n", city.icao);
    }
    gLastWeatherMs = millis();
    gWeatherStale = false;
    // A failure must not lock the next attempt out for the full interval: the
    // first try happens before WiFi has associated and always fails.
    gWeatherEveryMs = ok ? WEATHER_INTERVAL_MS : WEATHER_RETRY_MS;
}

// Runs on the other core so a slow or failing request never stalls the radar.
static void fetchTask(void *) {
    uint32_t lastAttempt = 0;
    for (;;) {
        bool due = gRefetchNow || (millis() - lastAttempt >= FETCH_INTERVAL_MS);
        if (!due) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
        gRefetchNow = false;
        lastAttempt = millis();

        connectWiFi();

        adsb::Snapshot fresh;
        if (fetchAircraft(fresh)) {
            adsb::computeRelative(fresh, currentCentre());
            adsb::sortByDistance(fresh);
            Serial.printf("fetch: %d aircraft, %d within %dkm\n", fresh.count,
                          adsb::countWithin(fresh, currentRangeKm()), currentRangeKm());
            xSemaphoreTake(gLock, portMAX_DELAY);
            gPublished = fresh;
            gHasNew = true;
            gLastGoodMs = millis();
            xSemaphoreGive(gLock);

            // At most one route lookup per cycle: the panel fills in over the
            // next few refreshes instead of firing a burst of requests.
            const char *pending = routes::nextPending(fresh, PANEL_ROWS);
            if (pending) fetchOneRoute(pending);
        }

        if (WiFi.status() == WL_CONNECTED &&
            (gWeatherStale || (millis() - gLastWeatherMs) >= gWeatherEveryMs)) {
            fetchWeather();
        }
    }
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

static radar_ui::Status currentStatus() {
    uint32_t good = gLastGoodMs;
    if (good == 0) return radar_ui::Status::Connecting;
    uint32_t age = millis() - good;
    if (age > DISCARD_AFTER_MS) return radar_ui::Status::NoData;
    if (age > STALE_AFTER_MS || WiFi.status() != WL_CONNECTED) return radar_ui::Status::Stale;
    return radar_ui::Status::Ok;
}

static void clockString(char *out, size_t n) {
    time_t now = time(nullptr);
    struct tm tm_now;
    // Before NTP has synced the clock sits in 1970; show nothing rather than lie.
    if (now < 1600000000 || !localtime_r(&now, &tm_now)) { out[0] = '\0'; return; }
    strftime(out, n, "%H:%M:%S", &tm_now);
}

static void renderFrame() {
    geo::LatLon centre = currentCentre();
    int rangeKm = currentRangeKm();
    uint32_t now = millis();

    // Pick up a fresh fetch if the task published one.
    if (gHasNew && xSemaphoreTake(gLock, 0) == pdTRUE) {
        gWorking = gPublished;
        gHasNew = false;
        xSemaphoreGive(gLock);
        gLastMotionMs = now;
        adsb::computeRelative(gWorking, centre);
    }

    radar_ui::Status status = currentStatus();
    if (status == radar_ui::Status::NoData) gWorking.count = 0;

    // Glide everything forward by the time since the last frame.
    double dt = (now - gLastMotionMs) / 1000.0;
    gLastMotionMs = now;
    if (dt > 0.0 && gWorking.count > 0) adsb::deadReckon(gWorking, centre, dt);
    adsb::sortByDistance(gWorking);

    trails::record(gWorking, now, TRAIL_SAMPLE_MS);
    trails::expire(now, TRAIL_MAX_AGE_MS);

    char clock[12];
    clockString(clock, sizeof(clock));

    const City &city = CITIES[settings::cityIndex()];
    geo::LatLon airport{city.airportLat, city.airportLon};

    // Is anything directly above you right now? Measured from home, which is
    // not the radar centre.
    const char *overheadHex = nullptr;
    const char *overheadCallsign = nullptr;
    bool homeValid = (HOME_LAT != 0.0 || HOME_LON != 0.0);
    geo::LatLon home{HOME_LAT, HOME_LON};
    if (homeValid && gWorking.count > 0) {
        float km = 0.0f;
        int i = adsb::nearestToPoint(gWorking, home, km);
        if (i >= 0 && km <= (float)OVERHEAD_RADIUS_KM) {
            overheadHex = gWorking.ac[i].hex;
            overheadCallsign = gWorking.ac[i].callsign;
        }
    }

    radar_ui::Frame frame{
        .cityName = city.name,
        .rangeKm = rangeKm,
        .snap = &gWorking,
        .inRange = adsb::countWithin(gWorking, rangeKm),
        .status = status,
        .clock = clock,
        .airportCode = city.airport,
        .airportDistKm = (float)geo::distanceKm(centre, airport),
        .airportBearingDeg = (float)geo::bearingDeg(centre, airport),
        .weather = gWeatherLine,
        .homeValid = homeValid,
        .homeDistKm = homeValid ? (float)geo::distanceKm(centre, home) : 0.0f,
        .homeBearingDeg = homeValid ? (float)geo::bearingDeg(centre, home) : 0.0f,
        .overheadHex = overheadHex,
        .overheadCallsign = overheadCallsign,
        // Driven from the clock, not the frame counter, so the sweep turns at a
        // steady rate whatever the render loop manages.
        .sweepDeg = SWEEP_PERIOD_MS ? (float)((now % SWEEP_PERIOD_MS) * 360.0 / SWEEP_PERIOD_MS)
                                    : -1.0f,
    };
    radar_ui::draw(gfx, frame);
    gfx->flush();

    static uint32_t frames = 0, statsSince = 0;
    if (statsSince == 0) statsSince = now;
    if (++frames >= 200) {
        Serial.printf("render: %.1f fps\n", frames * 1000.0 / (millis() - statsSince));
        frames = 0;
        statsSince = millis();
    }
}

// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
#if PIN_PWR >= 0
    pinMode(PIN_PWR, OUTPUT); digitalWrite(PIN_PWR, HIGH);   // enable panel power
#endif
#if PIN_BL >= 0
    pinMode(PIN_BL, OUTPUT); digitalWrite(PIN_BL, HIGH);     // backlight on
#endif

    settings::begin();
    buttons::begin();

    gfx->begin();
    gLock = xSemaphoreCreateMutex();
    gLastMotionMs = millis();
    renderFrame();  // draw the empty scope immediately, before the radio warms up

    xTaskCreatePinnedToCore(fetchTask, "adsb", 12288, nullptr, 1, nullptr, 0);
}

void loop() {
    if (buttons::cityPressed()) {
        settings::nextCity();
        gWorking.count = 0;      // the old city's traffic is meaningless here
        trails::clear();         // and trails are offsets from the old centre
        routes::clear();         // free the cache for the new city's traffic
        gWeatherLine[0] = '\0';  // and the old airport's weather
        gWeatherStale = true;
        gWeatherEveryMs = WEATHER_INTERVAL_MS;
        gLastGoodMs = 0;
        gRefetchNow = true;
    }
    if (buttons::rangePressed()) {
        settings::nextRange();
        gRefetchNow = true;      // the request radius changed
    }

    static uint32_t lastFrame = 0;
    uint32_t now = millis();
    if (now - lastFrame >= FRAME_INTERVAL_MS) {
        lastFrame = now;
        renderFrame();
    }
    delay(5);
}
