#pragma once
// Everything you would normally want to change lives here.

// ---------------------------------------------------------------------------
// Colour scheme
//
// Default is inverted radar: a green ground with black ink on top. Define
// CLASSIC_SCHEME for the traditional black ground with green ink.
// ---------------------------------------------------------------------------
// #define CLASSIC_SCHEME

// Named C_RGB rather than RGB565 because Arduino_GFX already defines that.
#define C_RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#ifdef CLASSIC_SCHEME
  #define C_GROUND C_RGB(0, 0, 0)        // black background
  #define C_INK    C_RGB(64, 255, 96)    // phosphor green data
  #define C_GRID   C_RGB(24, 104, 40)    // dim green rings
  #define C_CHIP_BG C_INK
  #define C_CHIP_FG C_GROUND
#else
  #define C_GROUND C_RGB(64, 205, 88)    // phosphor green background
  #define C_INK    C_RGB(0, 0, 0)        // black data
  #define C_GRID   C_RGB(30, 122, 48)    // darker green rings, sit under the data
  #define C_CHIP_BG C_INK
  #define C_CHIP_FG C_GROUND
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
};

static const City CITIES[] = {
    {"RIGA",      56.9496, 24.1052},
    {"VILNIUS",   54.6872, 25.2797},
    {"TALLINN",   59.4370, 24.7536},
    {"KAUNAS",    54.8985, 23.9036},
    {"HELSINKI",  60.1699, 24.9384},
    {"STOCKHOLM", 59.3293, 18.0686},
    {"WARSAW",    52.2297, 21.0122},
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
