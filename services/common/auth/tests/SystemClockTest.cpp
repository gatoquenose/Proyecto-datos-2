#include <gtest/gtest.h>

#include <chrono>

#include "tecflix/auth/SystemClock.hpp"

namespace tecflix::auth {

TEST(SystemClockTest, NowIsCloseToTheSystemClock) {
    const SystemClock clock;
    const auto before = std::chrono::system_clock::now();
    const auto reading = clock.now();
    const auto after = std::chrono::system_clock::now();

    EXPECT_GE(reading, before);
    EXPECT_LE(reading, after);
}

}  // namespace tecflix::auth
