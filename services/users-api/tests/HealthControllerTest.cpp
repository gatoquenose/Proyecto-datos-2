#include <gtest/gtest.h>

#include <httplib.h>

#include <atomic>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>

#include "api/HealthController.hpp"

namespace tecflix::users {

TEST(HealthControllerTest, BodyReportsStatusOk) {
    const auto body = nlohmann::json::parse(HealthController::healthBody());

    EXPECT_EQ(body.at("status"), "ok");
}


namespace {

// Levanta un servidor solo con /health y /ready, y devuelve el código que responde cada ruta.
int statusOf(const HealthController& controller, const char* path) {
    httplib::Server server;
    controller.registerRoutes(server);
    const int port = server.bind_to_any_port("127.0.0.1");
    std::thread worker([&server] { server.listen_after_bind(); });
    httplib::Client client("127.0.0.1", port);
    const auto result = client.Get(path);
    server.stop();
    worker.join();
    return result ? result->status : 0;
}

}  // namespace

TEST(HealthControllerTest, ReadyIsOkWhenTheCheckPasses) {
    const HealthController controller{[] { return true; }};

    EXPECT_EQ(statusOf(controller, "/api/v1/ready"), 200);
    EXPECT_EQ(statusOf(controller, "/api/v1/health"), 200);
}

TEST(HealthControllerTest, ReadyIsUnavailableWhenTheCheckFailsOrThrows) {
    const HealthController failing{[] { return false; }};
    const HealthController throwing{[]() -> bool { throw std::runtime_error("archivo dañado"); }};

    EXPECT_EQ(statusOf(failing, "/api/v1/ready"), 503);
    EXPECT_EQ(statusOf(throwing, "/api/v1/ready"), 503);
    EXPECT_EQ(statusOf(failing, "/api/v1/health"), 200);  // vivo aunque no esté listo
}

}  // namespace tecflix::users
