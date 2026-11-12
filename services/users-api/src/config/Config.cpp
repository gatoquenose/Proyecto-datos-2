#include "config/Config.hpp"

#include <charconv>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace tecflix::users {

namespace {

constexpr std::string_view kPortVariable = "USERS_API_PORT";
constexpr std::string_view kLogLevelVariable = "TECFX_LOG_LEVEL";
constexpr std::uint16_t kDefaultPort = 8080;
constexpr std::string_view kDefaultLogLevel = "info";

std::uint16_t parsePort(const std::string& text) {
    unsigned int value = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto [end, error] = std::from_chars(first, last, value);
    const bool valid = error == std::errc{} && end == last && value > 0 &&
                       value <= std::numeric_limits<std::uint16_t>::max();
    if (!valid) {
        throw std::invalid_argument("USERS_API_PORT debe ser un entero entre 1 y 65535");
    }
    return static_cast<std::uint16_t>(value);
}

}  // namespace

Config Config::fromEnvironment(const IEnvironment& environment) {
    Config config{kDefaultPort, std::string{kDefaultLogLevel}};
    if (const auto port = environment.get(kPortVariable)) {
        config.port = parsePort(*port);
    }
    if (const auto level = environment.get(kLogLevelVariable)) {
        config.logLevel = *level;
    }
    return config;
}

}  // namespace tecflix::users
