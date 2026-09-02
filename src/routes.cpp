#include "routes.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>

namespace routes {

namespace {

struct Cached {
    char callsign[adsb::CALLSIGN_LEN];
    char route[ROUTE_LEN];
    bool used;
    uint32_t seq;   // insertion order, for evicting the oldest entry
};

Cached gCache[MAX_ROUTES];
uint32_t gSeq = 0;

Cached* find(const char* callsign) {
    for (int i = 0; i < MAX_ROUTES; i++) {
        if (gCache[i].used && strcmp(gCache[i].callsign, callsign) == 0) return &gCache[i];
    }
    return nullptr;
}

}  // namespace

bool parseRouteBody(const char* json, size_t len, char* out, size_t outLen) {
    if (!json || len == 0 || outLen == 0) return false;

    JsonDocument filter;
    filter["_airport_codes_iata"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter))) return false;

    const char* pair = doc["_airport_codes_iata"] | (const char*)nullptr;
    if (!pair || !pair[0]) return false;

    // "BRU-RIX" -> "BRU>RIX". The arrow reads directionally and costs no width.
    // Anything without a separator (a single airport, or "unknown") is not a route.
    const char* dash = strchr(pair, '-');
    if (!dash || dash == pair || !dash[1]) return false;

    size_t n = strlen(pair);
    if (n >= outLen) return false;   // multi-leg routes are longer than the panel allows
    for (size_t i = 0; i < n; i++) out[i] = (pair[i] == '-') ? '>' : pair[i];
    out[n] = '\0';
    return true;
}

void clear() {
    memset(gCache, 0, sizeof(gCache));
    gSeq = 0;
}

const char* lookup(const char* callsign) {
    if (!callsign || !callsign[0]) return nullptr;
    Cached* c = find(callsign);
    return c ? c->route : nullptr;
}

void store(const char* callsign, const char* route) {
    if (!callsign || !callsign[0]) return;

    Cached* c = find(callsign);
    if (!c) {
        // Reuse a free slot, else the oldest entry.
        for (int i = 0; i < MAX_ROUTES; i++) {
            if (!gCache[i].used) { c = &gCache[i]; break; }
            if (!c || gCache[i].seq < c->seq) c = &gCache[i];
        }
        memset(c, 0, sizeof(*c));
        snprintf(c->callsign, sizeof(c->callsign), "%s", callsign);
        c->used = true;
        c->seq = ++gSeq;
    }
    snprintf(c->route, sizeof(c->route), "%s", route ? route : "");
}

const char* nextPending(const adsb::Snapshot& s, int maxConsider) {
    int limit = s.count < maxConsider ? s.count : maxConsider;
    for (int i = 0; i < limit; i++) {
        const char* cs = s.ac[i].callsign;
        if (!cs[0]) continue;
        if (!find(cs)) return cs;
    }
    return nullptr;
}

int cachedCount() {
    int n = 0;
    for (int i = 0; i < MAX_ROUTES; i++)
        if (gCache[i].used) n++;
    return n;
}

}  // namespace routes
