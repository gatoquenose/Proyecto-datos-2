#include "config/Config.hpp"

#include <algorithm>
#include <charconv>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace tecflix::users {

namespace {

constexpr std::string_view kPortVariable = "USERS_API_PORT";
constexpr std::string_view kLogLevelVariable = "TECFX_LOG_LEVEL";
constexpr std::string_view kJwtSecretVariable = "JWT_SECRET";
constexpr std::string_view kServiceTokenVariable = "SERVICE_TOKEN";
constexpr std::string_view kReservationLifetimeVariable = "RESERVATION_TTL_SECONDS";
constexpr std::string_view kSweepIntervalVariable = "RESERVATION_SWEEP_SECONDS";
constexpr std::string_view kJwtLifetimeVariable = "JWT_EXPIRATION_SECONDS";
constexpr std::string_view kQuotaVariable = "DEFAULT_QUOTA_BYTES";
constexpr std::string_view kDataDirectoryVariable = "USERS_DATA_DIR";
constexpr std::string_view kServiceNameVariable = "OTEL_SERVICE_NAME";
constexpr std::string_view kOtlpEndpointVariable = "OTEL_EXPORTER_OTLP_ENDPOINT";
constexpr std::string_view kSdkDisabledVariable = "OTEL_SDK_DISABLED";
constexpr std::string_view kMetricIntervalVariable = "OTEL_METRIC_EXPORT_INTERVAL";  // milisegundos, como en el estándar
constexpr std::string_view kDefaultServiceName = "users-api";
constexpr std::uint64_t kDefaultMetricIntervalMs = 15000;
constexpr std::uint64_t kMinMetricIntervalMs = 100;
constexpr std::uint64_t kMaxMetricIntervalMs = 3600ULL * 1000;

constexpr std::uint16_t kDefaultPort = 8080;
constexpr std::string_view kDefaultLogLevel = "info";
constexpr std::uint64_t kDefaultJwtLifetimeSeconds = 3600;
constexpr std::uint64_t kMaxJwtLifetimeSeconds = 30ULL * 24 * 3600;  // 30 días
constexpr std::uint64_t kDefaultQuotaBytes = 5ULL * 1024 * 1024 * 1024;  // 5 GiB
constexpr std::uint64_t kMaxQuotaBytes = 1ULL << 50;  // 1 PiB
constexpr std::string_view kDefaultDataDirectory = "data";
constexpr std::size_t kMinSecretLength = 32;
constexpr std::uint64_t kDefaultReservationLifetimeSeconds = 24ULL * 3600;  // 1 día
constexpr std::uint64_t kDefaultSweepIntervalSeconds = 60;
constexpr std::uint64_t kMaxSeconds = 365ULL * 24 * 3600;
constexpr std::uint64_t kMaxPort = 65535;

std::uint64_t parseInteger(std::string_view variable, const std::string& text, std::uint64_t min, std::uint64_t max) {
    std::uint64_t value = 0;
    const char* first = text.data();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): from_chars exige un rango de punteros.
    const char* last = first + text.size();
    const auto [end, error] = std::from_chars(first, last, value);
    if (error != std::errc{} || end != last || value < min || value > max) {
        throw std::invalid_argument(std::string{variable} + " debe ser un entero entre " + std::to_string(min) +
                                    " y " + std::to_string(max));
    }
    return value;
}

// OTEL_SDK_DISABLED=true (sin distinguir mayúsculas) apaga la telemetría.
bool isTrue(const std::optional<std::string>& value) {
    if (!value || value->size() != 4) {
        return false;
    }
    std::string lower = *value;
    std::ranges::transform(lower, lower.begin(), [](char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c); });
    return lower == "true";
}

std::string requiredSecret(const IEnvironment& environment, std::string_view variable, std::size_t minLength) {
    const auto value = environment.get(variable);
    if (!value || value->empty()) {
        throw std::invalid_argument(std::string{variable} + " es obligatorio");
    }
    if (value->size() < minLength) {
        throw std::invalid_argument(std::string{variable} + " debe tener al menos " + std::to_string(minLength) +
                                    " caracteres");
    }
    return *value;
}

std::uint64_t integerOrDefault(const IEnvironment& environment, std::string_view variable, std::uint64_t fallback,
                               std::uint64_t min, std::uint64_t max) {
    const auto text = environment.get(variable);
    return text ? parseInteger(variable, *text, min, max) : fallback;
}

std::chrono::seconds secondsOrDefault(const IEnvironment& environment, std::string_view variable,
                                      std::uint64_t fallback, std::uint64_t min, std::uint64_t max) {
    return std::chrono::seconds{
        static_cast<std::chrono::seconds::rep>(integerOrDefault(environment, variable, fallback, min, max))};
}

}  // namespace

Config Config::fromEnvironment(const IEnvironment& environment) {
    Config config{};
    config.port = static_cast<std::uint16_t>(integerOrDefault(environment, kPortVariable, kDefaultPort, 1, kMaxPort));
    config.logLevel = environment.get(kLogLevelVariable).value_or(std::string{kDefaultLogLevel});
    config.jwtSecret = requiredSecret(environment, kJwtSecretVariable, 1);
    config.serviceToken = requiredSecret(environment, kServiceTokenVariable, kMinSecretLength);
    config.jwtLifetime =
        secondsOrDefault(environment, kJwtLifetimeVariable, kDefaultJwtLifetimeSeconds, 1, kMaxJwtLifetimeSeconds);
    config.defaultQuotaBytes = integerOrDefault(environment, kQuotaVariable, kDefaultQuotaBytes, 1, kMaxQuotaBytes);
    config.reservationLifetime =
        secondsOrDefault(environment, kReservationLifetimeVariable, kDefaultReservationLifetimeSeconds, 1, kMaxSeconds);
    config.reservationSweepInterval =
        secondsOrDefault(environment, kSweepIntervalVariable, kDefaultSweepIntervalSeconds, 1, kMaxSeconds);
    config.dataDirectory = environment.get(kDataDirectoryVariable).value_or(std::string{kDefaultDataDirectory});
    config.serviceName = environment.get(kServiceNameVariable).value_or(std::string{kDefaultServiceName});
    config.otlpEndpoint = isTrue(environment.get(kSdkDisabledVariable)) ? std::string{}
                                                                        : environment.get(kOtlpEndpointVariable).value_or("");
    config.metricExportInterval = std::chrono::milliseconds{static_cast<std::chrono::milliseconds::rep>(
        integerOrDefault(environment, kMetricIntervalVariable, kDefaultMetricIntervalMs, kMinMetricIntervalMs,
                         kMaxMetricIntervalMs))};
    return config;
}

}  // namespace tecflix::users
