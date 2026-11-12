#include <gtest/gtest.h>

#include <stdexcept>

#include "FakeEnvironment.hpp"
#include "config/Config.hpp"

namespace tecflix::users {

using testing::FakeEnvironment;

TEST(ConfigTest, UsesDefaultsWhenVariablesAreMissing) {
    const FakeEnvironment environment;

    const auto config = Config::fromEnvironment(environment);

    EXPECT_EQ(config.port, 8080);
    EXPECT_EQ(config.logLevel, "info");
}

TEST(ConfigTest, ReadsPortAndLogLevelFromEnvironment) {
    const FakeEnvironment environment{{{"USERS_API_PORT", "9090"}, {"TECFX_LOG_LEVEL", "debug"}}};

    const auto config = Config::fromEnvironment(environment);

    EXPECT_EQ(config.port, 9090);
    EXPECT_EQ(config.logLevel, "debug");
}

TEST(ConfigTest, RejectsPortThatIsNotANumber) {
    const FakeEnvironment environment{{{"USERS_API_PORT", "abc"}}};

    EXPECT_THROW(Config::fromEnvironment(environment), std::invalid_argument);
}

TEST(ConfigTest, RejectsPortOutOfRange) {
    const FakeEnvironment zero{{{"USERS_API_PORT", "0"}}};
    const FakeEnvironment tooBig{{{"USERS_API_PORT", "70000"}}};

    EXPECT_THROW(Config::fromEnvironment(zero), std::invalid_argument);
    EXPECT_THROW(Config::fromEnvironment(tooBig), std::invalid_argument);
}

}  // namespace tecflix::users
