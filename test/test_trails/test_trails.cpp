#include <unity.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "trails.h"

static const geo::LatLon RIGA{56.9496, 24.1052};

void setUp() { trails::clear(); }
void tearDown() {}

// Builds a snapshot of one aircraft at a given offset from the centre.
static adsb::Snapshot oneAt(const char* hex, double eastKm, double northKm) {
    adsb::Snapshot s;
    s.count = 1;
    adsb::Aircraft& a = s.ac[0];
    memset(&a, 0, sizeof(a));
    snprintf(a.hex, sizeof(a.hex), "%s", hex);
    a.lat = RIGA.lat + northKm / geo::KM_PER_DEG_LAT;
    a.lon = RIGA.lon + eastKm / geo::kmPerDegLon(RIGA.lat);
    adsb::computeRelative(s, RIGA);
    return s;
}

static void test_clear_leaves_no_trails() {
    trails::record(oneAt("abc123", 10, 0), 1000, 100);
    TEST_ASSERT_EQUAL_INT(1, trails::activeCount());
    trails::clear();
    TEST_ASSERT_EQUAL_INT(0, trails::activeCount());
}

static void test_records_a_point_at_the_aircraft_position() {
    trails::record(oneAt("abc123", 10.0, -5.0), 1000, 100);
    trails::Point pts[trails::TRAIL_POINTS];
    int n = trails::fetch("abc123", pts, trails::TRAIL_POINTS);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_FLOAT_WITHIN(0.05, 10.0, pts[0].x);
    TEST_ASSERT_FLOAT_WITHIN(0.05, -5.0, pts[0].y);
}

static void test_unknown_aircraft_has_no_trail() {
    trails::Point pts[4];
    TEST_ASSERT_EQUAL_INT(0, trails::fetch("nosuch", pts, 4));
}

static void test_sampling_interval_is_respected() {
    trails::record(oneAt("abc123", 0, 0), 1000, 5000);
    trails::record(oneAt("abc123", 1, 0), 2000, 5000);   // too soon
    trails::record(oneAt("abc123", 2, 0), 3000, 5000);   // still too soon
    trails::Point pts[trails::TRAIL_POINTS];
    TEST_ASSERT_EQUAL_INT(1, trails::fetch("abc123", pts, trails::TRAIL_POINTS));

    trails::record(oneAt("abc123", 3, 0), 6001, 5000);   // now due
    TEST_ASSERT_EQUAL_INT(2, trails::fetch("abc123", pts, trails::TRAIL_POINTS));
}

static void test_points_come_back_oldest_first() {
    for (int i = 0; i < 5; i++) trails::record(oneAt("abc123", i * 2.0, 0), 1000 + i * 5000, 4000);
    trails::Point pts[trails::TRAIL_POINTS];
    int n = trails::fetch("abc123", pts, trails::TRAIL_POINTS);
    TEST_ASSERT_EQUAL_INT(5, n);
    for (int i = 0; i < n; i++) TEST_ASSERT_FLOAT_WITHIN(0.05, i * 2.0, pts[i].x);
}

static void test_ring_buffer_keeps_the_newest_points() {
    const int extra = 6;
    for (int i = 0; i < trails::TRAIL_POINTS + extra; i++)
        trails::record(oneAt("abc123", i * 1.0, 0), 1000 + i * 5000, 4000);

    trails::Point pts[trails::TRAIL_POINTS];
    int n = trails::fetch("abc123", pts, trails::TRAIL_POINTS);
    TEST_ASSERT_EQUAL_INT(trails::TRAIL_POINTS, n);
    // The oldest `extra` points fell off the back.
    TEST_ASSERT_FLOAT_WITHIN(0.05, (double)extra, pts[0].x);
    TEST_ASSERT_FLOAT_WITHIN(0.05, (double)(trails::TRAIL_POINTS + extra - 1), pts[n - 1].x);
}

static void test_small_output_buffer_gets_the_newest_points() {
    for (int i = 0; i < 10; i++) trails::record(oneAt("abc123", i * 1.0, 0), 1000 + i * 5000, 4000);
    trails::Point pts[3];
    int n = trails::fetch("abc123", pts, 3);
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_FLOAT_WITHIN(0.05, 7.0, pts[0].x);
    TEST_ASSERT_FLOAT_WITHIN(0.05, 9.0, pts[2].x);
}

static void test_trail_survives_a_replaced_snapshot() {
    // Each fetch builds a brand new Snapshot; the trail is keyed by hex, not by
    // position in the array, so it must carry over.
    trails::record(oneAt("abc123", 0, 0), 1000, 4000);
    adsb::Snapshot replacement = oneAt("abc123", 8, 0);
    trails::record(replacement, 6000, 4000);
    trails::Point pts[trails::TRAIL_POINTS];
    TEST_ASSERT_EQUAL_INT(2, trails::fetch("abc123", pts, trails::TRAIL_POINTS));
}

static void test_expire_drops_only_the_stale() {
    trails::record(oneAt("old111", 5, 0), 1000, 100);
    trails::record(oneAt("new222", 5, 0), 50000, 100);
    TEST_ASSERT_EQUAL_INT(2, trails::activeCount());

    trails::expire(60000, 30000);   // old111 last seen 59 s ago, new222 10 s ago
    TEST_ASSERT_EQUAL_INT(1, trails::activeCount());
    trails::Point pts[4];
    TEST_ASSERT_EQUAL_INT(0, trails::fetch("old111", pts, 4));
    TEST_ASSERT_EQUAL_INT(1, trails::fetch("new222", pts, 4));
}

static void test_a_full_table_evicts_the_least_recently_seen() {
    char hex[8];
    for (int i = 0; i < trails::MAX_TRAILS; i++) {
        snprintf(hex, sizeof(hex), "a%05d", i);
        adsb::Snapshot s = oneAt(hex, 5, 0);
        trails::record(s, 1000 + i, 100);
    }
    TEST_ASSERT_EQUAL_INT(trails::MAX_TRAILS, trails::activeCount());

    trails::record(oneAt("ffffff", 5, 0), 99000, 100);
    TEST_ASSERT_EQUAL_INT(trails::MAX_TRAILS, trails::activeCount());

    trails::Point pts[4];
    TEST_ASSERT_EQUAL_INT(1, trails::fetch("ffffff", pts, 4));   // newcomer is in
    TEST_ASSERT_EQUAL_INT(0, trails::fetch("a00000", pts, 4));   // oldest was evicted
}

static void test_aircraft_without_a_hex_is_ignored() {
    adsb::Snapshot s = oneAt("", 5, 0);
    trails::record(s, 1000, 100);
    TEST_ASSERT_EQUAL_INT(0, trails::activeCount());
}

static void test_sampling_survives_a_millis_wrap() {
    uint32_t nearWrap = 0xFFFFFF00;
    trails::record(oneAt("abc123", 0, 0), nearWrap, 4000);
    // 512 ms later, having wrapped through zero: still inside the interval.
    trails::record(oneAt("abc123", 1, 0), nearWrap + 512, 4000);
    trails::Point pts[trails::TRAIL_POINTS];
    TEST_ASSERT_EQUAL_INT(1, trails::fetch("abc123", pts, trails::TRAIL_POINTS));
    // 5 s later, also past the wrap: due again.
    trails::record(oneAt("abc123", 2, 0), nearWrap + 5000, 4000);
    TEST_ASSERT_EQUAL_INT(2, trails::fetch("abc123", pts, trails::TRAIL_POINTS));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_clear_leaves_no_trails);
    RUN_TEST(test_records_a_point_at_the_aircraft_position);
    RUN_TEST(test_unknown_aircraft_has_no_trail);
    RUN_TEST(test_sampling_interval_is_respected);
    RUN_TEST(test_points_come_back_oldest_first);
    RUN_TEST(test_ring_buffer_keeps_the_newest_points);
    RUN_TEST(test_small_output_buffer_gets_the_newest_points);
    RUN_TEST(test_trail_survives_a_replaced_snapshot);
    RUN_TEST(test_expire_drops_only_the_stale);
    RUN_TEST(test_a_full_table_evicts_the_least_recently_seen);
    RUN_TEST(test_aircraft_without_a_hex_is_ignored);
    RUN_TEST(test_sampling_survives_a_millis_wrap);
    return UNITY_END();
}
