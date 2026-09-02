#pragma once
// Position history per aircraft, so the radar can draw where each one has been.
//
// Kept separate from the aircraft snapshot because a snapshot is replaced
// wholesale on every fetch, while a trail has to survive that. Entries are
// keyed by ICAO hex address and store offsets in kilometres from the radar
// centre, which stays valid as the range changes. Arduino-free, host-testable.

#include <stdint.h>

#include "adsb.h"
#include "geo.h"

namespace trails {

constexpr int MAX_TRAILS = adsb::MAX_AIRCRAFT;
constexpr int TRAIL_POINTS = 32;

// Kilometres east (x) and north (y) of the radar centre.
struct Point {
    float x;
    float y;
};

// Forgets everything. Call when the radar centre moves.
void clear();

// Appends the current position of every aircraft in `s`, at most one point per
// aircraft per `sampleMs`. Requires computeRelative to have run on `s`; points
// are recorded relative to whatever centre it used.
void record(const adsb::Snapshot& s, uint32_t nowMs, uint32_t sampleMs);

// Copies an aircraft's history oldest-first into `out`, returning how many
// points were written. Zero when the aircraft has no trail yet.
int fetch(const char* hex, Point* out, int maxOut);

// Drops aircraft not seen for longer than maxAgeMs.
void expire(uint32_t nowMs, uint32_t maxAgeMs);

// How many aircraft currently have a trail. For tests and diagnostics.
int activeCount();

}  // namespace trails
