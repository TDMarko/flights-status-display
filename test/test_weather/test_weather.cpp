#include <unity.h>

#include <cstring>

#include "weather.h"

// The real NOAA response for Riga, trimmed to the fields the parser keeps.
static const char* REAL =
    "[{\"icaoId\":\"EVRA\",\"temp\":19,\"dewp\":14,\"wdir\":210,\"wspd\":4,"
    "\"visib\":\"6+\",\"altim\":1011,\"rawOb\":\"METAR EVRA 020950Z 21004KT 9999 BKN020 19/14\","
    "\"cover\":\"BKN\",\"fltCat\":\"MVFR\"}]";

void setUp() {}
void tearDown() {}

static void test_parses_a_real_noaa_report() {
    weather::Report r;
    TEST_ASSERT_TRUE(weather::parse(REAL, strlen(REAL), r));
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_TRUE(r.hasWind);
    TEST_ASSERT_FALSE(r.windVariable);
    TEST_ASSERT_EQUAL_INT(210, r.windDirDeg);
    TEST_ASSERT_EQUAL_INT(4, r.windKt);
    TEST_ASSERT_TRUE(r.hasTemp);
    TEST_ASSERT_EQUAL_INT(19, r.tempC);
    TEST_ASSERT_EQUAL_STRING("BKN", r.cover);
}

static void test_formats_the_header_line() {
    weather::Report r;
    TEST_ASSERT_TRUE(weather::parse(REAL, strlen(REAL), r));
    char line[40];
    weather::format(r, "RIX", line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("RIX SSW 4kt 19C BKN", line);
}

static void test_variable_wind_is_reported_as_vrb() {
    const char* body = "[{\"wdir\":\"VRB\",\"wspd\":3,\"temp\":-2,\"cover\":\"OVC\"}]";
    weather::Report r;
    TEST_ASSERT_TRUE(weather::parse(body, strlen(body), r));
    TEST_ASSERT_TRUE(r.windVariable);
    char line[40];
    weather::format(r, "RIX", line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("RIX VRB 3kt -2C OVC", line);
}

static void test_calm_wind_says_so_rather_than_zero_knots() {
    const char* body = "[{\"wdir\":0,\"wspd\":0,\"temp\":5,\"cover\":\"CLR\"}]";
    weather::Report r;
    TEST_ASSERT_TRUE(weather::parse(body, strlen(body), r));
    char line[40];
    weather::format(r, "RIX", line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("RIX CALM 5C CLR", line);
}

static void test_missing_fields_are_simply_left_out() {
    const char* body = "[{\"temp\":7}]";
    weather::Report r;
    TEST_ASSERT_TRUE(weather::parse(body, strlen(body), r));
    TEST_ASSERT_FALSE(r.hasWind);
    char line[40];
    weather::format(r, "TLL", line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("TLL 7C", line);
}

static void test_a_direction_without_a_speed_is_not_shown() {
    const char* body = "[{\"wdir\":180,\"temp\":7}]";
    weather::Report r;
    TEST_ASSERT_TRUE(weather::parse(body, strlen(body), r));
    TEST_ASSERT_FALSE(r.hasWind);
}

static void test_empty_array_and_junk_are_rejected() {
    weather::Report r;
    TEST_ASSERT_FALSE(weather::parse("[]", 2, r));
    TEST_ASSERT_FALSE(weather::parse("<html></html>", 13, r));
    TEST_ASSERT_FALSE(weather::parse("", 0, r));
    TEST_ASSERT_FALSE(weather::parse("[{}]", 4, r));
}

static void test_invalid_report_formats_to_an_empty_line() {
    weather::Report r;   // default: not valid
    char line[40] = "leftover";
    weather::format(r, "RIX", line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("", line);
}

static void test_compass_covers_all_sixteen_points_and_wraps() {
    TEST_ASSERT_EQUAL_STRING("N", weather::compass(0));
    TEST_ASSERT_EQUAL_STRING("N", weather::compass(360));
    TEST_ASSERT_EQUAL_STRING("N", weather::compass(11));
    TEST_ASSERT_EQUAL_STRING("NNE", weather::compass(23));
    TEST_ASSERT_EQUAL_STRING("E", weather::compass(90));
    TEST_ASSERT_EQUAL_STRING("S", weather::compass(180));
    TEST_ASSERT_EQUAL_STRING("SSW", weather::compass(210));
    TEST_ASSERT_EQUAL_STRING("W", weather::compass(270));
    TEST_ASSERT_EQUAL_STRING("NNW", weather::compass(340));
    TEST_ASSERT_EQUAL_STRING("N", weather::compass(350));
    // Wrapping must not index off the end of the table.
    TEST_ASSERT_EQUAL_STRING("N", weather::compass(359));
    TEST_ASSERT_EQUAL_STRING("N", weather::compass(-5));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_a_real_noaa_report);
    RUN_TEST(test_formats_the_header_line);
    RUN_TEST(test_variable_wind_is_reported_as_vrb);
    RUN_TEST(test_calm_wind_says_so_rather_than_zero_knots);
    RUN_TEST(test_missing_fields_are_simply_left_out);
    RUN_TEST(test_a_direction_without_a_speed_is_not_shown);
    RUN_TEST(test_empty_array_and_junk_are_rejected);
    RUN_TEST(test_invalid_report_formats_to_an_empty_line);
    RUN_TEST(test_compass_covers_all_sixteen_points_and_wraps);
    return UNITY_END();
}
