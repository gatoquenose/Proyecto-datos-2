#include "api/HealthController.hpp"

#include <exception>
#include <nlohmann/json.hpp>
#include <string_view>

#include "api/ApiResponses.hpp"
#include "api/HttpStatus.hpp"

namespace tecflix::users {

namespace {

constexpr std::string_view kHealthPath = "/api/v1/health";
constexpr std::string_view kReadyPath = "/api/v1/ready";

}  // namespace

HealthController::HealthController() : readinessCheck_([] { return true; }) {}

HealthController::HealthController(ReadinessCheck readinessCheck) : readinessCheck_(std::move(readinessCheck)) {}

std::string HealthController::healthBody() {
    return nlohmann::json{{"status", "ok"}}.dump();
}

void HealthController::registerRoutes(httplib::Server& server) const {
    server.Get(std::string{kHealthPath}, [](const httplib::Request& /*request*/, httplib::Response& response) {
        sendJson(response, http_status::kOk, nlohmann::json{{"status", "ok"}});
    });
    server.Get(std::string{kReadyPath}, [this](const httplib::Request& /*request*/, httplib::Response& response) {
        bool ready = false;
        try {
            ready = readinessCheck_();
        } catch (const std::exception&) {
            ready = false;  // si la comprobación falla, el servicio no está listo
        }
        if (ready) {
            sendJson(response, http_status::kOk, nlohmann::json{{"status", "ready"}});
        } else {
            sendError(response, http_status::kServiceUnavailable, "SERVICE_UNAVAILABLE", "El servicio no está listo", "");
        }
    });
}

}  // namespace tecflix::users
