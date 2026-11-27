#include <gtest/gtest.h>

#include <httplib.h>

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <thread>

#include "api/ServerSetup.hpp"
#include "domain/IIdGenerator.hpp"

namespace tecflix::users {

namespace {

class FixedIds final : public IIdGenerator {
public:
    [[nodiscard]] std::string next() const override { return "traza-fija"; }
};

class ServerSetupTest : public ::testing::Test {
protected:
    ServerSetupTest() {
        configureServer(server_, ids_);
        server_.Get("/api/v1/falla", [](const httplib::Request&, httplib::Response&) { throw std::runtime_error("detalle interno"); });
        server_.Get("/api/v1/rara", [](const httplib::Request&, httplib::Response&) { throw 42; });
        server_.Post("/api/v1/eco", [](const httplib::Request& request, httplib::Response& response) { response.set_content(request.body, "text/plain"); });
        port_ = server_.bind_to_any_port("127.0.0.1");
        worker_ = std::thread([this] { server_.listen_after_bind(); });
    }
    ~ServerSetupTest() override {
        server_.stop();
        worker_.join();
    }

    httplib::Client client() const { return httplib::Client("127.0.0.1", port_); }

    FixedIds ids_;
    httplib::Server server_;
    int port_ = 0;
    std::thread worker_;
};

}  // namespace

TEST_F(ServerSetupTest, UnknownRouteGivesJsonNotFoundWithTraceId) {
    auto http = client();
    const auto result = http.Get("/api/v1/no-existe");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 404);
    const auto body = nlohmann::json::parse(result->body);
    EXPECT_EQ(body.at("code"), "NOT_FOUND");
    EXPECT_EQ(body.at("traceId"), "traza-fija");
}

TEST_F(ServerSetupTest, UnhandledExceptionGivesInternalErrorWithoutLeakingTheCause) {
    auto http = client();
    const auto result = http.Get("/api/v1/falla");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 500);
    const auto body = nlohmann::json::parse(result->body);
    EXPECT_EQ(body.at("code"), "INTERNAL_ERROR");
    EXPECT_EQ(body.at("traceId"), "traza-fija");
    EXPECT_EQ(result->body.find("detalle interno"), std::string::npos);
}

TEST_F(ServerSetupTest, NonStandardExceptionAlsoGivesInternalError) {
    auto http = client();
    const auto result = http.Get("/api/v1/rara");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 500);
    EXPECT_EQ(nlohmann::json::parse(result->body).at("code"), "INTERNAL_ERROR");
}

TEST_F(ServerSetupTest, OversizedBodyIsRejectedWith413) {
    auto http = client();
    const std::string huge(200 * 1024, 'x');
    const auto result = http.Post("/api/v1/eco", huge, "text/plain");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 413);
    EXPECT_EQ(nlohmann::json::parse(result->body).at("code"), "PAYLOAD_TOO_LARGE");
}

}  // namespace tecflix::users
