#pragma once
// Origin/destination lookup, cached.
//
// The aircraft feed carries no route, but adsb.lol answers
// GET /api/0/route/<callsign> with the airport pair. A route does not change
// during a flight, so each callsign is looked up once and remembered; a busy
// sky costs one extra request per newly seen aircraft, not one per refresh.
// Arduino-free apart from the HTTP call, which the caller makes.

#include <stddef.h>
#include <stdint.h>

#include "adsb.h"

namespace routes {

constexpr int MAX_ROUTES = 48;
constexpr int ROUTE_LEN = 12;   // "BRU>RIX", or a three-leg "CAN>CKG>AMS"

// Pulls "BRU>RIX" out of a route response body, or "CAN>CKG>AMS" for a
// multi-leg service. Returns false for anything that is not a route document —
// the host answers unknown callsigns with an HTML page, so this must reject
// non-JSON without complaining — and for routes too long to show.
bool parseRouteBody(const char* json, size_t len, char* out, size_t outLen);

void clear();

// The cached route for a callsign: the string when known, "" when the API had
// no route for it, and nullptr when it has not been looked up yet.
const char* lookup(const char* callsign);

// Remembers a result. Pass "" to record "the API has no route for this".
void store(const char* callsign, const char* route);

// The nearest aircraft in `s` that has no cache entry yet, or nullptr when
// they all have one. Only the first `maxConsider` are examined, because only
// those are ever shown in the side panel.
const char* nextPending(const adsb::Snapshot& s, int maxConsider);

int cachedCount();

}  // namespace routes
