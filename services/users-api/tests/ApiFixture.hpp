#pragma once

#include <gtest/gtest.h>
#include <httplib.h>

#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "TempFile.hpp"
#include "TestClock.hpp"
#include "api/HealthController.hpp"
#include "api/InternalQuotaController.hpp"
#include "api/ServerSetup.hpp"
#include "api/TracedRouter.hpp"
#include "api/UsersController.hpp"
#include "auth/Argon2PasswordHasher.hpp"
#include "auth/UuidV4Generator.hpp"
#include "service/QuotaService.hpp"
#include "telemetry/ActivityTracker.hpp"
#include "tecflix/telemetry/IHttpServerMetrics.hpp"
#include "service/UserService.hpp"
#include "storage/BTree.hpp"
#include "storage/FileReservationRepository.hpp"
#include "storage/FileUserRepository.hpp"
#include "storage/LockedUserRepository.hpp"
#include "storage/MemoryPageStore.hpp"
#include "storage/UserFile.hpp"
#include "tecflix/auth/JwtSigner.hpp"
#include "tecflix/auth/JwtVerifier.hpp"

namespace tecflix::users::testing {

inline const std::string kTestSecret = "un-secreto-de-prueba-con-mas-de-32-bytes!";
inline const std::string kTestServiceToken = "token-de-servicio-de-prueba-de-32-bytes-o-mas";
inline constexpr std::uint64_t kTestQuota = 5'000'000'000ULL;
inline constexpr std::chrono::seconds kTestStart{1'800'000'000};
inline constexpr std::chrono::seconds kTestTokenLifetime{3600};
inline constexpr std::chrono::seconds kTestReservationLifetime{7200};

// Servidor real en un puerto libre del equipo, con todas las piezas conectadas igual que en main.
class ApiFixture : public ::testing::Test {
protected:
    using Json = nlohmann::json;

    struct Reply {
        int status;
        Json body;
    };

    ApiFixture()
        : usersTemp_(std::string{"api_users_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".dat"),
          reservationsTemp_(std::string{"api_res_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".dat"),
          usersFile_(usersTemp_.path()),
          userIndex_(userPages_),
          fileUsers_(usersFile_, userIndex_),
          users_(fileUsers_),
          reservationIndex_(reservationPages_),
          reservations_(reservationsTemp_.path(), reservationIndex_),
          hasher_(Argon2Params::minimumForTests()),
          signer_(kTestSecret, clock_, kTestTokenLifetime),
          verifier_(kTestSecret, clock_),
          userService_(users_, hasher_, ids_, signer_, kTestQuota),
          quotaService_(users_, reservations_, clock_, kTestReservationLifetime),
          activity_(clock_),
          router_(server_, httpMetrics_, ids_, "users-api"),
          usersController_(userService_, verifier_, ids_, activity_, kTestTokenLifetime),
          quotaController_(quotaService_, kTestServiceToken, ids_) {
        configureServer(server_, ids_);
        health_.registerRoutes(server_);
        usersController_.registerRoutes(router_);
        quotaController_.registerRoutes(router_);
        port_ = server_.bind_to_any_port(kHost);
        thread_ = std::thread([this] { server_.listen_after_bind(); });
    }

    ~ApiFixture() override {
        server_.stop();
        thread_.join();
    }

    ApiFixture(const ApiFixture&) = delete;
    ApiFixture& operator=(const ApiFixture&) = delete;
    ApiFixture(ApiFixture&&) = delete;
    ApiFixture& operator=(ApiFixture&&) = delete;

    static Reply reply(const httplib::Result& result) {
        EXPECT_TRUE(result) << "el servidor no respondió";
        if (!result) {
            return {0, Json{}};
        }
        Json body = result->body.empty() ? Json{} : Json::parse(result->body, nullptr, false);
        return {result->status, std::move(body)};
    }

    static httplib::Headers bearer(const std::string& token) { return {{"Authorization", "Bearer " + token}}; }
    static httplib::Headers serviceAuth() { return bearer(kTestServiceToken); }

    Reply post(const std::string& path, const Json& body, const httplib::Headers& headers = {}) {
        httplib::Client client(kHost, port_);
        return reply(client.Post("/api/v1" + path, headers, body.dump(), kJson));
    }
    Reply get(const std::string& path, const httplib::Headers& headers = {}) {
        httplib::Client client(kHost, port_);
        return reply(client.Get("/api/v1" + path, headers));
    }
    Reply patch(const std::string& path, const Json& body, const httplib::Headers& headers = {}) {
        httplib::Client client(kHost, port_);
        return reply(client.Patch("/api/v1" + path, headers, body.dump(), kJson));
    }
    Reply del(const std::string& path, const httplib::Headers& headers = {}) {
        httplib::Client client(kHost, port_);
        return reply(client.Delete("/api/v1" + path, headers));
    }
    Reply postEmpty(const std::string& path, const httplib::Headers& headers = {}) {
        httplib::Client client(kHost, port_);
        return reply(client.Post("/api/v1" + path, headers, "", kJson));
    }

    // Registra e inicia sesión; devuelve el token.
    std::string registerAndLogin(const std::string& username, const std::string& email) {
        EXPECT_EQ(post("/users", {{"username", username}, {"email", email}, {"password", "contrasena-valida"}}).status, 201);
        const Reply login = post("/auth/login", {{"username", username}, {"password", "contrasena-valida"}});
        EXPECT_EQ(login.status, 200);
        return login.body.value("accessToken", "");
    }

    std::string idOf(const std::string& token) { return get("/users/me", bearer(token)).body.value("id", ""); }

    static constexpr const char* kJson = "application/json";
    static constexpr const char* kHost = "127.0.0.1";

    TempFile usersTemp_;
    TempFile reservationsTemp_;
    UserFile usersFile_;
    MemoryPageStore userPages_;
    BTree userIndex_;
    FileUserRepository fileUsers_;
    LockedUserRepository users_;
    MemoryPageStore reservationPages_;
    BTree reservationIndex_;
    FileReservationRepository reservations_;
    Argon2PasswordHasher hasher_;
    TestClock clock_{kTestStart};
    UuidV4Generator ids_;
    tecflix::auth::JwtSigner signer_;
    tecflix::auth::JwtVerifier verifier_;
    UserService userService_;
    QuotaService quotaService_;
    ActivityTracker activity_;
    tecflix::telemetry::NullHttpServerMetrics httpMetrics_;
    TracedRouter router_;
    UsersController usersController_;
    InternalQuotaController quotaController_;
    HealthController health_;
    httplib::Server server_;
    int port_ = 0;
    std::thread thread_;
};

}  // namespace tecflix::users::testing
