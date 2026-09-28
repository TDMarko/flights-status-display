#include <unity.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "adsb.h"

static const geo::LatLon RIGA{56.9496, 24.1052};

void setUp() {}
void tearDown() {}

static std::string loadFixture(const char* name) {
    std::string path = std::string(FIXTURE_DIR) + "/" + name;
    FILE* f = fopen(path.c_str(), "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path.c_str());
    std::string out;
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    fclose(f);
    return out;
}

static void test_parses_five_aircraft_from_a_real_response() {
    std::string body = loadFixture("riga_5ac.json");
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body.c_str(), body.size(), RIGA, s));
    TEST_ASSERT_EQUAL_INT(5, s.count);
}

static void test_strips_the_padding_adsb_lol_puts_after_callsigns() {
    std::string body = loadFixture("riga_1ac.json");
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body.c_str(), body.size(), RIGA, s));
    TEST_ASSERT_EQUAL_INT(1, s.count);
    TEST_ASSERT_EQUAL_STRING("PYR013", s.ac[0].callsign);
    TEST_ASSERT_EQUAL_STRING("SU-PAE", s.ac[0].reg);
    TEST_ASSERT_EQUAL_STRING("A321", s.ac[0].type);
    TEST_ASSERT_EQUAL_STRING("010285", s.ac[0].hex);
}

static void test_keeps_position_and_kinematics() {
    std::string body = loadFixture("riga_1ac.json");
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body.c_str(), body.size(), RIGA, s));
    const adsb::Aircraft& a = s.ac[0];
    TEST_ASSERT_FLOAT_WITHIN(1e-4, 56.779404, a.lat);
    TEST_ASSERT_FLOAT_WITHIN(1e-4, 24.364414, a.lon);
    TEST_ASSERT_TRUE(a.hasAlt);
    TEST_ASSERT_FALSE(a.onGround);
    TEST_ASSERT_FLOAT_WITHIN(0.5, 32000.0, a.altFt);
    TEST_ASSERT_FLOAT_WITHIN(0.5, 435.4, a.gsKt);
    TEST_ASSERT_FLOAT_WITHIN(0.5, 262.88, a.trackDeg);
    TEST_ASSERT_FLOAT_WITHIN(0.5, 0.0, a.baroRateFpm);
}

static void test_compute_relative_then_sort_gives_nearest_first() {
    std::string body = loadFixture("riga_5ac.json");
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body.c_str(), body.size(), RIGA, s));
    adsb::computeRelative(s, RIGA);
    adsb::sortByDistance(s);
    for (int i = 1; i < s.count; i++) {
        TEST_ASSERT_TRUE(s.ac[i - 1].distKm <= s.ac[i].distKm);
    }
    // PYR013 sits south-east of the city centre and is the closest of the five.
    TEST_ASSERT_EQUAL_STRING("PYR013", s.ac[0].callsign);
    TEST_ASSERT_FLOAT_WITHIN(1.5, 24.5, s.ac[0].distKm);
    TEST_ASSERT_TRUE(s.ac[0].bearingDeg > 120.0 && s.ac[0].bearingDeg < 160.0);
}

static void test_count_within_range() {
    std::string body = loadFixture("riga_5ac.json");
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body.c_str(), body.size(), RIGA, s));
    adsb::computeRelative(s, RIGA);
    TEST_ASSERT_EQUAL_INT(5, adsb::countWithin(s, 200.0));
    TEST_ASSERT_EQUAL_INT(0, adsb::countWithin(s, 5.0));
    TEST_ASSERT_EQUAL_INT(1, adsb::countWithin(s, 50.0));
}

static void test_aircraft_without_a_position_are_dropped() {
    const char* body =
        "{\"ac\":[{\"hex\":\"aaa111\",\"flight\":\"NOPOS\"},"
        "{\"hex\":\"bbb222\",\"flight\":\"HASPOS\",\"lat\":57.0,\"lon\":24.0}]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));
    TEST_ASSERT_EQUAL_INT(1, s.count);
    TEST_ASSERT_EQUAL_STRING("HASPOS", s.ac[0].callsign);
}

static void test_ground_altitude_is_not_treated_as_a_number() {
    const char* body =
        "{\"ac\":[{\"hex\":\"ccc333\",\"flight\":\"TAXI1\",\"lat\":56.92,"
        "\"lon\":23.97,\"alt_baro\":\"ground\",\"gs\":12.0,\"track\":90.0}]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));
    TEST_ASSERT_EQUAL_INT(1, s.count);
    TEST_ASSERT_TRUE(s.ac[0].onGround);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, s.ac[0].altFt);
}

static void test_callsign_falls_back_to_registration_then_hex() {
    const char* body =
        "{\"ac\":[{\"hex\":\"ddd444\",\"r\":\"YL-ABC\",\"lat\":57.0,\"lon\":24.0},"
        "{\"hex\":\"eee555\",\"lat\":57.1,\"lon\":24.1}]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));
    TEST_ASSERT_EQUAL_INT(2, s.count);
    TEST_ASSERT_EQUAL_STRING("YL-ABC", s.ac[0].callsign);
    TEST_ASSERT_EQUAL_STRING("eee555", s.ac[1].callsign);
}

static void test_more_aircraft_than_capacity_keeps_the_nearest() {
    // Listed farthest first, the way an unordered feed can arrive: keeping
    // the first MAX_AIRCRAFT would throw away everything close to the centre.
    const int total = adsb::MAX_AIRCRAFT + 12;
    std::string body = "{\"ac\":[";
    for (int i = 0; i < total; i++) {
        char one[160];
        snprintf(one, sizeof(one),
                 "%s{\"hex\":\"f%05d\",\"flight\":\"T%03d\",\"lat\":%f,\"lon\":24.1052}",
                 i ? "," : "", i, i, 56.9496 + (total - i) * 0.01);
        body += one;
    }
    body += "]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body.c_str(), body.size(), RIGA, s));
    TEST_ASSERT_EQUAL_INT(adsb::MAX_AIRCRAFT, s.count);
    TEST_ASSERT_EQUAL_INT(12, s.overflow);

    // The 12 dropped are the 12 farthest, T000..T011.
    for (int i = 0; i < s.count; i++) {
        int n = atoi(s.ac[i].callsign + 1);
        TEST_ASSERT_TRUE_MESSAGE(n >= 12, s.ac[i].callsign);
    }
}

static void test_parse_fills_distance_and_bearing() {
    std::string body = loadFixture("riga_1ac.json");
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body.c_str(), body.size(), RIGA, s));
    TEST_ASSERT_FLOAT_WITHIN(1.5, 24.5, s.ac[0].distKm);
    TEST_ASSERT_TRUE(s.ac[0].bearingDeg > 120.0 && s.ac[0].bearingDeg < 160.0);
    TEST_ASSERT_EQUAL_INT(0, s.overflow);
}

static void test_only_a_real_flight_number_counts_as_a_flight() {
    const char* body =
        "{\"ac\":[{\"hex\":\"ddd444\",\"flight\":\"BTI1PA  \",\"lat\":57.0,\"lon\":24.0},"
        "{\"hex\":\"eee555\",\"r\":\"YL-ABC\",\"lat\":57.1,\"lon\":24.1}]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));
    TEST_ASSERT_TRUE(s.ac[0].hasFlight);
    TEST_ASSERT_FALSE(s.ac[1].hasFlight);   // "YL-ABC" is a registration
}

static void test_catch_up_advances_by_each_reports_own_age() {
    // Both head north at 360 kt (0.1852 km/s); one report is 10 s old, one fresh.
    const char* body =
        "{\"ac\":[{\"hex\":\"a00001\",\"flight\":\"OLD\",\"lat\":56.9496,\"lon\":24.1052,"
        "\"gs\":360.0,\"track\":0.0,\"alt_baro\":10000,\"seen_pos\":10.0},"
        "{\"hex\":\"a00002\",\"flight\":\"NEW\",\"lat\":56.9496,\"lon\":24.1052,"
        "\"gs\":360.0,\"track\":0.0,\"alt_baro\":10000,\"seen_pos\":0.0}]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));
    adsb::catchUp(s, RIGA, 2.0);
    TEST_ASSERT_FLOAT_WITHIN(0.02, 2.222, s.ac[0].distKm);  // 10 s + 2 s
    TEST_ASSERT_FLOAT_WITHIN(0.02, 0.370, s.ac[1].distKm);  // 2 s
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, s.ac[0].seenPosSec);

    // A second call only adds its own interval: the report age is spent.
    adsb::catchUp(s, RIGA, 0.0);
    TEST_ASSERT_FLOAT_WITHIN(0.02, 2.222, s.ac[0].distKm);
}

static void test_catch_up_does_not_extrapolate_a_coasting_track() {
    const char* body =
        "{\"ac\":[{\"hex\":\"a00003\",\"flight\":\"GHOST\",\"lat\":56.9496,\"lon\":24.1052,"
        "\"gs\":360.0,\"track\":0.0,\"alt_baro\":10000,\"seen_pos\":300.0}]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));
    adsb::catchUp(s, RIGA, 0.0);
    TEST_ASSERT_FLOAT_WITHIN(0.02, 5.556, s.ac[0].distKm);  // capped at 30 s
}

static void test_malformed_json_is_rejected() {
    adsb::Snapshot s;
    s.count = 7;
    const char* junk = "{\"ac\":[{\"hex\"";
    TEST_ASSERT_FALSE(adsb::parse(junk, strlen(junk), RIGA, s));
    TEST_ASSERT_EQUAL_INT(7, s.count);  // caller's previous snapshot is left alone
}

static void test_response_without_ac_array_is_rejected() {
    adsb::Snapshot s;
    const char* body = "{\"msg\":\"No error\",\"total\":0}";
    TEST_ASSERT_FALSE(adsb::parse(body, strlen(body), RIGA, s));
}

static void test_empty_ac_array_parses_to_zero_aircraft() {
    adsb::Snapshot s;
    const char* body = "{\"ac\":[],\"total\":0}";
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));
    TEST_ASSERT_EQUAL_INT(0, s.count);
}

static void test_dead_reckon_moves_aircraft_along_its_track() {
    const char* body =
        "{\"ac\":[{\"hex\":\"aaa000\",\"flight\":\"NORTH1\",\"lat\":56.9496,"
        "\"lon\":24.1052,\"gs\":360.0,\"track\":0.0,\"alt_baro\":10000}]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));
    adsb::computeRelative(s, RIGA);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, s.ac[0].distKm);

    adsb::deadReckon(s, RIGA, 60.0);  // 360 kt for a minute = 11.112 km
    TEST_ASSERT_FLOAT_WITHIN(0.05, 11.112, s.ac[0].distKm);
    // Compare on the circle: due north is equally 0 and 359.999.
    TEST_ASSERT_FLOAT_WITHIN(0.5, 0.0, geo::angleDiffDeg(s.ac[0].bearingDeg, 0.0));
    TEST_ASSERT_TRUE(s.ac[0].bearingDeg >= 0.0f && s.ac[0].bearingDeg < 360.0f);
}

static void test_dead_reckon_leaves_stationary_aircraft_alone() {
    const char* body =
        "{\"ac\":[{\"hex\":\"bbb000\",\"flight\":\"PARKED\",\"lat\":56.99,"
        "\"lon\":24.20,\"alt_baro\":\"ground\"}]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));
    adsb::computeRelative(s, RIGA);
    float before = s.ac[0].distKm;
    adsb::deadReckon(s, RIGA, 300.0);
    TEST_ASSERT_FLOAT_WITHIN(1e-4, before, s.ac[0].distKm);
}

// A round, obviously synthetic point north-east of the city centre. Never use a
// real address here: test fixtures end up in the public history.
static const geo::LatLon TEST_HOME{57.0000, 24.2000};

static void test_ground_obstacles_and_vehicles_are_not_traffic() {
    // Emitter category C is surface vehicles and fixed obstacles. A mast that
    // never moves has no business in a list of aircraft overhead.
    const char* body =
        "{\"ac\":[{\"hex\":\"c00001\",\"flight\":\"TOWER1\",\"category\":\"C3\","
        "\"lat\":56.95,\"lon\":24.10},"
        "{\"hex\":\"c00002\",\"flight\":\"SWEEPER\",\"category\":\"C2\","
        "\"lat\":56.92,\"lon\":23.97},"
        "{\"hex\":\"c00003\",\"flight\":\"LINEOBS\",\"category\":\"C5\","
        "\"lat\":56.93,\"lon\":24.00},"
        "{\"hex\":\"a00001\",\"flight\":\"REALJET\",\"category\":\"A3\","
        "\"lat\":56.96,\"lon\":24.12}]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));
    TEST_ASSERT_EQUAL_INT(1, s.count);
    TEST_ASSERT_EQUAL_STRING("REALJET", s.ac[0].callsign);
    TEST_ASSERT_EQUAL_STRING("A3", s.ac[0].category);
}

static void test_things_that_actually_fly_are_kept() {
    // Category B is gliders, balloons, parachutists, ultralights and drones.
    // They fly, so they belong on the radar; and plenty of contacts carry no
    // category at all, which must not exclude them either.
    const char* body =
        "{\"ac\":[{\"hex\":\"b00001\",\"flight\":\"GLIDER1\",\"category\":\"B1\","
        "\"lat\":56.95,\"lon\":24.10},"
        "{\"hex\":\"b00004\",\"flight\":\"DRONE1\",\"category\":\"B6\","
        "\"lat\":56.96,\"lon\":24.11},"
        "{\"hex\":\"n00001\",\"flight\":\"NOCAT\",\"lat\":56.97,\"lon\":24.13}]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));
    TEST_ASSERT_EQUAL_INT(3, s.count);
}

static void test_nearest_to_point_finds_what_is_overhead() {
    // Two aircraft: one right over the test home, one well away from it.
    const char* body =
        "{\"ac\":[{\"hex\":\"aaa111\",\"flight\":\"FARAWAY\",\"lat\":57.30,\"lon\":24.60},"
        "{\"hex\":\"bbb222\",\"flight\":\"ABOVEME\",\"lat\":57.0001,\"lon\":24.2001}]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));

    geo::LatLon home = TEST_HOME;
    float km = -1.0f;
    int i = adsb::nearestToPoint(s, home, km);
    TEST_ASSERT_EQUAL_INT(1, i);
    TEST_ASSERT_EQUAL_STRING("ABOVEME", s.ac[i].callsign);
    TEST_ASSERT_TRUE(km < 0.1f);
}

static void test_nearest_to_point_is_measured_from_home_not_the_radar_centre() {
    // This aircraft is nearer Riga city centre than it is to home, so a
    // centre-based search would pick the wrong frame of reference.
    const char* body =
        "{\"ac\":[{\"hex\":\"ccc333\",\"flight\":\"OVERCITY\",\"lat\":56.9496,\"lon\":24.1052}]}";
    adsb::Snapshot s;
    TEST_ASSERT_TRUE(adsb::parse(body, strlen(body), RIGA, s));

    float km = -1.0f;
    adsb::nearestToPoint(s, TEST_HOME, km);
    TEST_ASSERT_FLOAT_WITHIN(0.5, 8.0, km);   // the test home sits ~8 km from the centre
}

static void test_nearest_to_point_on_an_empty_sky() {
    adsb::Snapshot s;
    float km = -1.0f;
    TEST_ASSERT_EQUAL_INT(-1, adsb::nearestToPoint(s, TEST_HOME, km));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, km);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_five_aircraft_from_a_real_response);
    RUN_TEST(test_strips_the_padding_adsb_lol_puts_after_callsigns);
    RUN_TEST(test_keeps_position_and_kinematics);
    RUN_TEST(test_compute_relative_then_sort_gives_nearest_first);
    RUN_TEST(test_count_within_range);
    RUN_TEST(test_aircraft_without_a_position_are_dropped);
    RUN_TEST(test_ground_altitude_is_not_treated_as_a_number);
    RUN_TEST(test_callsign_falls_back_to_registration_then_hex);
    RUN_TEST(test_more_aircraft_than_capacity_keeps_the_nearest);
    RUN_TEST(test_parse_fills_distance_and_bearing);
    RUN_TEST(test_only_a_real_flight_number_counts_as_a_flight);
    RUN_TEST(test_catch_up_advances_by_each_reports_own_age);
    RUN_TEST(test_catch_up_does_not_extrapolate_a_coasting_track);
    RUN_TEST(test_malformed_json_is_rejected);
    RUN_TEST(test_response_without_ac_array_is_rejected);
    RUN_TEST(test_empty_ac_array_parses_to_zero_aircraft);
    RUN_TEST(test_dead_reckon_moves_aircraft_along_its_track);
    RUN_TEST(test_dead_reckon_leaves_stationary_aircraft_alone);
    RUN_TEST(test_ground_obstacles_and_vehicles_are_not_traffic);
    RUN_TEST(test_things_that_actually_fly_are_kept);
    RUN_TEST(test_nearest_to_point_finds_what_is_overhead);
    RUN_TEST(test_nearest_to_point_is_measured_from_home_not_the_radar_centre);
    RUN_TEST(test_nearest_to_point_on_an_empty_sky);
    return UNITY_END();
}
