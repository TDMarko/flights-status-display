#include <unity.h>

#include <cstdio>
#include <cstring>

#include "routes.h"

void setUp() { routes::clear(); }
void tearDown() {}

// The real shape of an adsb.lol route response, trimmed to what matters.
static const char* REAL_BODY =
    "{\"callsign\":\"BTI60L\",\"number\":\"60L\",\"airline_code\":\"BTI\","
    "\"airport_codes\":\"EBBR-EVRA\",\"_airport_codes_iata\":\"BRU-RIX\","
    "\"_airports\":[{\"name\":\"Brussels Airport\",\"icao\":\"EBBR\",\"iata\":\"BRU\"},"
    "{\"name\":\"Riga International Airport\",\"icao\":\"EVRA\",\"iata\":\"RIX\"}]}";

static void test_parses_a_real_route_response() {
    char out[routes::ROUTE_LEN];
    TEST_ASSERT_TRUE(routes::parseRouteBody(REAL_BODY, strlen(REAL_BODY), out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("BRU>RIX", out);
}

static void test_rejects_the_html_returned_for_unknown_callsigns() {
    // The API answers an unknown callsign with a redirect landing page, not a
    // 404 with JSON, so the parser has to shrug this off quietly.
    const char* html = "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"></head></html>";
    char out[routes::ROUTE_LEN];
    TEST_ASSERT_FALSE(routes::parseRouteBody(html, strlen(html), out, sizeof(out)));
}

static void test_rejects_json_without_an_iata_pair() {
    const char* body = "{\"callsign\":\"XXX1\",\"airport_codes\":\"unknown\"}";
    char out[routes::ROUTE_LEN];
    TEST_ASSERT_FALSE(routes::parseRouteBody(body, strlen(body), out, sizeof(out)));
}

static void test_rejects_a_pair_with_no_separator_or_a_missing_half() {
    char out[routes::ROUTE_LEN];
    const char* single = "{\"_airport_codes_iata\":\"RIX\"}";
    TEST_ASSERT_FALSE(routes::parseRouteBody(single, strlen(single), out, sizeof(out)));
    const char* trailing = "{\"_airport_codes_iata\":\"RIX-\"}";
    TEST_ASSERT_FALSE(routes::parseRouteBody(trailing, strlen(trailing), out, sizeof(out)));
    const char* leading = "{\"_airport_codes_iata\":\"-RIX\"}";
    TEST_ASSERT_FALSE(routes::parseRouteBody(leading, strlen(leading), out, sizeof(out)));
}

static void test_keeps_a_three_leg_route() {
    // Real example seen over Riga: CSG2695 flies CAN-CKG-AMS.
    const char* body = "{\"_airport_codes_iata\":\"CAN-CKG-AMS\"}";
    char out[routes::ROUTE_LEN];
    TEST_ASSERT_TRUE(routes::parseRouteBody(body, strlen(body), out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("CAN>CKG>AMS", out);
}

static void test_rejects_a_route_too_long_to_show() {
    const char* body = "{\"_airport_codes_iata\":\"BRU-RIX-HEL-ARN\"}";
    char out[routes::ROUTE_LEN];
    TEST_ASSERT_FALSE(routes::parseRouteBody(body, strlen(body), out, sizeof(out)));
}

static void test_rejects_empty_input() {
    char out[routes::ROUTE_LEN];
    TEST_ASSERT_FALSE(routes::parseRouteBody("", 0, out, sizeof(out)));
    TEST_ASSERT_FALSE(routes::parseRouteBody(nullptr, 10, out, sizeof(out)));
}

static void test_unlooked_up_callsign_is_null_but_known_absent_is_empty() {
    TEST_ASSERT_NULL(routes::lookup("BTI60L"));
    routes::store("BTI60L", "");            // API had nothing for it
    TEST_ASSERT_NOT_NULL(routes::lookup("BTI60L"));
    TEST_ASSERT_EQUAL_STRING("", routes::lookup("BTI60L"));
}

static void test_store_and_lookup_round_trip() {
    routes::store("BTI60L", "BRU>RIX");
    TEST_ASSERT_EQUAL_STRING("BRU>RIX", routes::lookup("BTI60L"));
    TEST_ASSERT_EQUAL_INT(1, routes::cachedCount());
    routes::store("BTI60L", "HEL>RIX");     // updates in place, no duplicate
    TEST_ASSERT_EQUAL_STRING("HEL>RIX", routes::lookup("BTI60L"));
    TEST_ASSERT_EQUAL_INT(1, routes::cachedCount());
}

static adsb::Snapshot withCallsigns(const char* const* names, int n) {
    adsb::Snapshot s;
    s.count = n;
    for (int i = 0; i < n; i++) {
        memset(&s.ac[i], 0, sizeof(s.ac[i]));
        snprintf(s.ac[i].callsign, sizeof(s.ac[i].callsign), "%s", names[i]);
    }
    return s;
}

static void test_next_pending_walks_nearest_first_and_stops_when_all_cached() {
    const char* names[] = {"AAA111", "BBB222", "CCC333"};
    adsb::Snapshot s = withCallsigns(names, 3);

    TEST_ASSERT_EQUAL_STRING("AAA111", routes::nextPending(s, 4));
    routes::store("AAA111", "RIX>ARN");
    TEST_ASSERT_EQUAL_STRING("BBB222", routes::nextPending(s, 4));
    routes::store("BBB222", "");            // no route, but now known
    TEST_ASSERT_EQUAL_STRING("CCC333", routes::nextPending(s, 4));
    routes::store("CCC333", "HEL>RIX");
    TEST_ASSERT_NULL(routes::nextPending(s, 4));
}

static void test_next_pending_ignores_aircraft_beyond_the_panel() {
    const char* names[] = {"AAA111", "BBB222", "CCC333"};
    adsb::Snapshot s = withCallsigns(names, 3);
    routes::store("AAA111", "RIX>ARN");
    // Only the first two are ever shown, so the third is not worth a request.
    TEST_ASSERT_EQUAL_STRING("BBB222", routes::nextPending(s, 2));
    routes::store("BBB222", "");
    TEST_ASSERT_NULL(routes::nextPending(s, 2));
}

static void test_blank_callsigns_are_skipped() {
    const char* names[] = {"", "BBB222"};
    adsb::Snapshot s = withCallsigns(names, 2);
    TEST_ASSERT_EQUAL_STRING("BBB222", routes::nextPending(s, 4));
    routes::store("", "RIX>ARN");
    TEST_ASSERT_EQUAL_INT(0, routes::cachedCount());
}

static void test_a_full_cache_evicts_the_oldest_entry() {
    char cs[12];
    for (int i = 0; i < routes::MAX_ROUTES; i++) {
        snprintf(cs, sizeof(cs), "AA%04d", i);
        routes::store(cs, "RIX>ARN");
    }
    TEST_ASSERT_EQUAL_INT(routes::MAX_ROUTES, routes::cachedCount());

    routes::store("ZZ9999", "HEL>RIX");
    TEST_ASSERT_EQUAL_INT(routes::MAX_ROUTES, routes::cachedCount());
    TEST_ASSERT_EQUAL_STRING("HEL>RIX", routes::lookup("ZZ9999"));
    TEST_ASSERT_NULL(routes::lookup("AA0000"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_a_real_route_response);
    RUN_TEST(test_rejects_the_html_returned_for_unknown_callsigns);
    RUN_TEST(test_rejects_json_without_an_iata_pair);
    RUN_TEST(test_rejects_a_pair_with_no_separator_or_a_missing_half);
    RUN_TEST(test_keeps_a_three_leg_route);
    RUN_TEST(test_rejects_a_route_too_long_to_show);
    RUN_TEST(test_rejects_empty_input);
    RUN_TEST(test_unlooked_up_callsign_is_null_but_known_absent_is_empty);
    RUN_TEST(test_store_and_lookup_round_trip);
    RUN_TEST(test_next_pending_walks_nearest_first_and_stops_when_all_cached);
    RUN_TEST(test_next_pending_ignores_aircraft_beyond_the_panel);
    RUN_TEST(test_blank_callsigns_are_skipped);
    RUN_TEST(test_a_full_cache_evicts_the_oldest_entry);
    return UNITY_END();
}
