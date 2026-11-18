#include <gtest/gtest.h>

#include <httplib.h>

#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "TempFile.hpp"
#include "TestClock.hpp"
#include "api/HealthController.hpp"
#include "api/ServerSetup.hpp"
#include "api/UsersController.hpp"
#include "auth/Argon2PasswordHasher.hpp"
#include "auth/UuidV4Generator.hpp"
#include "service/UserService.hpp"
#include "storage/BTree.hpp"
#include "storage/FileUserRepository.hpp"
#include "storage/LockedUserRepository.hpp"
#include "storage/MemoryPageStore.hpp"
#include "storage/UserFile.hpp"
#include "tecflix/auth/JwtSigner.hpp"
#include "tecflix/auth/JwtVerifier.hpp"

namespace tecflix::users {

using nlohmann::json;
using testing::TempFile;
using testing::TestClock;

namespace {

const std::string kSecret = "un-secreto-de-prueba-con-mas-de-32-bytes!";
constexpr std::uint64_t kQuota = 5'000'000'000ULL;
constexpr std::chrono::seconds kStart{1'800'000'000};
constexpr std::chrono::seconds kLifetime{3600};
constexpr const char* kJson = "application/json";
constexpr const char* kHost = "127.0.0.1";

// Servidor real en un puerto libre del equipo, con todas las piezas conectadas igual que en main.
class UsersControllerTest : public ::testing::Test {
protected:
    UsersControllerTest()
        : temp_(std::string{"controller_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".dat"),
          file_(temp_.path()),
          index_(pages_),
          fileRepository_(file_, index_),
          repository_(fileRepository_),
          hasher_(Argon2Params::minimumForTests()),
          signer_(kSecret, clock_, kLifetime),
          verifier_(kSecret, clock_),
          service_(repository_, hasher_, ids_, signer_, kQuota),
          controller_(service_, verifier_, ids_, kLifetime) {
        configureServer(server_, ids_);
        health_.registerRoutes(server_);
        controller_.registerRoutes(server_);
        port_ = server_.bind_to_any_port(kHost);
        thread_ = std::thread([this] { server_.listen_after_bind(); });
    }

    ~UsersControllerTest() override {
        server_.stop();
        thread_.join();
    }

    struct Reply {
        int status;
        json body;
    };

    static Reply reply(const httplib::Result& result) {
        EXPECT_TRUE(result) << "el servidor no respondió";
        if (!result) {
            return {0, json{}};
        }
        json body = result->body.empty() ? json{} : json::parse(result->body, nullptr, false);
        return {result->status, std::move(body)};
    }

    static httplib::Headers bearer(const std::string& token) { return {{"Authorization", "Bearer " + token}}; }

    Reply post(const std::string& path, const json& body, const httplib::Headers& headers = {}) {
        httplib::Client client(kHost, port_);
        return reply(client.Post("/api/v1" + path, headers, body.dump(), kJson));
    }
    Reply get(const std::string& path, const httplib::Headers& headers = {}) {
        httplib::Client client(kHost, port_);
        return reply(client.Get("/api/v1" + path, headers));
    }
    Reply patch(const std::string& path, const json& body, const httplib::Headers& headers = {}) {
        httplib::Client client(kHost, port_);
        return reply(client.Patch("/api/v1" + path, headers, body.dump(), kJson));
    }
    Reply del(const std::string& path, const httplib::Headers& headers = {}) {
        httplib::Client client(kHost, port_);
        return reply(client.Delete("/api/v1" + path, headers));
    }

    // Registra e inicia sesión; devuelve el token.
    std::string registerAndLogin(const std::string& username, const std::string& email) {
        EXPECT_EQ(post("/users", {{"username", username}, {"email", email}, {"password", "contrasena-valida"}}).status, 201);
        const Reply login = post("/auth/login", {{"username", username}, {"password", "contrasena-valida"}});
        EXPECT_EQ(login.status, 200);
        return login.body.value("accessToken", "");
    }

    std::string idOf(const std::string& token) { return get("/users/me", bearer(token)).body.value("id", ""); }

    TempFile temp_;
    UserFile file_;
    MemoryPageStore pages_;
    BTree index_;
    FileUserRepository fileRepository_;
    LockedUserRepository repository_;
    Argon2PasswordHasher hasher_;
    TestClock clock_{kStart};
    UuidV4Generator ids_;
    tecflix::auth::JwtSigner signer_;
    tecflix::auth::JwtVerifier verifier_;
    UserService service_;
    UsersController controller_;
    HealthController health_;
    httplib::Server server_;
    int port_ = 0;
    std::thread thread_;
};

}  // namespace

TEST_F(UsersControllerTest, HealthAnswersOk) {
    const Reply result = get("/health");

    EXPECT_EQ(result.status, 200);
    EXPECT_EQ(result.body["status"], "ok");
}

TEST_F(UsersControllerTest, RegisterCreatesTheUserAndNeverReturnsSecrets) {
    const Reply result = post("/users", {{"username", "alice"}, {"email", "alice@tec.cr"}, {"password", "contrasena-valida"}});

    EXPECT_EQ(result.status, 201);
    EXPECT_EQ(result.body["username"], "alice");
    EXPECT_FALSE(result.body["id"].get<std::string>().empty());
    EXPECT_FALSE(result.body.contains("password"));
    EXPECT_FALSE(result.body.contains("passwordHash"));
    EXPECT_FALSE(result.body.contains("email"));
}

TEST_F(UsersControllerTest, RegisterValidatesTheInput) {
    EXPECT_EQ(post("/users", {{"username", "ab"}, {"email", "a@tec.cr"}, {"password", "contrasena-valida"}}).body["code"],
              "VALIDATION_ERROR");
    EXPECT_EQ(post("/users", {{"username", "alice"}, {"email", "mal"}, {"password", "contrasena-valida"}}).status, 400);
    EXPECT_EQ(post("/users", {{"username", "alice"}, {"email", "a@tec.cr"}, {"password", "corta"}}).status, 400);
    const Reply missing = post("/users", {{"username", "alice"}});
    EXPECT_EQ(missing.status, 400);
    EXPECT_EQ(missing.body["code"], "VALIDATION_ERROR");
    EXPECT_FALSE(missing.body["traceId"].get<std::string>().empty());
}

TEST_F(UsersControllerTest, RegisterRejectsDuplicatesWithConflict) {
    registerAndLogin("alice", "alice@tec.cr");

    EXPECT_EQ(post("/users", {{"username", "ALICE"}, {"email", "otro@tec.cr"}, {"password", "contrasena-valida"}}).body["code"],
              "USERNAME_EXISTS");
    EXPECT_EQ(post("/users", {{"username", "otra"}, {"email", "alice@tec.cr"}, {"password", "contrasena-valida"}}).body["code"],
              "EMAIL_EXISTS");
}

TEST_F(UsersControllerTest, RegisterRejectsBodiesThatAreNotJson) {
    httplib::Client client(kHost, port_);

    const auto wrongType = client.Post("/api/v1/users", "username=a", "text/plain");
    const auto brokenJson = client.Post("/api/v1/users", "{no es json", kJson);

    ASSERT_TRUE(wrongType);
    EXPECT_EQ(wrongType->status, 415);
    EXPECT_EQ(json::parse(wrongType->body)["code"], "UNSUPPORTED_MEDIA_TYPE");
    ASSERT_TRUE(brokenJson);
    EXPECT_EQ(brokenJson->status, 400);
}

TEST_F(UsersControllerTest, LoginReturnsTokenAndTheUser) {
    post("/users", {{"username", "alice"}, {"email", "alice@tec.cr"}, {"password", "contrasena-valida"}});

    const Reply result = post("/auth/login", {{"username", "alice"}, {"password", "contrasena-valida"}});

    EXPECT_EQ(result.status, 200);
    EXPECT_EQ(result.body["tokenType"], "Bearer");
    EXPECT_EQ(result.body["expiresIn"], kLifetime.count());
    EXPECT_FALSE(result.body["accessToken"].get<std::string>().empty());
    EXPECT_EQ(result.body["user"]["email"], "alice@tec.cr");
    EXPECT_EQ(result.body["user"]["quotaBytes"], kQuota);
    EXPECT_EQ(result.body["user"]["usedBytes"], 0);
}

TEST_F(UsersControllerTest, LoginFailsWithTheSameErrorForWrongPasswordAndUnknownUser) {
    post("/users", {{"username", "alice"}, {"email", "alice@tec.cr"}, {"password", "contrasena-valida"}});

    const Reply wrongPassword = post("/auth/login", {{"username", "alice"}, {"password", "incorrecta-123"}});
    const Reply unknownUser = post("/auth/login", {{"username", "nadie"}, {"password", "contrasena-valida"}});

    EXPECT_EQ(wrongPassword.status, 401);
    EXPECT_EQ(unknownUser.status, 401);
    EXPECT_EQ(wrongPassword.body["code"], "INVALID_CREDENTIALS");
    EXPECT_EQ(unknownUser.body["code"], wrongPassword.body["code"]);
    EXPECT_EQ(unknownUser.body["message"], wrongPassword.body["message"]);
}

TEST_F(UsersControllerTest, ProtectedEndpointsNeedAValidToken) {
    EXPECT_EQ(get("/users/me").body["code"], "MISSING_TOKEN");
    EXPECT_EQ(get("/users/me", {{"Authorization", "Basic abc"}}).body["code"], "MISSING_TOKEN");
    EXPECT_EQ(get("/users/me", bearer("esto.no.sirve")).body["code"], "INVALID_TOKEN");
    EXPECT_EQ(get("/users/me/quota").status, 401);
    EXPECT_EQ(get("/users/some-id").status, 401);
}

TEST_F(UsersControllerTest, ExpiredTokenIsReportedAsExpired) {
    const std::string token = registerAndLogin("alice", "alice@tec.cr");

    clock_.advance(kLifetime + std::chrono::seconds{1});

    const Reply result = get("/users/me", bearer(token));
    EXPECT_EQ(result.status, 401);
    EXPECT_EQ(result.body["code"], "TOKEN_EXPIRED");
}

TEST_F(UsersControllerTest, MeShowsTheOwnProfileAndQuota) {
    const std::string token = registerAndLogin("alice", "alice@tec.cr");

    const Reply me = get("/users/me", bearer(token));
    const Reply quota = get("/users/me/quota", bearer(token));

    EXPECT_EQ(me.status, 200);
    EXPECT_EQ(me.body["email"], "alice@tec.cr");
    EXPECT_FALSE(me.body.contains("passwordHash"));
    EXPECT_EQ(quota.status, 200);
    EXPECT_EQ(quota.body["quotaBytes"], kQuota);
    EXPECT_EQ(quota.body["usedBytes"], 0);
    EXPECT_EQ(quota.body["remainingBytes"], kQuota);
}

TEST_F(UsersControllerTest, OtherUsersOnlyShowIdAndUsername) {
    const std::string aliceToken = registerAndLogin("alice", "alice@tec.cr");
    const std::string bobToken = registerAndLogin("bob", "bob@tec.cr");
    const std::string aliceId = idOf(aliceToken);

    const Reply seenByBob = get("/users/" + aliceId, bearer(bobToken));
    const Reply seenByAlice = get("/users/" + aliceId, bearer(aliceToken));

    EXPECT_EQ(seenByBob.status, 200);
    EXPECT_EQ(seenByBob.body["username"], "alice");
    EXPECT_FALSE(seenByBob.body.contains("email"));
    EXPECT_FALSE(seenByBob.body.contains("quotaBytes"));
    EXPECT_EQ(seenByAlice.body["email"], "alice@tec.cr");
    EXPECT_EQ(get("/users/no-existe", bearer(bobToken)).status, 404);
    EXPECT_EQ(get("/users/no-existe", bearer(bobToken)).body["code"], "NOT_FOUND");
}

TEST_F(UsersControllerTest, UserCanChangeOnlyTheirEmail) {
    const std::string token = registerAndLogin("alice", "alice@tec.cr");

    const Reply changed = patch("/users/me", {{"email", "nuevo@tec.cr"}}, bearer(token));

    EXPECT_EQ(changed.status, 200);
    EXPECT_EQ(changed.body["email"], "nuevo@tec.cr");
    EXPECT_EQ(changed.body["username"], "alice");
    EXPECT_EQ(get("/users/me", bearer(token)).body["email"], "nuevo@tec.cr");
}

TEST_F(UsersControllerTest, UpdateRejectsOtherFieldsBadEmailsAndTakenEmails) {
    const std::string token = registerAndLogin("alice", "alice@tec.cr");
    registerAndLogin("bob", "bob@tec.cr");

    EXPECT_EQ(patch("/users/me", {{"username", "otro"}}, bearer(token)).status, 400);
    EXPECT_EQ(patch("/users/me", {{"quotaBytes", 999}}, bearer(token)).status, 400);
    EXPECT_EQ(patch("/users/me", {{"email", "nuevo@tec.cr"}, {"usedBytes", 0}}, bearer(token)).status, 400);
    EXPECT_EQ(patch("/users/me", {{"email", "mal"}}, bearer(token)).status, 400);
    EXPECT_EQ(patch("/users/me", {{"email", "bob@tec.cr"}}, bearer(token)).body["code"], "EMAIL_EXISTS");
    EXPECT_EQ(get("/users/me", bearer(token)).body["quotaBytes"], kQuota);
}

TEST_F(UsersControllerTest, UsersCannotModifyOrDeleteOthers) {
    const std::string aliceToken = registerAndLogin("alice", "alice@tec.cr");
    const std::string bobToken = registerAndLogin("bob", "bob@tec.cr");
    const std::string aliceId = idOf(aliceToken);

    const Reply patched = patch("/users/" + aliceId, {{"email", "robado@tec.cr"}}, bearer(bobToken));
    const Reply deleted = del("/users/" + aliceId, bearer(bobToken));

    EXPECT_EQ(patched.status, 403);
    EXPECT_EQ(patched.body["code"], "FORBIDDEN");
    EXPECT_EQ(deleted.status, 403);
    EXPECT_EQ(get("/users/me", bearer(aliceToken)).body["email"], "alice@tec.cr");
}

TEST_F(UsersControllerTest, UserCanChangeEmailThroughTheirOwnId) {
    const std::string token = registerAndLogin("alice", "alice@tec.cr");

    const Reply changed = patch("/users/" + idOf(token), {{"email", "nuevo@tec.cr"}}, bearer(token));

    EXPECT_EQ(changed.status, 200);
    EXPECT_EQ(changed.body["email"], "nuevo@tec.cr");
}

TEST_F(UsersControllerTest, DeletedUserCannotLogInNorUseTheirOldToken) {
    const std::string token = registerAndLogin("alice", "alice@tec.cr");

    const Reply deleted = del("/users/" + idOf(token), bearer(token));

    EXPECT_EQ(deleted.status, 204);
    EXPECT_EQ(post("/auth/login", {{"username", "alice"}, {"password", "contrasena-valida"}}).status, 401);
    EXPECT_EQ(get("/users/me", bearer(token)).status, 401);
    EXPECT_EQ(get("/users/me", bearer(token)).body["code"], "INVALID_TOKEN");
}

TEST_F(UsersControllerTest, SearchFindsUsersByExactUsername) {
    const std::string aliceToken = registerAndLogin("alice", "alice@tec.cr");
    const std::string bobToken = registerAndLogin("bob", "bob@tec.cr");

    const Reply found = get("/users?username=ALICE", bearer(bobToken));
    const Reply missing = get("/users?username=nadie", bearer(bobToken));

    EXPECT_EQ(found.status, 200);
    ASSERT_EQ(found.body["items"].size(), 1U);
    EXPECT_EQ(found.body["items"][0]["id"], idOf(aliceToken));
    EXPECT_FALSE(found.body["items"][0].contains("email"));
    EXPECT_TRUE(found.body["nextCursor"].is_null());
    EXPECT_EQ(missing.body["items"].size(), 0U);
}

TEST_F(UsersControllerTest, SearchByEmailOnlyRevealsTheCallersOwnAccount) {
    const std::string aliceToken = registerAndLogin("alice", "alice@tec.cr");
    const std::string bobToken = registerAndLogin("bob", "bob@tec.cr");

    const Reply others = get("/users?email=alice@tec.cr", bearer(bobToken));
    const Reply own = get("/users?email=alice@tec.cr", bearer(aliceToken));

    EXPECT_EQ(others.body["items"].size(), 0U);
    EXPECT_EQ(own.body["items"].size(), 1U);
}

TEST_F(UsersControllerTest, SearchValidatesItsParameters) {
    const std::string token = registerAndLogin("alice", "alice@tec.cr");

    EXPECT_EQ(get("/users", bearer(token)).status, 400);
    EXPECT_EQ(get("/users?username=alice&pageSize=0", bearer(token)).status, 400);
    EXPECT_EQ(get("/users?username=alice&pageSize=51", bearer(token)).status, 400);
    EXPECT_EQ(get("/users?username=alice&pageSize=abc", bearer(token)).status, 400);
    EXPECT_EQ(get("/users?username=alice&pageSize=50", bearer(token)).status, 200);
}

TEST_F(UsersControllerTest, UnknownRoutesAnswerWithTheCommonErrorFormat) {
    const Reply result = get("/ruta-que-no-existe");

    EXPECT_EQ(result.status, 404);
    EXPECT_EQ(result.body["code"], "NOT_FOUND");
    EXPECT_TRUE(result.body.contains("traceId"));
}

TEST_F(UsersControllerTest, ManyClientsCanRegisterAtTheSameTime) {
    constexpr int kClients = 6;
    std::vector<std::thread> clients;
    std::atomic<int> created{0};
    for (int i = 0; i < kClients; ++i) {
        clients.emplace_back([this, i, &created] {
            const std::string name = "user" + std::to_string(i);
            const Reply result = post("/users", {{"username", name}, {"email", name + "@tec.cr"}, {"password", "contrasena-valida"}});
            if (result.status == 201) {
                ++created;
            }
        });
    }
    for (auto& client : clients) {
        client.join();
    }

    EXPECT_EQ(created.load(), kClients);
}

}  // namespace tecflix::users
