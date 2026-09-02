#include <unity.h>

#include <stdint.h>   // config.h uses the fixed-width types Arduino.h normally supplies
#include <cstdio>
#include <cstring>

#include "config.h"
#include "geo.h"

void setUp() {}
void tearDown() {}

// Guards against the easiest mistake in a hand-entered coordinate table: a
// swapped lat/lon, a dropped digit, or an airport pasted from the wrong city.
static void test_every_airport_is_plausibly_near_its_city() {
    for (int i = 0; i < CITY_COUNT; i++) {
        const City& c = CITIES[i];
        double d = geo::distanceKm({c.lat, c.lon}, {c.airportLat, c.airportLon});
        char msg[96];
        snprintf(msg, sizeof(msg), "%s -> %s is %.1f km", c.name, c.airport, d);
        TEST_ASSERT_TRUE_MESSAGE(d > 2.0, msg);    // not the city centre itself
        TEST_ASSERT_TRUE_MESSAGE(d < 60.0, msg);   // not another country
    }
}

static void test_coordinates_are_in_the_right_hemisphere() {
    // Every city in the table is northern Europe: positive lat and lon.
    for (int i = 0; i < CITY_COUNT; i++) {
        const City& c = CITIES[i];
        TEST_ASSERT_TRUE(c.lat > 45.0 && c.lat < 71.0);
        TEST_ASSERT_TRUE(c.lon > 10.0 && c.lon < 32.0);
        TEST_ASSERT_TRUE(c.airportLat > 45.0 && c.airportLat < 71.0);
        TEST_ASSERT_TRUE(c.airportLon > 10.0 && c.airportLon < 32.0);
    }
}

static void test_every_city_has_a_name_and_airport_code() {
    for (int i = 0; i < CITY_COUNT; i++) {
        TEST_ASSERT_TRUE(CITIES[i].name && strlen(CITIES[i].name) > 0);
        TEST_ASSERT_TRUE(CITIES[i].airport && strlen(CITIES[i].airport) == 3);
        // Names render at text size 2; anything longer collides with the clock.
        TEST_ASSERT_TRUE(strlen(CITIES[i].name) <= 12);
    }
}

static void test_riga_is_first_so_it_is_the_default() {
    TEST_ASSERT_EQUAL_STRING("RIGA", CITIES[0].name);
    TEST_ASSERT_EQUAL_STRING("RIX", CITIES[0].airport);
}

static void test_rix_sits_west_south_west_of_riga_centre() {
    const City& r = CITIES[0];
    double b = geo::bearingDeg({r.lat, r.lon}, {r.airportLat, r.airportLon});
    TEST_ASSERT_FLOAT_WITHIN(15.0, 0.0, geo::angleDiffDeg(b, 251.0));
}

static void test_default_range_index_is_valid() {
    TEST_ASSERT_TRUE(DEFAULT_RANGE_INDEX >= 0 && DEFAULT_RANGE_INDEX < RANGE_COUNT);
    for (int i = 1; i < RANGE_COUNT; i++) TEST_ASSERT_TRUE(RANGES_KM[i] > RANGES_KM[i - 1]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_every_airport_is_plausibly_near_its_city);
    RUN_TEST(test_coordinates_are_in_the_right_hemisphere);
    RUN_TEST(test_every_city_has_a_name_and_airport_code);
    RUN_TEST(test_riga_is_first_so_it_is_the_default);
    RUN_TEST(test_rix_sits_west_south_west_of_riga_centre);
    RUN_TEST(test_default_range_index_is_valid);
    return UNITY_END();
}
