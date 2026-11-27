#include <gtest/gtest.h>

#include <httplib.h>

#include <thread>

#include "api/Cors.hpp"

namespace tecflix::users {

namespace {

constexpr const char* kOrigin = "http://localhost:5173";

// Levanta un servidor con CORS y una ruta GET, y ejecuta "request" contra él.
template <typename Request>
httplib::Result withCorsServer(Request request) {
    httplib::Server server;
    configureCors(server, kOrigin);
    server.Get("/api/v1/ping", [](const httplib::Request&, httplib::Response& response) { response.set_content("pong", "text/plain"); });
    const int port = server.bind_to_any_port("127.0.0.1");
    std::thread worker([&server] { server.listen_after_bind(); });
    httplib::Client client("127.0.0.1", port);
    auto result = request(client);
    server.stop();
    worker.join();
    return result;
}

}  // namespace

TEST(CorsTest, PreflightIsAnsweredWith204AndTheAllowedHeaders) {
    const auto result = withCorsServer([](httplib::Client& client) {
        return client.Options("/api/v1/auth/login", {{"Origin", kOrigin}, {"Access-Control-Request-Method", "POST"}});
    });

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 204);
    EXPECT_EQ(result->get_header_value("Access-Control-Allow-Origin"), kOrigin);
    EXPECT_NE(result->get_header_value("Access-Control-Allow-Methods").find("PATCH"), std::string::npos);
    EXPECT_NE(result->get_header_value("Access-Control-Allow-Headers").find("Authorization"), std::string::npos);
    EXPECT_EQ(result->get_header_value("Access-Control-Max-Age"), "600");
}

TEST(CorsTest, NormalResponsesCarryTheAllowedOrigin) {
    const auto result = withCorsServer([](httplib::Client& client) { return client.Get("/api/v1/ping"); });

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
    EXPECT_EQ(result->body, "pong");
    EXPECT_EQ(result->get_header_value("Access-Control-Allow-Origin"), kOrigin);
    EXPECT_EQ(result->get_header_value("Vary"), "Origin");
    EXPECT_NE(result->get_header_value("Access-Control-Expose-Headers").find("Content-Range"), std::string::npos);
}

}  // namespace tecflix::users
