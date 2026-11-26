#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "ApiFixture.hpp"

namespace tecflix::users {

using nlohmann::json;
using testing::kTestQuota;
using testing::kTestTokenLifetime;

namespace {

constexpr std::uint64_t kQuota = kTestQuota;
constexpr std::chrono::seconds kLifetime = kTestTokenLifetime;

class UsersControllerTest : public testing::ApiFixture {};

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
