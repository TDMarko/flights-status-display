#include <unity.h>
#include <cmath>
#include "geo.h"

static const geo::LatLon RIGA{56.9496, 24.1052};

void setUp() {}
void tearDown() {}

static void test_km_per_deg_lon_shrinks_with_latitude() {
    TEST_ASSERT_FLOAT_WITHIN(0.01, 111.320, geo::kmPerDegLon(0.0));
    // cos(56.9496 deg) = 0.545276
    TEST_ASSERT_FLOAT_WITHIN(0.05, 60.70, geo::kmPerDegLon(56.9496));
}

static void test_project_origin_is_zero() {
    geo::Vec2 v = geo::project(RIGA, RIGA);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, v.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, v.y);
}

static void test_project_north_is_positive_y_east_is_positive_x() {
    geo::Vec2 n = geo::project(RIGA, {RIGA.lat + 1.0, RIGA.lon});
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, n.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 110.574, n.y);

    geo::Vec2 e = geo::project(RIGA, {RIGA.lat, RIGA.lon + 1.0});
    TEST_ASSERT_FLOAT_WITHIN(0.05, 60.70, e.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, e.y);
}

static void test_distance_riga_to_vilnius() {
    // True great-circle distance is ~262 km; the flat projection is good to a
    // few km at that range, which is far beyond anything the radar draws.
    double d = geo::distanceKm(RIGA, {54.6872, 25.2797});
    TEST_ASSERT_FLOAT_WITHIN(5.0, 262.0, d);
}

static void test_bearing_cardinals() {
    TEST_ASSERT_FLOAT_WITHIN(0.5, 0.0,   geo::bearingDeg(RIGA, {RIGA.lat + 1.0, RIGA.lon}));
    TEST_ASSERT_FLOAT_WITHIN(0.5, 90.0,  geo::bearingDeg(RIGA, {RIGA.lat, RIGA.lon + 1.0}));
    TEST_ASSERT_FLOAT_WITHIN(0.5, 180.0, geo::bearingDeg(RIGA, {RIGA.lat - 1.0, RIGA.lon}));
    TEST_ASSERT_FLOAT_WITHIN(0.5, 270.0, geo::bearingDeg(RIGA, {RIGA.lat, RIGA.lon - 1.0}));
}

static void test_bearing_is_never_negative() {
    double b = geo::bearingDeg(RIGA, {RIGA.lat + 1.0, RIGA.lon - 1.0});
    TEST_ASSERT_TRUE(b >= 0.0 && b < 360.0);
}

static void test_to_screen_centre_and_edges() {
    geo::ScreenPt c = geo::toScreen({0.0, 0.0}, 50.0, 88, 95, 72);
    TEST_ASSERT_EQUAL_INT(88, c.x);
    TEST_ASSERT_EQUAL_INT(95, c.y);

    // 50 km due north at 50 km range lands on the top of the outer ring.
    geo::ScreenPt n = geo::toScreen({0.0, 50.0}, 50.0, 88, 95, 72);
    TEST_ASSERT_EQUAL_INT(88, n.x);
    TEST_ASSERT_EQUAL_INT(23, n.y);

    // 25 km due east at 50 km range is halfway out to the right.
    geo::ScreenPt e = geo::toScreen({25.0, 0.0}, 50.0, 88, 95, 72);
    TEST_ASSERT_EQUAL_INT(124, e.x);
    TEST_ASSERT_EQUAL_INT(95, e.y);
}

static void test_advance_due_north() {
    // 360 kt for 60 s = 6 NM = 11.112 km north.
    geo::LatLon p = geo::advance(RIGA, 0.0, 360.0, 60.0);
    double north = geo::project(RIGA, p).y;
    TEST_ASSERT_FLOAT_WITHIN(0.05, 11.112, north);
    TEST_ASSERT_FLOAT_WITHIN(0.001, RIGA.lon, p.lon);
}

static void test_advance_due_east() {
    geo::LatLon p = geo::advance(RIGA, 90.0, 360.0, 60.0);
    geo::Vec2 v = geo::project(RIGA, p);
    TEST_ASSERT_FLOAT_WITHIN(0.05, 11.112, v.x);
    TEST_ASSERT_FLOAT_WITHIN(0.05, 0.0, v.y);
}

static void test_advance_zero_speed_is_a_noop() {
    geo::LatLon p = geo::advance(RIGA, 123.0, 0.0, 999.0);
    TEST_ASSERT_FLOAT_WITHIN(1e-9, RIGA.lat, p.lat);
    TEST_ASSERT_FLOAT_WITHIN(1e-9, RIGA.lon, p.lon);
}

static void test_normalize_and_angle_diff_wrap_around_north() {
    TEST_ASSERT_FLOAT_WITHIN(1e-9, 10.0, geo::normalizeDeg(370.0));
    TEST_ASSERT_FLOAT_WITHIN(1e-9, 350.0, geo::normalizeDeg(-10.0));
    TEST_ASSERT_FLOAT_WITHIN(1e-9, 0.0, geo::normalizeDeg(720.0));
    TEST_ASSERT_FLOAT_WITHIN(1e-9, 2.0, geo::angleDiffDeg(359.0, 1.0));
    TEST_ASSERT_FLOAT_WITHIN(1e-9, 180.0, geo::angleDiffDeg(0.0, 180.0));
    TEST_ASSERT_FLOAT_WITHIN(1e-9, 90.0, geo::angleDiffDeg(350.0, 80.0));
}

static void test_bearing_stays_below_360_just_west_of_north() {
    // A hair west of due north must not come back as 360.
    double b = geo::bearingDeg(RIGA, {RIGA.lat + 1.0, RIGA.lon - 1e-12});
    TEST_ASSERT_TRUE(b >= 0.0 && b < 360.0);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, geo::angleDiffDeg(b, 0.0));
}

static void test_unit_conversions() {
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.852, geo::knotsToKmh(1.0));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 304.8, geo::feetToMeters(1000.0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_km_per_deg_lon_shrinks_with_latitude);
    RUN_TEST(test_project_origin_is_zero);
    RUN_TEST(test_project_north_is_positive_y_east_is_positive_x);
    RUN_TEST(test_distance_riga_to_vilnius);
    RUN_TEST(test_bearing_cardinals);
    RUN_TEST(test_bearing_is_never_negative);
    RUN_TEST(test_to_screen_centre_and_edges);
    RUN_TEST(test_advance_due_north);
    RUN_TEST(test_advance_due_east);
    RUN_TEST(test_advance_zero_speed_is_a_noop);
    RUN_TEST(test_normalize_and_angle_diff_wrap_around_north);
    RUN_TEST(test_bearing_stays_below_360_just_west_of_north);
    RUN_TEST(test_unit_conversions);
    return UNITY_END();
}
