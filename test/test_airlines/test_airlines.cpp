#include <unity.h>

#include <cstdio>
#include <cstring>

#include "airlines.h"

void setUp() {}
void tearDown() {}

static void test_resolves_carriers_seen_over_riga() {
    TEST_ASSERT_EQUAL_STRING("airBaltic", airlines::fromCallsign("BTI9UG"));
    TEST_ASSERT_EQUAL_STRING("Ryanair", airlines::fromCallsign("RYR9JC"));
    TEST_ASSERT_EQUAL_STRING("Finnair", airlines::fromCallsign("FIN1143"));
    TEST_ASSERT_EQUAL_STRING("SAS", airlines::fromCallsign("SAS742"));
    TEST_ASSERT_EQUAL_STRING("Rossiya", airlines::fromCallsign("SDM6324"));
    TEST_ASSERT_EQUAL_STRING("Uzbekistan", airlines::fromCallsign("UZB211"));
    TEST_ASSERT_EQUAL_STRING("Norwegian", airlines::fromCallsign("NSZ2005"));
}

static void test_registration_style_callsigns_have_no_airline() {
    // A registration is not an ICAO airline callsign, and guessing from one
    // would put a wrong carrier on screen.
    TEST_ASSERT_NULL(airlines::fromCallsign("N159L"));
    TEST_ASSERT_NULL(airlines::fromCallsign("YLEVI"));
    TEST_ASSERT_NULL(airlines::fromCallsign("SP-RKO"));
}

static void test_private_callsigns_without_a_digit_fourth_are_rejected() {
    TEST_ASSERT_NULL(airlines::fromCallsign("BRIO66"));
    TEST_ASSERT_NULL(airlines::fromCallsign("ABCDEF"));
}

static void test_unknown_designator_returns_null_rather_than_guessing() {
    TEST_ASSERT_NULL(airlines::fromCallsign("ZZZ123"));
    TEST_ASSERT_NULL(airlines::fromCallsign("QQQ9"));
}

static void test_handles_empty_short_and_null_input() {
    TEST_ASSERT_NULL(airlines::fromCallsign(nullptr));
    TEST_ASSERT_NULL(airlines::fromCallsign(""));
    TEST_ASSERT_NULL(airlines::fromCallsign("BT"));
    TEST_ASSERT_NULL(airlines::fromCallsign("BTI"));
}

static void test_every_name_fits_beside_a_route_on_the_panel_line() {
    // Panel line is 22 chars at text size 1; "BRU>RIX " takes eight.
    TEST_ASSERT_TRUE_MESSAGE(airlines::longestNameLength() <= 14, "an airline name is too long");
}

static void test_table_is_sorted_and_has_no_duplicate_designators() {
    // Sorted order keeps the table readable and makes a duplicate obvious;
    // a duplicate designator would silently shadow the second entry.
    int n = airlines::count();
    TEST_ASSERT_TRUE(n > 50);
    for (int i = 1; i < n; i++) {
        int cmp = strcmp(airlines::codeAt(i - 1), airlines::codeAt(i));
        char msg[64];
        snprintf(msg, sizeof(msg), "%s then %s", airlines::codeAt(i - 1), airlines::codeAt(i));
        TEST_ASSERT_TRUE_MESSAGE(cmp < 0, msg);
    }
}

static void test_every_designator_is_three_upper_case_letters() {
    for (int i = 0; i < airlines::count(); i++) {
        const char* c = airlines::codeAt(i);
        TEST_ASSERT_EQUAL_INT(3, (int)strlen(c));
        for (int k = 0; k < 3; k++) TEST_ASSERT_TRUE(c[k] >= 'A' && c[k] <= 'Z');
    }
}

static void test_every_name_is_plain_ascii() {
    // The panel uses the classic 5x7 font: anything above 0x7E renders as junk.
    for (int i = 0; i < airlines::count(); i++) {
        const char* nm = airlines::nameAt(i);
        TEST_ASSERT_TRUE(strlen(nm) > 0);
        for (const char* p = nm; *p; p++)
            TEST_ASSERT_TRUE_MESSAGE((unsigned char)*p >= 0x20 && (unsigned char)*p <= 0x7E, nm);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_resolves_carriers_seen_over_riga);
    RUN_TEST(test_registration_style_callsigns_have_no_airline);
    RUN_TEST(test_private_callsigns_without_a_digit_fourth_are_rejected);
    RUN_TEST(test_unknown_designator_returns_null_rather_than_guessing);
    RUN_TEST(test_handles_empty_short_and_null_input);
    RUN_TEST(test_every_name_fits_beside_a_route_on_the_panel_line);
    RUN_TEST(test_table_is_sorted_and_has_no_duplicate_designators);
    RUN_TEST(test_every_designator_is_three_upper_case_letters);
    RUN_TEST(test_every_name_is_plain_ascii);
    return UNITY_END();
}
