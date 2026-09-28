#include "adsb.h"

#include <ArduinoJson.h>

#include <cmath>
#include <cstring>

namespace adsb {

namespace {

// Copies at most n-1 bytes and strips the trailing spaces adsb.lol pads
// callsigns with ("RYR9JC  " -> "RYR9JC").
void copyTrimmed(char* dst, size_t n, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    size_t len = strlen(src);
    while (len > 0 && (src[len - 1] == ' ' || src[len - 1] == '\t')) len--;
    if (len > n - 1) len = n - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

float numberOr(JsonVariantConst v, float fallback) {
    return v.is<double>() ? (float)v.as<double>() : fallback;
}

// Only these fields survive deserialization, which keeps a full response well
// inside the ESP32's heap no matter how busy the sky is.
void buildFilter(JsonDocument& filter) {
    JsonObject f = filter["ac"].add<JsonObject>();
    f["hex"] = true;
    f["flight"] = true;
    f["r"] = true;
    f["t"] = true;
    f["category"] = true;
    f["lat"] = true;
    f["lon"] = true;
    f["alt_baro"] = true;
    f["gs"] = true;
    f["track"] = true;
    f["baro_rate"] = true;
    f["seen_pos"] = true;
}

}  // namespace

bool parse(const char* json, size_t len, geo::LatLon centre, Snapshot& out) {
    JsonDocument filter;
    buildFilter(filter);

    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, json, len, DeserializationOption::Filter(filter));
    if (err) return false;

    JsonArrayConst arr = doc["ac"].as<JsonArrayConst>();
    if (arr.isNull()) return false;

    Snapshot parsed;
    for (JsonObjectConst o : arr) {
        JsonVariantConst lat = o["lat"];
        JsonVariantConst lon = o["lon"];
        if (!lat.is<double>() || !lon.is<double>()) continue;  // no position, nothing to draw

        // ADS-B emitter category C is surface vehicles and fixed obstacles:
        // C1/C2 airport ground vehicles, C3 point obstacles such as masts and
        // towers, C4/C5 obstacle groups and lines. None of it is traffic.
        const char* cat = o["category"] | "";
        if (cat[0] == 'C' || cat[0] == 'c') continue;

        Aircraft a;
        copyTrimmed(a.hex, sizeof(a.hex), o["hex"] | "");
        copyTrimmed(a.reg, sizeof(a.reg), o["r"] | "");
        copyTrimmed(a.type, sizeof(a.type), o["t"] | "");
        copyTrimmed(a.category, sizeof(a.category), cat);
        copyTrimmed(a.callsign, sizeof(a.callsign), o["flight"] | "");
        a.hasFlight = a.callsign[0] != '\0';
        if (a.callsign[0] == '\0') copyTrimmed(a.callsign, sizeof(a.callsign), a.reg);
        if (a.callsign[0] == '\0') copyTrimmed(a.callsign, sizeof(a.callsign), a.hex);

        a.lat = lat.as<double>();
        a.lon = lon.as<double>();

        JsonVariantConst alt = o["alt_baro"];
        a.onGround = alt.is<const char*>();  // adsb.lol sends the string "ground"
        a.hasAlt = alt.is<double>();
        a.altFt = a.hasAlt ? (float)alt.as<double>() : 0.0f;

        a.gsKt = numberOr(o["gs"], 0.0f);
        a.trackDeg = numberOr(o["track"], 0.0f);
        a.baroRateFpm = numberOr(o["baro_rate"], 0.0f);
        a.seenPosSec = numberOr(o["seen_pos"], 0.0f);
        a.distKm = (float)geo::distanceKm(centre, {a.lat, a.lon});
        a.bearingDeg = 0.0f;

        if (parsed.count < MAX_AIRCRAFT) {
            parsed.ac[parsed.count++] = a;
            continue;
        }
        // Full: this one replaces the farthest kept, if it is nearer.
        int far = 0;
        for (int i = 1; i < MAX_AIRCRAFT; i++)
            if (parsed.ac[i].distKm > parsed.ac[far].distKm) far = i;
        if (a.distKm < parsed.ac[far].distKm) parsed.ac[far] = a;
        parsed.overflow++;
    }
    computeRelative(parsed, centre);

    out = parsed;
    return true;
}

void computeRelative(Snapshot& s, geo::LatLon centre) {
    for (int i = 0; i < s.count; i++) {
        geo::LatLon p{s.ac[i].lat, s.ac[i].lon};
        s.ac[i].distKm = (float)geo::distanceKm(centre, p);
        // Narrowing to float can round a bearing of 359.9999 up to 360.0,
        // which would break the [0, 360) contract callers rely on.
        float b = (float)geo::bearingDeg(centre, p);
        s.ac[i].bearingDeg = (b >= 360.0f) ? 0.0f : b;
    }
}

void sortByDistance(Snapshot& s) {
    // Insertion sort: n <= 32 and the list is nearly sorted between fetches.
    for (int i = 1; i < s.count; i++) {
        Aircraft key = s.ac[i];
        int j = i - 1;
        while (j >= 0 && s.ac[j].distKm > key.distKm) {
            s.ac[j + 1] = s.ac[j];
            j--;
        }
        s.ac[j + 1] = key;
    }
}

void deadReckon(Snapshot& s, geo::LatLon centre, double dtSec) {
    for (int i = 0; i < s.count; i++) {
        if (s.ac[i].onGround || s.ac[i].gsKt <= 0.0f) continue;
        geo::LatLon moved = geo::advance({s.ac[i].lat, s.ac[i].lon}, s.ac[i].trackDeg,
                                         s.ac[i].gsKt, dtSec);
        s.ac[i].lat = moved.lat;
        s.ac[i].lon = moved.lon;
    }
    computeRelative(s, centre);
}

void catchUp(Snapshot& s, geo::LatLon centre, double sinceFetchSec) {
    for (int i = 0; i < s.count; i++) {
        Aircraft& a = s.ac[i];
        // A report older than this is a coasting track, not a position worth
        // projecting from.
        double age = (a.seenPosSec < 30.0f ? a.seenPosSec : 30.0f) + sinceFetchSec;
        if (!a.onGround && a.gsKt > 0.0f && age > 0.0) {
            geo::LatLon moved = geo::advance({a.lat, a.lon}, a.trackDeg, a.gsKt, age);
            a.lat = moved.lat;
            a.lon = moved.lon;
        }
        a.seenPosSec = 0.0f;
    }
    computeRelative(s, centre);
}

int nearestToPoint(const Snapshot& s, geo::LatLon p, float& distKmOut) {
    int best = -1;
    double bestKm = 0.0;
    for (int i = 0; i < s.count; i++) {
        double d = geo::distanceKm(p, {s.ac[i].lat, s.ac[i].lon});
        if (best < 0 || d < bestKm) { best = i; bestKm = d; }
    }
    distKmOut = (best < 0) ? 0.0f : (float)bestKm;
    return best;
}

int countWithin(const Snapshot& s, double rangeKm) {
    int n = 0;
    for (int i = 0; i < s.count; i++) {
        if (s.ac[i].distKm <= rangeKm) n++;
    }
    return n;
}

}  // namespace adsb
