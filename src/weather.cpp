#include "weather.h"

#include <ArduinoJson.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace weather {

namespace {
const char* POINTS[16] = {"N",  "NNE", "NE", "ENE", "E",  "ESE", "SE", "SSE",
                          "S",  "SSW", "SW", "WSW", "W",  "WNW", "NW", "NNW"};
}  // namespace

const char* compass(int degrees) {
    int d = degrees % 360;
    if (d < 0) d += 360;
    // Each point spans 22.5 degrees, centred on its bearing.
    return POINTS[(int)((d + 11.25) / 22.5) % 16];
}

bool parse(const char* json, size_t len, Report& out) {
    if (!json || len == 0) return false;

    JsonDocument filter;
    JsonObject f = filter.add<JsonObject>();
    f["wdir"] = true;
    f["wspd"] = true;
    f["temp"] = true;
    f["cover"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter))) return false;

    JsonArrayConst arr = doc.as<JsonArrayConst>();
    if (arr.isNull() || arr.size() == 0) return false;
    JsonObjectConst o = arr[0];
    if (o.isNull()) return false;

    Report r;

    JsonVariantConst wdir = o["wdir"];
    if (wdir.is<const char*>()) {
        // NOAA sends the string "VRB" when the wind has no settled direction.
        r.windVariable = true;
        r.hasWind = true;
    } else if (wdir.is<double>()) {
        r.windDirDeg = (int)lround(wdir.as<double>());
        r.hasWind = true;
    }

    JsonVariantConst wspd = o["wspd"];
    if (wspd.is<double>()) r.windKt = (int)lround(wspd.as<double>());
    else r.hasWind = false;   // a direction without a speed is not worth showing

    JsonVariantConst temp = o["temp"];
    if (temp.is<double>()) {
        r.tempC = (int)lround(temp.as<double>());
        r.hasTemp = true;
    }

    const char* cover = o["cover"] | "";
    snprintf(r.cover, sizeof(r.cover), "%s", cover);

    // A report carrying none of the three is not worth calling valid.
    if (!r.hasWind && !r.hasTemp && !r.cover[0]) return false;
    r.valid = true;
    out = r;
    return true;
}

void format(const Report& r, const char* iata, char* out, size_t n) {
    if (!r.valid) { out[0] = '\0'; return; }

    char wind[16] = "";
    if (r.hasWind) {
        if (r.windVariable) snprintf(wind, sizeof(wind), "VRB %dkt", r.windKt);
        else if (r.windKt == 0) snprintf(wind, sizeof(wind), "CALM");
        else snprintf(wind, sizeof(wind), "%s %dkt", compass(r.windDirDeg), r.windKt);
    }
    char temp[10] = "";
    if (r.hasTemp) snprintf(temp, sizeof(temp), "%dC", r.tempC);

    snprintf(out, n, "%s%s%s%s%s%s%s", iata ? iata : "",
             wind[0] ? " " : "", wind,
             temp[0] ? " " : "", temp,
             r.cover[0] ? " " : "", r.cover);
}

}  // namespace weather
