#pragma once
// Airport weather from a METAR, for the header strip.
//
// aviationweather.gov (NOAA) serves a decoded METAR as JSON, free and without
// a key. Reports are issued about every half hour, so this is polled rarely.
// Arduino-free apart from the HTTP call, which the caller makes.

#include <stddef.h>

namespace weather {

struct Report {
    bool valid = false;
    bool windVariable = false;  // METAR reported VRB rather than a direction
    bool hasWind = false;
    bool hasTemp = false;
    int windDirDeg = 0;
    int windKt = 0;
    int tempC = 0;
    char cover[6] = {0};        // "FEW", "SCT", "BKN", "OVC", "CLR", ...
};

// Decodes a NOAA METAR JSON array. Returns false when the body is not a report
// (an empty array for an airport with no observation, or anything malformed).
bool parse(const char* json, size_t len, Report& out);

// 210 -> "SSW". Sixteen points, which is as fine as a header line deserves.
const char* compass(int degrees);

// "RIX SSW 4kt 19C BKN" — the airport's IATA code, then wind, temperature and
// cloud cover, skipping anything the report did not carry.
void format(const Report& r, const char* iata, char* out, size_t n);

}  // namespace weather
