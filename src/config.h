#pragma once
// Everything you would normally want to change lives here.

// ---------------------------------------------------------------------------
// Colour scheme
//
// Default is inverted radar: a green ground with black ink on top. Define
// CLASSIC_SCHEME for the traditional black ground with green ink.
// ---------------------------------------------------------------------------
// #define CLASSIC_SCHEME

// The panel backlight is a simple on/off enable and is already driven full on,
// so screen brightness is set by these colours. Raise the C_GROUND components
// together to brighten the whole face; keep C_GRID well below it or the rings
// stop reading as background.

// Named C_RGB rather than RGB565 because Arduino_GFX already defines that.
#define C_RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#ifdef CLASSIC_SCHEME
  #define C_GROUND C_RGB(0, 0, 0)        // black background
  #define C_INK    C_RGB(96, 255, 128)   // phosphor green data
  #define C_GRID   C_RGB(32, 132, 52)    // dim green rings
  #define C_CHIP_BG C_INK
  #define C_CHIP_FG C_GROUND
#else
  #define C_GROUND C_RGB(96, 236, 112)   // phosphor green background
  #define C_INK    C_RGB(0, 0, 0)        // black data
  #define C_GRID   C_RGB(38, 146, 58)    // darker green rings, sit under the data
  #define C_CHIP_BG C_INK
  #define C_CHIP_FG C_GROUND
#endif

// The airport beacon deliberately breaks the two-colour scheme so it reads as a
// fixed place at a glance, never as traffic. Red works on both grounds.
#define C_AIRPORT C_RGB(232, 32, 32)

// Aircraft history trails sit between the grid and the ink: clearly subordinate
// to the aircraft they belong to, clearly above the rings.
#ifdef CLASSIC_SCHEME
  #define C_TRAIL C_RGB(40, 150, 60)
#else
  #define C_TRAIL C_RGB(22, 86, 34)
#endif

// ---------------------------------------------------------------------------
// Cities — button 1 (BOOT / GPIO0) cycles this list. Index 0 is the default.
// Add your own: {"NAME", latitude, longitude}. Names render at text size 1,
// so keep them under about 12 characters.
// ---------------------------------------------------------------------------
struct City {
    const char* name;
    double lat;
    double lon;
    const char* airport;    // IATA code drawn beside the airport marker
    double airportLat;
    double airportLon;
};

static const City CITIES[] = {
    //  name         city centre            main airport
    {"RIGA",      56.9496, 24.1052, "RIX", 56.9236, 23.9711},
    {"VILNIUS",   54.6872, 25.2797, "VNO", 54.6341, 25.2858},
    {"TALLINN",   59.4370, 24.7536, "TLL", 59.4133, 24.8328},
    {"KAUNAS",    54.8985, 23.9036, "KUN", 54.9639, 24.0848},
    {"HELSINKI",  60.1699, 24.9384, "HEL", 60.3172, 24.9633},
    {"STOCKHOLM", 59.3293, 18.0686, "ARN", 59.6519, 17.9186},
    {"WARSAW",    52.2297, 21.0122, "WAW", 52.1657, 20.9671},
};
static const int CITY_COUNT = sizeof(CITIES) / sizeof(CITIES[0]);

// ---------------------------------------------------------------------------
// Ranges — button 2 (GPIO14) cycles these. The outer ring is this many km.
// ---------------------------------------------------------------------------
static const int RANGES_KM[] = {20, 50, 100, 200};
static const int RANGE_COUNT = sizeof(RANGES_KM) / sizeof(RANGES_KM[0]);
static const int DEFAULT_RANGE_INDEX = 1;  // 50 km

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
static const uint32_t FETCH_INTERVAL_MS = 10000;  // adsb.lol asks for polite polling
static const uint32_t FRAME_INTERVAL_MS = 100;    // ~10 fps dead-reckoned motion
static const uint32_t STALE_AFTER_MS    = 30000;  // show the STALE badge
static const uint32_t DISCARD_AFTER_MS  = 120000; // drop the aircraft list entirely
static const uint32_t HTTP_TIMEOUT_MS   = 8000;
static const uint32_t WIFI_RETRY_MS     = 5000;
static const uint32_t BUTTON_DEBOUNCE_MS = 40;

// ---------------------------------------------------------------------------
// History trails
//
// TRAIL_POINTS x TRAIL_SAMPLE_MS is how far back a trail reaches. At the
// default 32 x 5 s that is about two and a half minutes of flight: roughly
// 30 km behind an airliner, which is a long streak at 20 km range and a short
// stub at 200 km. Raise TRAIL_POINTS (in trails.h) for longer history; it
// costs 8 bytes per point per aircraft.
// ---------------------------------------------------------------------------
static const uint32_t TRAIL_SAMPLE_MS  = 5000;
static const uint32_t TRAIL_MAX_AGE_MS = 120000;  // forget an aircraft not seen this long

// ---------------------------------------------------------------------------
// Buttons (active low, internal pull-ups)
// ---------------------------------------------------------------------------
static const int PIN_BTN_CITY  = 0;   // BOOT button
static const int PIN_BTN_RANGE = 14;

// ---------------------------------------------------------------------------
// Layout for the 320x170 landscape panel
// ---------------------------------------------------------------------------
static const int HEADER_H     = 18;
static const int RADAR_CX     = 88;
static const int RADAR_CY     = 95;
static const int RADAR_R      = 72;
static const int PANEL_X      = 180;
static const int PANEL_W      = 136;
static const int PANEL_ROWS   = 4;    // aircraft listed in the side panel

// ---------------------------------------------------------------------------
// Time — used only for the header clock
// ---------------------------------------------------------------------------
#define NTP_SERVER "pool.ntp.org"
// POSIX TZ string. Default is Latvia (EET/EEST, EU DST rules).
#define TZ_STRING "EET-2EEST,M3.5.0/3,M10.5.0/4"
