#include <gtest/gtest.h>

#include "util/IsoTime.hpp"

namespace tecflix::users {

TEST(IsoTimeTest, FormatsKnownMoments) {
    EXPECT_EQ(formatIsoUtc(0), "1970-01-01T00:00:00Z");
    EXPECT_EQ(formatIsoUtc(1'800'000'000), "2027-01-15T08:00:00Z");
    EXPECT_EQ(formatIsoUtc(951'782'400), "2000-02-29T00:00:00Z");  // año bisiesto
}

TEST(IsoTimeTest, ParsesWhatItFormats) {
    for (const std::int64_t moment : {0LL, 1LL, 86'399LL, 951'782'400LL, 1'800'000'000LL, 4'102'444'799LL}) {
        EXPECT_EQ(parseIsoUtc(formatIsoUtc(moment)), moment);
    }
}

TEST(IsoTimeTest, ParsesAKnownDate) {
    EXPECT_EQ(parseIsoUtc("2027-01-15T08:00:00Z"), 1'800'000'000);
}

TEST(IsoTimeTest, RejectsInvalidText) {
    EXPECT_FALSE(parseIsoUtc("").has_value());
    EXPECT_FALSE(parseIsoUtc("2027-01-15").has_value());
    EXPECT_FALSE(parseIsoUtc("2027-01-15T08:00:00").has_value());         // sin la Z
    EXPECT_FALSE(parseIsoUtc("2027-01-15T08:00:00+00:00").has_value());   // con desfase
    EXPECT_FALSE(parseIsoUtc("2027-01-15 08:00:00Z").has_value());
    EXPECT_FALSE(parseIsoUtc("2027-13-15T08:00:00Z").has_value());        // mes 13
    EXPECT_FALSE(parseIsoUtc("2027-02-30T08:00:00Z").has_value());        // día que no existe
    EXPECT_FALSE(parseIsoUtc("2027-01-15T24:00:00Z").has_value());
    EXPECT_FALSE(parseIsoUtc("2027-01-15T08:60:00Z").has_value());
    EXPECT_FALSE(parseIsoUtc("2027-01-15T08:00:60Z").has_value());
    EXPECT_FALSE(parseIsoUtc("20x7-01-15T08:00:00Z").has_value());
    EXPECT_FALSE(parseIsoUtc("2023-02-29T00:00:00Z").has_value());        // 2023 no es bisiesto
}

}  // namespace tecflix::users
