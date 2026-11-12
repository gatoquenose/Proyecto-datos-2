#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "api/HealthController.hpp"

namespace tecflix::users {

TEST(HealthControllerTest, BodyReportsStatusOk) {
    const auto body = nlohmann::json::parse(HealthController::healthBody());

    EXPECT_EQ(body.at("status"), "ok");
}

}  // namespace tecflix::users
