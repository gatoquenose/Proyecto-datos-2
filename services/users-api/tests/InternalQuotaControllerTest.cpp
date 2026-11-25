#include <gtest/gtest.h>

#include <string>

#include "ApiFixture.hpp"
#include "util/IsoTime.hpp"

namespace tecflix::users {

using nlohmann::json;
using testing::kTestQuota;
using testing::kTestReservationLifetime;
using testing::kTestStart;

namespace {

const std::string kReservationId = "11111111-2222-4333-8444-555555555555";
const std::string kOtherReservationId = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";
constexpr const char* kBase = "/internal/quota/reservations";

class InternalQuotaControllerTest : public testing::ApiFixture {
protected:
    struct Account {
        std::string token;
        std::string id;
    };

    Account newAccount(const std::string& name = "alice") {
        const std::string token = registerAndLogin(name, name + "@tec.cr");
        return {token, idOf(token)};
    }

    std::string loginAgain(const std::string& name) {
        return post("/auth/login", {{"username", name}, {"password", "contrasena-valida"}}).body["accessToken"];
    }

    Reply reserve(const std::string& reservationId, const std::string& userId, std::uint64_t bytes,
                  const httplib::Headers& headers = serviceAuth()) {
        return post(kBase, {{"reservationId", reservationId}, {"userId", userId}, {"bytes", bytes}}, headers);
    }
    Reply settle(const std::string& reservationId, std::uint64_t finalBytes, const httplib::Headers& headers = serviceAuth()) {
        return post(std::string{kBase} + "/" + reservationId + "/settle", {{"finalBytes", finalBytes}}, headers);
    }
    Reply release(const std::string& reservationId, const httplib::Headers& headers = serviceAuth()) {
        return postEmpty(std::string{kBase} + "/" + reservationId + "/release", headers);
    }
    std::uint64_t usedBy(const Account& account) {
        return get("/users/me/quota", bearer(account.token)).body["usedBytes"].get<std::uint64_t>();
    }
};

}  // namespace

TEST_F(InternalQuotaControllerTest, ReserveCreatesAReservationAndTheUserSeesItAsUsed) {
    const Account alice = newAccount();

    const Reply result = reserve(kReservationId, alice.id, 1000);

    EXPECT_EQ(result.status, 201);
    EXPECT_EQ(result.body["reservationId"], kReservationId);
    EXPECT_EQ(result.body["userId"], alice.id);
    EXPECT_EQ(result.body["bytes"], 1000);
    EXPECT_EQ(result.body["status"], "active");
    EXPECT_EQ(result.body["expiresAt"], formatIsoUtc((kTestStart + kTestReservationLifetime).count()));
    EXPECT_EQ(usedBy(alice), 1000U);
    EXPECT_EQ(get("/users/me/quota", bearer(alice.token)).body["remainingBytes"], kTestQuota - 1000);
}

TEST_F(InternalQuotaControllerTest, ReserveIsIdempotent) {
    const Account alice = newAccount();
    ASSERT_EQ(reserve(kReservationId, alice.id, 1000).status, 201);

    const Reply again = reserve(kReservationId, alice.id, 1000);

    EXPECT_EQ(again.status, 200);
    EXPECT_EQ(again.body["status"], "active");
    EXPECT_EQ(usedBy(alice), 1000U);
    EXPECT_EQ(reserve(kReservationId, alice.id, 2000).status, 409);
}

TEST_F(InternalQuotaControllerTest, ReserveBeyondTheQuotaIsForbidden) {
    const Account alice = newAccount();

    const Reply result = reserve(kReservationId, alice.id, kTestQuota + 1);

    EXPECT_EQ(result.status, 403);
    EXPECT_EQ(result.body["code"], "QUOTA_EXCEEDED");
    EXPECT_EQ(usedBy(alice), 0U);
}

TEST_F(InternalQuotaControllerTest, ReserveValidatesItsInput) {
    const Account alice = newAccount();

    EXPECT_EQ(reserve("no-es-uuid", alice.id, 1).status, 400);
    EXPECT_EQ(reserve(kReservationId, "no-es-uuid", 1).status, 400);
    EXPECT_EQ(post(kBase, {{"reservationId", kReservationId}, {"userId", alice.id}, {"bytes", -5}}, serviceAuth()).status, 400);
    EXPECT_EQ(post(kBase, {{"reservationId", kReservationId}, {"userId", alice.id}}, serviceAuth()).status, 400);
    EXPECT_EQ(post(kBase, {{"reservationId", kReservationId}, {"userId", alice.id}, {"bytes", 1}, {"expiresAt", "ayer"}},
                   serviceAuth()).status, 400);
    EXPECT_EQ(post(kBase, {{"reservationId", kReservationId}, {"userId", alice.id}, {"bytes", 1},
                           {"expiresAt", formatIsoUtc(kTestStart.count() - 10)}}, serviceAuth()).status, 400);
    EXPECT_EQ(reserve(kReservationId, "99999999-9999-4999-8999-999999999999", 1).status, 404);
}

TEST_F(InternalQuotaControllerTest, ReserveAcceptsACustomExpiration) {
    const Account alice = newAccount();
    const std::string expiresAt = formatIsoUtc(kTestStart.count() + 600);

    const Reply result = post(kBase, {{"reservationId", kReservationId}, {"userId", alice.id}, {"bytes", 10},
                                      {"expiresAt", expiresAt}}, serviceAuth());

    EXPECT_EQ(result.status, 201);
    EXPECT_EQ(result.body["expiresAt"], expiresAt);
}

TEST_F(InternalQuotaControllerTest, SettleAdjustsTheUsedSpace) {
    const Account alice = newAccount();
    ASSERT_EQ(reserve(kReservationId, alice.id, 1000).status, 201);

    const Reply result = settle(kReservationId, 400);

    EXPECT_EQ(result.status, 200);
    EXPECT_EQ(result.body["status"], "settled");
    EXPECT_EQ(result.body["bytes"], 400);
    EXPECT_EQ(usedBy(alice), 400U);
    EXPECT_EQ(settle(kReservationId, 400).status, 200);  // idempotente
    EXPECT_EQ(settle(kReservationId, 500).status, 409);
    EXPECT_EQ(settle(kReservationId, 500).body["code"], "INVALID_STATE");
}

TEST_F(InternalQuotaControllerTest, SettleAboveTheQuotaIsForbidden) {
    const Account alice = newAccount();
    ASSERT_EQ(reserve(kReservationId, alice.id, 1000).status, 201);

    const Reply result = settle(kReservationId, kTestQuota + 1);

    EXPECT_EQ(result.status, 403);
    EXPECT_EQ(result.body["code"], "QUOTA_EXCEEDED");
    EXPECT_EQ(post(std::string{kBase} + "/" + kReservationId + "/settle", {{"finalBytes", -1}}, serviceAuth()).status, 400);
    EXPECT_EQ(settle(kOtherReservationId, 1).status, 404);
}

TEST_F(InternalQuotaControllerTest, ReleaseGivesTheSpaceBack) {
    const Account alice = newAccount();
    ASSERT_EQ(reserve(kReservationId, alice.id, 1000).status, 201);
    ASSERT_EQ(settle(kReservationId, 600).status, 200);

    EXPECT_EQ(release(kReservationId).status, 204);

    EXPECT_EQ(usedBy(alice), 0U);
    EXPECT_EQ(release(kReservationId).status, 204);  // idempotente
    EXPECT_EQ(usedBy(alice), 0U);
    EXPECT_EQ(release(kOtherReservationId).status, 404);
}

TEST_F(InternalQuotaControllerTest, ExpiredReservationsReturnTheirSpace) {
    const Account alice = newAccount();
    ASSERT_EQ(reserve(kReservationId, alice.id, 1000).status, 201);

    clock_.advance(kTestReservationLifetime);
    quotaService_.expireDueReservations();

    // El token de login ya venció con el paso del tiempo: se inicia sesión de nuevo.
    const Account later{loginAgain("alice"), alice.id};
    EXPECT_EQ(usedBy(later), 0U);
    EXPECT_EQ(settle(kReservationId, 1000).status, 409);
}

TEST_F(InternalQuotaControllerTest, EveryEndpointNeedsTheServiceToken) {
    const Account alice = newAccount();

    EXPECT_EQ(reserve(kReservationId, alice.id, 1, {}).status, 401);
    EXPECT_EQ(reserve(kReservationId, alice.id, 1, {}).body["code"], "MISSING_TOKEN");
    EXPECT_EQ(reserve(kReservationId, alice.id, 1, bearer("otro-token")).body["code"], "INVALID_TOKEN");
    EXPECT_EQ(settle(kReservationId, 1, {}).status, 401);
    EXPECT_EQ(release(kReservationId, {}).status, 401);
    EXPECT_EQ(usedBy(alice), 0U);
}

TEST_F(InternalQuotaControllerTest, AUserTokenIsNotAServiceToken) {
    const Account alice = newAccount();

    const Reply result = reserve(kReservationId, alice.id, 1, bearer(alice.token));

    EXPECT_EQ(result.status, 401);
    EXPECT_EQ(result.body["code"], "INVALID_TOKEN");
}

TEST_F(InternalQuotaControllerTest, ServiceTokenDoesNotOpenUserEndpoints) {
    EXPECT_EQ(get("/users/me", serviceAuth()).status, 401);
}

TEST_F(InternalQuotaControllerTest, ReadyReportsTheServiceIsReady) {
    const Reply result = get("/ready");

    EXPECT_EQ(result.status, 200);
    EXPECT_EQ(result.body["status"], "ready");
}

}  // namespace tecflix::users
