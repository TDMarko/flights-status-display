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
    bool hasFlight;              // callsign is a real flight number, not a fallback
    char reg[10];                // tail number, empty when unknown
    char type[6];                // ICAO type designator, e.g. "B738"
    char category[4];            // ADS-B emitter category, e.g. "A3"
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
    int overflow = 0;   // contacts the feed reported that did not fit in ac[]
};

// Decodes the JSON body. Aircraft without a position are dropped, as are
// emitter category C contacts: those are surface vehicles and fixed obstacles
// (masts, towers, cranes), which are not traffic and never fly overhead.
// Returns false when the body is not valid JSON or has no "ac" array, leaving
// `out` untouched.
//
// When there are more contacts than MAX_AIRCRAFT, the nearest to `centre` are
// kept and the rest are counted in `overflow`: the feed is not ordered by
// distance, so taking the first ones would drop aircraft right overhead.
// distKm and bearingDeg are filled relative to `centre`.
bool parse(const char* json, size_t len, geo::LatLon centre, Snapshot& out);

// Fills distKm and bearingDeg for every aircraft, relative to the radar centre.
void computeRelative(Snapshot& s, geo::LatLon centre);

// Nearest first. Call after computeRelative.
void sortByDistance(Snapshot& s);

// Advances every aircraft along its own track by dtSec, then recomputes
// distance and bearing. Used to glide between fetches.
void deadReckon(Snapshot& s, geo::LatLon centre, double dtSec);

// Brings freshly parsed positions up to date: each aircraft is advanced by the
// age of its own position report plus `sinceFetchSec`, the time since the
// response arrived. Without it every fetch visibly pulls the traffic back.
void catchUp(Snapshot& s, geo::LatLon centre, double sinceFetchSec);

// How many are within rangeKm. Assumes computeRelative has run.
int countWithin(const Snapshot& s, double rangeKm);

// Index of the aircraft closest to an arbitrary point (not the radar centre),
// or -1 when the snapshot is empty. Writes the distance in km to distKmOut.
// Used to decide what, if anything, is overhead.
int nearestToPoint(const Snapshot& s, geo::LatLon p, float& distKmOut);

}  // namespace adsb
