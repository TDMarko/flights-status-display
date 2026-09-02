#pragma once
// All drawing. Composes one complete frame onto an off-screen canvas; main.ino
// blits it in a single pass so nothing ever tears or flickers.

#include <Arduino_GFX_Library.h>

#include "adsb.h"

namespace radar_ui {

enum class Status {
    Ok,          // fresh data
    Connecting,  // no WiFi yet, or no successful fetch yet
    Stale,       // last good fetch is getting old
    NoData,      // last good fetch is too old to trust; list cleared
};

struct Frame {
    const char* cityName;
    int rangeKm;
    const adsb::Snapshot* snap;  // sorted nearest-first, relative fields filled
    int inRange;                 // how many of them are inside rangeKm
    Status status;
    const char* clock;           // "12:04:37", or "" before NTP has synced

    // The city's main airport, as an offset from the radar centre. Null code
    // means "this city has none configured" and nothing is drawn.
    const char* airportCode;
    float airportDistKm;
    float airportBearingDeg;
};

void draw(Arduino_GFX* gfx, const Frame& f);

}  // namespace radar_ui
