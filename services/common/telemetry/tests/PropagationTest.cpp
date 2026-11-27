#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "tecflix/telemetry/ServerSpan.hpp"
#include "tecflix/telemetry/TelemetrySetup.hpp"

namespace tecflix::telemetry {

namespace {

constexpr const char* kClientTrace = "0af7651916cd43dd8448eb211c80319c";
constexpr const char* kClientParent = "b7ad6b7169203331";
constexpr std::size_t kTraceIdHexLength = 32;
constexpr std::size_t kSpanIdHexLength = 16;

// Nadie escucha en ese puerto: las pruebas solo necesitan que el SDK esté encendido, no enviar datos.
TelemetryConfig enabledConfig() {
    return {"prueba", "1.0.0", "http://127.0.0.1:1", std::chrono::milliseconds{60000}, {}};
}

HeaderLookup withTraceparent(const std::string& value) {
    return [value](std::string_view name) { return name == "traceparent" ? value : std::string{}; };
}

}  // namespace

TEST(PropagationTest, ContinuesTheTraceOfTheCaller) {
    const TelemetrySetup telemetry{enabledConfig()};
    ASSERT_TRUE(telemetry.enabled());

    const ServerSpan span{"prueba", "GET", "/ruta/:id", "/ruta/7",
                          withTraceparent(std::string{"00-"} + kClientTrace + "-" + kClientParent + "-01")};

    EXPECT_EQ(span.traceId(), kClientTrace);
    EXPECT_EQ(span.spanId().size(), kSpanIdHexLength);
    EXPECT_NE(span.spanId(), kClientParent);  // el span del servidor es hijo, con su propio id
}

TEST(PropagationTest, ContinuesTheTraceEvenWhenTracestateIsAlsoSent) {
    const TelemetrySetup telemetry{enabledConfig()};
    const HeaderLookup lookup = [](std::string_view name) {
        if (name == "traceparent") {
            return std::string{"00-"} + kClientTrace + "-" + kClientParent + "-01";
        }
        return name == "tracestate" ? std::string{"vendor=valor"} : std::string{};
    };

    const ServerSpan span{"prueba", "GET", "/ruta", "/ruta", lookup};

    EXPECT_EQ(span.traceId(), kClientTrace);
}

TEST(PropagationTest, StartsANewTraceWhenTheCallerSendsNone) {
    const TelemetrySetup telemetry{enabledConfig()};

    std::string first;
    std::string second;
    {
        const ServerSpan span{"prueba", "GET", "/a", "/a", withTraceparent("")};
        first = span.traceId();
    }
    {
        const ServerSpan span{"prueba", "GET", "/b", "/b", withTraceparent("")};
        second = span.traceId();
    }

    EXPECT_EQ(first.size(), kTraceIdHexLength);
    EXPECT_NE(first, second);  // cada petición sin traceparent abre su propia traza
}

TEST(PropagationTest, IgnoresAMalformedTraceparent) {
    const TelemetrySetup telemetry{enabledConfig()};

    const ServerSpan span{"prueba", "GET", "/a", "/a", withTraceparent("esto-no-es-traceparent")};

    EXPECT_EQ(span.traceId().size(), kTraceIdHexLength);
    EXPECT_NE(span.traceId(), kClientTrace);
}

TEST(PropagationTest, TheActiveTraceIsTheOneOfTheRunningSpan) {
    const TelemetrySetup telemetry{enabledConfig()};
    EXPECT_TRUE(currentTraceId().empty());

    {
        const ServerSpan span{"prueba", "GET", "/a", "/a", withTraceparent("")};
        EXPECT_EQ(currentTraceId(), span.traceId());
    }

    EXPECT_TRUE(currentTraceId().empty());  // al terminar la petición ya no hay traza activa
}

TEST(PropagationTest, WithoutTelemetryThereAreNoIdentifiers) {
    const TelemetrySetup telemetry{{"prueba", "1.0.0", "", std::chrono::milliseconds{1000}, {}}};

    const ServerSpan span{"prueba", "GET", "/a", "/a", withTraceparent("")};

    EXPECT_FALSE(telemetry.enabled());
    EXPECT_TRUE(span.traceId().empty());
    EXPECT_TRUE(currentTraceId().empty());
}

}  // namespace tecflix::telemetry
