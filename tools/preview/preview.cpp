// Renders radar_ui frames to PPM files so the layout can be checked without
// flashing the board. Build with tools/preview/build.sh.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "adsb.h"
#include "config.h"
#include "radar_ui.h"
#include "routes.h"
#include "weather.h"
#include "trails.h"

static const geo::LatLon RIGA{56.9496, 24.1052};

// The preview must build from a clean checkout, where secrets.h does not exist,
// so it uses its own synthetic home rather than the real HOME_LAT/HOME_LON.
static const geo::LatLon PREVIEW_HOME{57.0000, 24.2000};

static std::string readFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path.c_str()); return {}; }
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    fclose(f);
    return out;
}

static void writePPM(const Arduino_GFX& g, const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    fprintf(f, "P6\n%d %d\n255\n", g.width(), g.height());
    for (uint16_t p : g.pixels()) {
        unsigned char rgb[3] = {(unsigned char)(((p >> 11) & 0x1F) * 255 / 31),
                                (unsigned char)(((p >> 5) & 0x3F) * 255 / 63),
                                (unsigned char)((p & 0x1F) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void render(const std::string& out, const adsb::Snapshot& snapIn, int rangeKm,
                   const char* cityName, radar_ui::Status status, const char* clock,
                   const char* metar = "", const char* overheadCs = nullptr,
                   const char* overheadHex = nullptr, float sweepDeg = 55.0f,
                   int rssi = -58) {
    // Look the city up so the preview draws the same airport the firmware would.
    const City* city = &CITIES[0];
    for (int i = 0; i < CITY_COUNT; i++)
        if (strcmp(CITIES[i].name, cityName) == 0) city = &CITIES[i];
    geo::LatLon centre{city->lat, city->lon};
    geo::LatLon airport{city->airportLat, city->airportLon};

    adsb::Snapshot snap = snapIn;
    adsb::computeRelative(snap, centre);
    adsb::sortByDistance(snap);

    // Synthesise the history the firmware would have accumulated: rewind the
    // sky by a full trail's worth of time, then step it forward recording as
    // it goes, so each aircraft ends up back at its true position.
    trails::clear();
    const double stepSec = TRAIL_SAMPLE_MS / 1000.0;
    adsb::Snapshot hist = snap;
    adsb::deadReckon(hist, centre, -stepSec * (trails::TRAIL_POINTS - 1));
    for (int k = 0; k < trails::TRAIL_POINTS; k++) {
        trails::record(hist, 100000 + k * TRAIL_SAMPLE_MS, TRAIL_SAMPLE_MS);
        if (k < trails::TRAIL_POINTS - 1) adsb::deadReckon(hist, centre, stepSec);
    }

    Arduino_GFX gfx(320, 170);
    radar_ui::Frame f{cityName,
                      rangeKm,
                      &snap,
                      adsb::countWithin(snap, rangeKm),
                      status,
                      clock,
                      city->airport,
                      (float)geo::distanceKm(centre, airport),
                      (float)geo::bearingDeg(centre, airport),
                      metar,
                      rssi,
                      true,
                      (float)geo::distanceKm(centre, PREVIEW_HOME),
                      (float)geo::bearingDeg(centre, PREVIEW_HOME),
                      overheadHex,
                      overheadCs,
                      sweepDeg};
    radar_ui::draw(&gfx, f);
    writePPM(gfx, out);
    printf("%-32s range=%3dkm inRange=%d/%d  %s at %.1fkm brg %.0f\n", out.c_str(), rangeKm,
           f.inRange, snap.count, f.airportCode, f.airportDistKm, f.airportBearingDeg);
}

// A busier sky than Riga happened to have when the fixtures were captured.
static adsb::Snapshot syntheticBusy(geo::LatLon centre) {
    std::string body = "{\"ac\":[";
    struct Row { const char* cs; const char* type; double brg, km, alt, track, rate; };
    const Row rows[] = {
        {"BTI1PA", "A220", 52,  4.7, 1250, 200, 1800},   // sits right over HOME
        {"RYR9JC", "B738", 95, 18.0, 11000, 260, -1600},
        {"SAS742",  "A20N", 160, 27.0, 22000, 340, 0},
        {"AFL2311", "B77W", 220, 41.0, 34000, 45,  0},
        {"DLH88X",  "A343", 275, 55.0, 29000, 120, -400},
        {"BTI711",  "BCS3", 320, 66.0, 18050, 190, 900},
        {"UZB211",  "B763", 40,  78.0, 20175, 250, 0},
        {"FIN1122", "E190", 200, 88.0, 9000,  10,  1200},
        {"YLEVI",   "C172", 350, 95.0, 300,   80,  0},
    };
    bool first = true;
    for (const Row& r : rows) {
        double t = r.brg * M_PI / 180.0;
        double north = r.km * cos(t), east = r.km * sin(t);
        double lat = centre.lat + north / geo::KM_PER_DEG_LAT;
        double lon = centre.lon + east / geo::kmPerDegLon(centre.lat);
        char one[256];
        snprintf(one, sizeof(one),
                 "%s{\"hex\":\"4%05x\",\"flight\":\"%s\",\"t\":\"%s\",\"lat\":%.6f,"
                 "\"lon\":%.6f,\"alt_baro\":%.0f,\"gs\":420,\"track\":%.1f,\"baro_rate\":%.0f}",
                 first ? "" : ",", (unsigned)(r.km * 100), r.cs, r.type, lat, lon, r.alt,
                 r.track, r.rate);
        body += one;
        first = false;
    }
    body += "]}";
    adsb::Snapshot s;
    adsb::parse(body.c_str(), body.size(), s);
    return s;
}

int main(int argc, char** argv) {
    std::string outDir = argc > 1 ? argv[1] : ".";
    std::string fixtures = argc > 2 ? argv[2] : "test/fixtures";

    adsb::Snapshot real;
    std::string body = readFile(fixtures + "/riga_5ac.json");
    if (!body.empty()) adsb::parse(body.c_str(), body.size(), real);

    geo::LatLon riga{CITIES[0].lat, CITIES[0].lon};
    geo::LatLon stockholm = riga;
    for (int i = 0; i < CITY_COUNT; i++)
        if (strcmp(CITIES[i].name, "STOCKHOLM") == 0)
            stockholm = {CITIES[i].lat, CITIES[i].lon};
    // Seed the route cache the way the board fills it in from adsb.lol, and
    // leave a couple of aircraft without one so the fallbacks are visible.
    routes::store("BTI1PA", "RIX>ARN");
    routes::store("RYR9JC", "STN>RIX");
    routes::store("SAS742", "CPH>HEL");
    routes::store("AFL2311", "LED>KGD");
    routes::store("DLH88X", "FRA>HEL");
    routes::store("BTI711", "RIX>TLL");
    routes::store("UZB211", "TAS>LHR");
    routes::store("FIN1122", "HEL>RIX");
    routes::store("BTI9KV", "RIX>DXB");
    routes::store("PYR013", "");     // API has no route: falls back to operator
    routes::store("YLEVI", "");      // light aircraft: falls back to registration

    adsb::Snapshot busy = syntheticBusy(riga);
    adsb::Snapshot busyStockholm = syntheticBusy(stockholm);
    adsb::Snapshot empty;

    render(outDir + "/01_real_200km.ppm", real, 200, "RIGA", radar_ui::Status::Ok, "12:04:37", "RIX SSW 4kt 19C BKN");
    render(outDir + "/02_real_50km.ppm", real, 50, "RIGA", radar_ui::Status::Ok, "12:04:37", "RIX CALM 19C BKN");
    render(outDir + "/03_busy_100km.ppm", busy, 100, "RIGA", radar_ui::Status::Ok, "12:04:37", "RIX WNW 18kt -3C OVC");
    render(outDir + "/04_busy_20km.ppm", busy, 20, "RIGA", radar_ui::Status::Ok, "12:04:37", "RIX VRB 3kt 7C FEW", "BTI1PA", "4001d6");
    render(outDir + "/05_connecting.ppm", empty, 50, "RIGA", radar_ui::Status::Connecting, "");
    render(outDir + "/06_stale.ppm", busyStockholm, 100, "STOCKHOLM", radar_ui::Status::Stale, "23:59:58", "ARN NNE 22kt -11C OVC");
    render(outDir + "/07_header_worstcase.ppm", busyStockholm, 200, "STOCKHOLM", radar_ui::Status::NoData, "23:59:58", "ARN NNE 22kt -11C OVC");
    return 0;
}
