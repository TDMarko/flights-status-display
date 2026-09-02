#include "settings.h"

#include <Preferences.h>

#include "config.h"

namespace settings {

namespace {
Preferences prefs;
int gCity = 0;
int gRange = DEFAULT_RANGE_INDEX;

int clamp(int v, int count, int fallback) {
    return (v >= 0 && v < count) ? v : fallback;
}
}  // namespace

void begin() {
    prefs.begin("flydar", false);
    gCity = clamp(prefs.getInt("city", 0), CITY_COUNT, 0);
    gRange = clamp(prefs.getInt("range", DEFAULT_RANGE_INDEX), RANGE_COUNT, DEFAULT_RANGE_INDEX);
}

int cityIndex() { return gCity; }
int rangeIndex() { return gRange; }

void nextCity() {
    gCity = (gCity + 1) % CITY_COUNT;
    prefs.putInt("city", gCity);
}

void nextRange() {
    gRange = (gRange + 1) % RANGE_COUNT;
    prefs.putInt("range", gRange);
}

}  // namespace settings
