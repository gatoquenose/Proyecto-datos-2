#include "api/HealthController.hpp"

#include <nlohmann/json.hpp>
#include <string_view>

namespace tecflix::users {

namespace {

constexpr std::string_view kHealthPath = "/api/v1/health";
constexpr std::string_view kJsonContentType = "application/json";

}  // namespace

std::string HealthController::healthBody() {
    return nlohmann::json{{"status", "ok"}}.dump();
}

void HealthController::registerRoutes(httplib::Server& server) const {
    server.Get(std::string{kHealthPath},
               [](const httplib::Request& /*request*/, httplib::Response& response) {
                   response.set_content(healthBody(), std::string{kJsonContentType});
               });
}

}  // namespace tecflix::users
