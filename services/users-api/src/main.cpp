#include <httplib.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "api/HealthController.hpp"
#include "config/Config.hpp"
#include "config/ProcessEnvironment.hpp"

namespace {

constexpr const char* kBindAddress = "0.0.0.0";

// Log estructurado en JSON, una línea por evento.
void logEvent(std::ostream& stream, const std::string& level, const std::string& message) {
    stream << nlohmann::json{{"level", level}, {"service", "users-api"}, {"message", message}}.dump()
           << '\n';
}

}  // namespace

int main() {
    try {
        const tecflix::users::ProcessEnvironment environment;
        const auto config = tecflix::users::Config::fromEnvironment(environment);

        httplib::Server server;
        const tecflix::users::HealthController health;
        health.registerRoutes(server);

        logEvent(std::cout, "info", "users-api escuchando en el puerto " + std::to_string(config.port));
        if (!server.listen(kBindAddress, config.port)) {
            logEvent(std::cerr, "error", "no se pudo abrir el puerto " + std::to_string(config.port));
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        logEvent(std::cerr, "error", error.what());
        return EXIT_FAILURE;
    }
}
