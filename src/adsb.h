#pragma once
// Arduino-free decoding of an adsb.lol /v2 response into a fixed-size snapshot,
// plus the range/bearing bookkeeping the radar needs. Host-testable.

#include <stddef.h>

#include "geo.h"

namespace adsb {

constexpr int MAX_AIRCRAFT = 32;
constexpr int CALLSIGN_LEN = 10;

struct Aircraft {
    char hex[8];                 // ICAO 24-bit address, lower case
    char callsign[CALLSIGN_LEN]; // trailing padding stripped; falls back to registration, then hex
    char reg[10];                // tail number, empty when unknown
    char type[6];                // ICAO type designator, e.g. "B738"
    double lat;
    double lon;
    float altFt;                 // barometric altitude in feet; 0 when on the ground
    bool onGround;               // alt_baro was the string "ground"
    bool hasAlt;                 // an altitude was reported at all
    float gsKt;                  // ground speed, knots
    float trackDeg;              // true track, degrees
    float baroRateFpm;           // vertical rate, feet per minute
    float seenPosSec;            // age of the position report
    float distKm;                // filled by computeRelative
    float bearingDeg;            // filled by computeRelative
};

struct Snapshot {
    Aircraft ac[MAX_AIRCRAFT];
    int count = 0;
};

// Decodes the JSON body. Aircraft without a position are dropped. Returns false
// when the body is not valid JSON or has no "ac" array, leaving `out` untouched.
bool parse(const char* json, size_t len, Snapshot& out);

// Fills distKm and bearingDeg for every aircraft, relative to the radar centre.
void computeRelative(Snapshot& s, geo::LatLon centre);

// Nearest first. Call after computeRelative.
void sortByDistance(Snapshot& s);

// Advances every aircraft along its own track by dtSec, then recomputes
// distance and bearing. Used to glide between fetches.
void deadReckon(Snapshot& s, geo::LatLon centre, double dtSec);

// How many are within rangeKm. Assumes computeRelative has run.
int countWithin(const Snapshot& s, double rangeKm);

// Index of the aircraft closest to an arbitrary point (not the radar centre),
// or -1 when the snapshot is empty. Writes the distance in km to distKmOut.
// Used to decide what, if anything, is overhead.
int nearestToPoint(const Snapshot& s, geo::LatLon p, float& distKmOut);

}  // namespace adsb
