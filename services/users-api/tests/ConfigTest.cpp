#include <gtest/gtest.h>

#include <stdexcept>

#include "FakeEnvironment.hpp"
#include "config/Config.hpp"

namespace tecflix::users {

using testing::FakeEnvironment;

namespace {

constexpr const char* kSecret = "un-secreto-de-prueba-con-mas-de-32-bytes!";
constexpr const char* kServiceToken = "token-de-servicio-de-prueba-de-32-bytes-o-mas";

FakeEnvironment environmentWith(std::map<std::string, std::string, std::less<>> extra = {}) {
    extra["JWT_SECRET"] = kSecret;
    extra.emplace("SERVICE_TOKEN", kServiceToken);
    return FakeEnvironment{std::move(extra)};
}

}  // namespace

TEST(ConfigTest, UsesDefaultsWhenOptionalVariablesAreMissing) {
    const auto config = Config::fromEnvironment(environmentWith());

    EXPECT_EQ(config.port, 8080);
    EXPECT_EQ(config.logLevel, "info");
    EXPECT_EQ(config.jwtSecret, kSecret);
    EXPECT_EQ(config.serviceToken, kServiceToken);
    EXPECT_EQ(config.reservationLifetime, std::chrono::seconds{86400});
    EXPECT_EQ(config.reservationSweepInterval, std::chrono::seconds{60});
    EXPECT_EQ(config.jwtLifetime, std::chrono::seconds{3600});
    EXPECT_EQ(config.defaultQuotaBytes, 5368709120ULL);
    EXPECT_EQ(config.dataDirectory, std::filesystem::path{"data"});
}

TEST(ConfigTest, ReadsEveryVariableFromTheEnvironment) {
    const auto config = Config::fromEnvironment(environmentWith({{"USERS_API_PORT", "9090"},
                                                                 {"TECFX_LOG_LEVEL", "debug"},
                                                                 {"JWT_EXPIRATION_SECONDS", "60"},
                                                                 {"DEFAULT_QUOTA_BYTES", "1000"},
                                                                 {"USERS_DATA_DIR", "otra-carpeta"},
                                                                 {"RESERVATION_TTL_SECONDS", "120"},
                                                                 {"RESERVATION_SWEEP_SECONDS", "5"}}));

    EXPECT_EQ(config.port, 9090);
    EXPECT_EQ(config.logLevel, "debug");
    EXPECT_EQ(config.jwtLifetime, std::chrono::seconds{60});
    EXPECT_EQ(config.defaultQuotaBytes, 1000U);
    EXPECT_EQ(config.dataDirectory, std::filesystem::path{"otra-carpeta"});
    EXPECT_EQ(config.reservationLifetime, std::chrono::seconds{120});
    EXPECT_EQ(config.reservationSweepInterval, std::chrono::seconds{5});
}

TEST(ConfigTest, TelemetryIsOffUnlessAnEndpointIsGiven) {
    const auto config = Config::fromEnvironment(environmentWith());

    EXPECT_TRUE(config.otlpEndpoint.empty());
    EXPECT_EQ(config.serviceName, "users-api");
    EXPECT_EQ(config.metricExportInterval, std::chrono::milliseconds{15000});
}

TEST(ConfigTest, ReadsTheOpenTelemetryVariables) {
    const auto config = Config::fromEnvironment(environmentWith({{"OTEL_EXPORTER_OTLP_ENDPOINT", "http://otel-collector:4318"},
                                                                 {"OTEL_SERVICE_NAME", "usuarios"},
                                                                 {"OTEL_METRIC_EXPORT_INTERVAL", "5000"}}));

    EXPECT_EQ(config.otlpEndpoint, "http://otel-collector:4318");
    EXPECT_EQ(config.serviceName, "usuarios");
    EXPECT_EQ(config.metricExportInterval, std::chrono::milliseconds{5000});
}

TEST(ConfigTest, OtelSdkDisabledTurnsTelemetryOffEvenWithAnEndpoint) {
    const auto config = Config::fromEnvironment(
        environmentWith({{"OTEL_EXPORTER_OTLP_ENDPOINT", "http://otel-collector:4318"}, {"OTEL_SDK_DISABLED", "TRUE"}}));

    EXPECT_TRUE(config.otlpEndpoint.empty());
}

TEST(ConfigTest, RejectsAMetricIntervalOutOfRange) {
    EXPECT_THROW(Config::fromEnvironment(environmentWith({{"OTEL_METRIC_EXPORT_INTERVAL", "10"}})), std::invalid_argument);
    EXPECT_THROW(Config::fromEnvironment(environmentWith({{"OTEL_METRIC_EXPORT_INTERVAL", "rapido"}})), std::invalid_argument);
}

TEST(ConfigTest, SecretsAreRequired) {
    EXPECT_THROW(Config::fromEnvironment(FakeEnvironment{}), std::invalid_argument);
    EXPECT_THROW(Config::fromEnvironment(FakeEnvironment{{{"JWT_SECRET", ""}, {"SERVICE_TOKEN", kServiceToken}}}),
                 std::invalid_argument);
    EXPECT_THROW(Config::fromEnvironment(FakeEnvironment{{{"JWT_SECRET", kSecret}}}), std::invalid_argument);
}

TEST(ConfigTest, ServiceTokenMustBeLongEnough) {
    EXPECT_THROW(Config::fromEnvironment(FakeEnvironment{{{"JWT_SECRET", kSecret}, {"SERVICE_TOKEN", "corto"}}}),
                 std::invalid_argument);
}

TEST(ConfigTest, RejectsPortThatIsNotANumber) {
    EXPECT_THROW(Config::fromEnvironment(environmentWith({{"USERS_API_PORT", "abc"}})), std::invalid_argument);
}

TEST(ConfigTest, RejectsPortOutOfRange) {
    EXPECT_THROW(Config::fromEnvironment(environmentWith({{"USERS_API_PORT", "0"}})), std::invalid_argument);
    EXPECT_THROW(Config::fromEnvironment(environmentWith({{"USERS_API_PORT", "70000"}})), std::invalid_argument);
}

TEST(ConfigTest, RejectsInvalidLifetimeAndQuota) {
    EXPECT_THROW(Config::fromEnvironment(environmentWith({{"JWT_EXPIRATION_SECONDS", "0"}})), std::invalid_argument);
    EXPECT_THROW(Config::fromEnvironment(environmentWith({{"JWT_EXPIRATION_SECONDS", "-5"}})), std::invalid_argument);
    EXPECT_THROW(Config::fromEnvironment(environmentWith({{"DEFAULT_QUOTA_BYTES", "0"}})), std::invalid_argument);
    EXPECT_THROW(Config::fromEnvironment(environmentWith({{"DEFAULT_QUOTA_BYTES", "mucho"}})), std::invalid_argument);
}

}  // namespace tecflix::users
