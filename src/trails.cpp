#include "trails.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace trails {

namespace {

struct Trail {
    char hex[8];
    Point pts[TRAIL_POINTS];
    uint8_t count;          // how many of pts are valid, up to TRAIL_POINTS
    uint8_t head;           // next slot to write
    uint32_t lastSeenMs;
    uint32_t lastSampleMs;
    bool used;
};

Trail gTrails[MAX_TRAILS];

Trail* find(const char* hex) {
    for (int i = 0; i < MAX_TRAILS; i++) {
        if (gTrails[i].used && strcmp(gTrails[i].hex, hex) == 0) return &gTrails[i];
    }
    return nullptr;
}

// Reuses the least recently seen slot once the table is full, so a busy sky
// costs the oldest trail rather than dropping the newest aircraft.
Trail* claim(const char* hex, uint32_t nowMs) {
    Trail* oldest = nullptr;
    for (int i = 0; i < MAX_TRAILS; i++) {
        if (!gTrails[i].used) { oldest = &gTrails[i]; break; }
        if (!oldest || gTrails[i].lastSeenMs < oldest->lastSeenMs) oldest = &gTrails[i];
    }
    memset(oldest, 0, sizeof(*oldest));
    snprintf(oldest->hex, sizeof(oldest->hex), "%s", hex);
    oldest->used = true;
    oldest->lastSeenMs = nowMs;
    oldest->lastSampleMs = nowMs;
    return oldest;
}

}  // namespace

void clear() { memset(gTrails, 0, sizeof(gTrails)); }

void record(const adsb::Snapshot& s, uint32_t nowMs, uint32_t sampleMs) {
    for (int i = 0; i < s.count; i++) {
        const adsb::Aircraft& a = s.ac[i];
        if (a.hex[0] == '\0') continue;

        Trail* t = find(a.hex);
        if (!t) t = claim(a.hex, nowMs);
        t->lastSeenMs = nowMs;

        // An aircraft's first point is always taken; after that the sampling
        // interval applies. Unsigned arithmetic, so a millis() wrap is fine.
        if (t->count > 0 && (uint32_t)(nowMs - t->lastSampleMs) < sampleMs) continue;
        t->lastSampleMs = nowMs;

        double rad = a.bearingDeg * 3.14159265358979323846 / 180.0;
        t->pts[t->head] = {(float)(a.distKm * sin(rad)), (float)(a.distKm * cos(rad))};
        t->head = (uint8_t)((t->head + 1) % TRAIL_POINTS);
        if (t->count < TRAIL_POINTS) t->count++;
    }
}

int fetch(const char* hex, Point* out, int maxOut) {
    Trail* t = find(hex);
    if (!t || maxOut <= 0) return 0;

    int n = t->count < maxOut ? t->count : maxOut;
    // head points at the next write slot, so the oldest kept point is head-count.
    int start = (t->head - t->count + TRAIL_POINTS * 2) % TRAIL_POINTS;
    int skip = t->count - n;  // if the caller's buffer is small, give the newest
    for (int i = 0; i < n; i++) {
        out[i] = t->pts[(start + skip + i) % TRAIL_POINTS];
    }
    return n;
}

void expire(uint32_t nowMs, uint32_t maxAgeMs) {
    for (int i = 0; i < MAX_TRAILS; i++) {
        if (!gTrails[i].used) continue;
        if ((uint32_t)(nowMs - gTrails[i].lastSeenMs) > maxAgeMs) {
            memset(&gTrails[i], 0, sizeof(gTrails[i]));
        }
    }
}

int activeCount() {
    int n = 0;
    for (int i = 0; i < MAX_TRAILS; i++)
        if (gTrails[i].used) n++;
    return n;
}

}  // namespace trails
