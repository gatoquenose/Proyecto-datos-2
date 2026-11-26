#include <gtest/gtest.h>

#include <httplib.h>

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>

#include "FakeMetrics.hpp"
#include "api/ServerSetup.hpp"
#include "api/TracedRouter.hpp"
#include "auth/UuidV4Generator.hpp"

namespace tecflix::users {

using nlohmann::json;
using testing::FakeHttpMetrics;

namespace {

class TracedRouterTest : public ::testing::Test {
protected:
    TracedRouterTest() : router_(server_, metrics_, ids_, "users-api") {
        configureServer(server_, ids_);
        router_.Get("/api/v1/things/:id", [](const httplib::Request& request, httplib::Response& response) {
            response.set_content("cosa " + request.path_params.at("id"), "text/plain");
        });
        router_.Post("/api/v1/things", [](const httplib::Request&, httplib::Response& response) { response.status = 201; });
        router_.Get("/api/v1/broken", [](const httplib::Request&, httplib::Response&) { throw std::runtime_error("falla interna"); });
        router_.Delete("/api/v1/things/:id", [](const httplib::Request&, httplib::Response& response) { response.status = 204; });
        router_.Patch("/api/v1/things/:id", [](const httplib::Request&, httplib::Response& response) { response.status = 404; });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
    }

    ~TracedRouterTest() override {
        server_.stop();
        thread_.join();
    }

    TracedRouterTest(const TracedRouterTest&) = delete;
    TracedRouterTest& operator=(const TracedRouterTest&) = delete;
    TracedRouterTest(TracedRouterTest&&) = delete;
    TracedRouterTest& operator=(TracedRouterTest&&) = delete;

    FakeHttpMetrics metrics_;
    UuidV4Generator ids_;
    httplib::Server server_;
    TracedRouter router_;
    int port_ = 0;
    std::thread thread_;
};

}  // namespace

TEST_F(TracedRouterTest, RoutesStillAnswerNormally) {
    httplib::Client client("127.0.0.1", port_);

    const auto result = client.Get("/api/v1/things/42");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
    EXPECT_EQ(result->body, "cosa 42");
}

TEST_F(TracedRouterTest, RecordsMethodRoutePatternStatusAndDuration) {
    httplib::Client client("127.0.0.1", port_);

    client.Get("/api/v1/things/42");
    client.Get("/api/v1/things/99");
    client.Post("/api/v1/things", "", "application/json");
    client.Delete("/api/v1/things/7");
    client.Patch("/api/v1/things/7", "", "application/json");

    ASSERT_EQ(metrics_.records.size(), 5U);
    // La ruta registrada es el patrón, no el id real: así no se crea una serie por cada usuario.
    EXPECT_EQ(metrics_.records[0].method, "GET");
    EXPECT_EQ(metrics_.records[0].route, "/api/v1/things/:id");
    EXPECT_EQ(metrics_.records[0].status, 200);
    EXPECT_EQ(metrics_.records[1].route, "/api/v1/things/:id");
    EXPECT_EQ(metrics_.records[2].method, "POST");
    EXPECT_EQ(metrics_.records[2].status, 201);
    EXPECT_EQ(metrics_.records[3].method, "DELETE");
    EXPECT_EQ(metrics_.records[3].status, 204);
    EXPECT_EQ(metrics_.records[4].method, "PATCH");
    EXPECT_EQ(metrics_.records[4].status, 404);
    for (const auto& record : metrics_.records) {
        EXPECT_GE(record.seconds, 0.0);
    }
}

TEST_F(TracedRouterTest, AnEndpointThatThrowsAnswers500WithoutLeakingTheCause) {
    httplib::Client client("127.0.0.1", port_);

    const auto result = client.Get("/api/v1/broken");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 500);
    const json body = json::parse(result->body);
    EXPECT_EQ(body["code"], "INTERNAL_ERROR");
    EXPECT_FALSE(body["traceId"].get<std::string>().empty());
    EXPECT_EQ(result->body.find("falla interna"), std::string::npos);
    ASSERT_EQ(metrics_.records.size(), 1U);
    EXPECT_EQ(metrics_.records[0].status, 500);
}

TEST_F(TracedRouterTest, TheServerKeepsWorkingAfterAFailure) {
    httplib::Client client("127.0.0.1", port_);
    client.Get("/api/v1/broken");

    const auto result = client.Get("/api/v1/things/1");

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
}

}  // namespace tecflix::users
