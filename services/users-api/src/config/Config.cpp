#include "config/Config.hpp"

#include <charconv>
#include <stdexcept>
#include <string_view>

namespace tecflix::users {

namespace {

constexpr std::string_view kPortVariable = "USERS_API_PORT";
constexpr std::string_view kLogLevelVariable = "TECFX_LOG_LEVEL";
constexpr std::string_view kJwtSecretVariable = "JWT_SECRET";
constexpr std::string_view kJwtLifetimeVariable = "JWT_EXPIRATION_SECONDS";
constexpr std::string_view kQuotaVariable = "DEFAULT_QUOTA_BYTES";
constexpr std::string_view kDataDirectoryVariable = "USERS_DATA_DIR";

constexpr std::uint16_t kDefaultPort = 8080;
constexpr std::string_view kDefaultLogLevel = "info";
constexpr std::uint64_t kDefaultJwtLifetimeSeconds = 3600;
constexpr std::uint64_t kMaxJwtLifetimeSeconds = 30ULL * 24 * 3600;  // 30 días
constexpr std::uint64_t kDefaultQuotaBytes = 5ULL * 1024 * 1024 * 1024;  // 5 GiB
constexpr std::uint64_t kMaxQuotaBytes = 1ULL << 50;  // 1 PiB
constexpr std::string_view kDefaultDataDirectory = "data";
constexpr std::uint64_t kMaxPort = 65535;

std::uint64_t parseInteger(std::string_view variable, const std::string& text, std::uint64_t min, std::uint64_t max) {
    std::uint64_t value = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto [end, error] = std::from_chars(first, last, value);
    if (error != std::errc{} || end != last || value < min || value > max) {
        throw std::invalid_argument(std::string{variable} + " debe ser un entero entre " + std::to_string(min) +
                                    " y " + std::to_string(max));
    }
    return value;
}

std::uint64_t integerOrDefault(const IEnvironment& environment, std::string_view variable, std::uint64_t fallback,
                               std::uint64_t min, std::uint64_t max) {
    const auto text = environment.get(variable);
    return text ? parseInteger(variable, *text, min, max) : fallback;
}

}  // namespace

Config Config::fromEnvironment(const IEnvironment& environment) {
    const auto secret = environment.get(kJwtSecretVariable);
    if (!secret || secret->empty()) {
        throw std::invalid_argument(std::string{kJwtSecretVariable} + " es obligatorio");
    }

    Config config{};
    config.port = static_cast<std::uint16_t>(integerOrDefault(environment, kPortVariable, kDefaultPort, 1, kMaxPort));
    config.logLevel = environment.get(kLogLevelVariable).value_or(std::string{kDefaultLogLevel});
    config.jwtSecret = *secret;
    config.jwtLifetime = std::chrono::seconds{static_cast<std::chrono::seconds::rep>(
        integerOrDefault(environment, kJwtLifetimeVariable, kDefaultJwtLifetimeSeconds, 1, kMaxJwtLifetimeSeconds))};
    config.defaultQuotaBytes = integerOrDefault(environment, kQuotaVariable, kDefaultQuotaBytes, 1, kMaxQuotaBytes);
    config.dataDirectory = environment.get(kDataDirectoryVariable).value_or(std::string{kDefaultDataDirectory});
    return config;
}

}  // namespace tecflix::users
