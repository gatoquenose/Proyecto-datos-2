#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "TempFile.hpp"
#include "TestClock.hpp"
#include "auth/Argon2PasswordHasher.hpp"
#include "domain/IIdGenerator.hpp"
#include "service/UserService.hpp"
#include "storage/BTree.hpp"
#include "storage/FileUserRepository.hpp"
#include "storage/MemoryPageStore.hpp"
#include "storage/UserFile.hpp"
#include "tecflix/auth/JwtVerifier.hpp"

namespace tecflix::users {

using testing::TempFile;

namespace {

const std::string kSecret = "un-secreto-de-prueba-con-mas-de-32-bytes!";
constexpr std::uint64_t kDefaultQuota = 5'000'000'000ULL;
constexpr std::chrono::seconds kStart{1'800'000'000};
constexpr std::chrono::seconds kLifetime{3600};

// Ids predecibles: id-1, id-2, ...
class CountingIds final : public IIdGenerator {
public:
    [[nodiscard]] std::string next() const override { return "id-" + std::to_string(++counter_); }

private:
    mutable int counter_ = 0;
};

class UserServiceTest : public ::testing::Test {
protected:
    UserServiceTest()
        : temp_(std::string{"service_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name() +
                ".dat"),
          file_(temp_.path()),
          index_(pages_),
          repository_(file_, index_),
          hasher_(Argon2Params::minimumForTests()),
          signer_(kSecret, clock_, kLifetime),
          verifier_(kSecret, clock_),
          service_(repository_, hasher_, ids_, signer_, kDefaultQuota) {}

    static RegisterRequest request(const std::string& username = "alice", const std::string& email = "alice@tec.cr",
                                   const std::string& password = "contrasena-valida") {
        return {username, email, password};
    }

    void registerDefaultUser() { static_cast<void>(service_.registerUser(request())); }

    TempFile temp_;
    UserFile file_;
    MemoryPageStore pages_;
    BTree index_;
    FileUserRepository repository_;
    Argon2PasswordHasher hasher_;
    testing::TestClock clock_{kStart};
    CountingIds ids_;
    tecflix::auth::JwtSigner signer_;
    tecflix::auth::JwtVerifier verifier_;
    UserService service_;
};

}  // namespace

TEST_F(UserServiceTest, RegistersAUserWithTheDefaultQuota) {
    const RegisterOutcome outcome = service_.registerUser(request());

    ASSERT_EQ(outcome.status, RegisterStatus::Created);
    EXPECT_EQ(outcome.user.id, "id-1");
    EXPECT_EQ(outcome.user.quotaBytes, kDefaultQuota);
    EXPECT_EQ(outcome.user.usedBytes, 0U);
}

TEST_F(UserServiceTest, StoresAnArgon2HashNeverThePassword) {
    registerDefaultUser();

    const auto stored = repository_.findByUsername("alice");

    ASSERT_TRUE(stored.has_value());
    EXPECT_TRUE(stored->passwordHash.starts_with("$argon2id$"));
    EXPECT_EQ(stored->passwordHash.find("contrasena-valida"), std::string::npos);
}

TEST_F(UserServiceTest, RejectsInvalidInputWithoutStoringAnything) {
    EXPECT_EQ(service_.registerUser(request("ab")).status, RegisterStatus::InvalidUsername);
    EXPECT_EQ(service_.registerUser(request("alice", "no-es-email")).status, RegisterStatus::InvalidEmail);
    EXPECT_EQ(service_.registerUser(request("alice", "alice@tec.cr", "corta")).status,
              RegisterStatus::InvalidPassword);

    EXPECT_EQ(file_.recordCount(), 0U);
}

TEST_F(UserServiceTest, RejectsDuplicateUsernameAndEmail) {
    registerDefaultUser();

    EXPECT_EQ(service_.registerUser(request("ALICE", "otro@tec.cr")).status, RegisterStatus::UsernameTaken);
    EXPECT_EQ(service_.registerUser(request("otra", "Alice@TEC.cr")).status, RegisterStatus::EmailTaken);
}

TEST_F(UserServiceTest, LoginReturnsATokenThatTheVerifierAccepts) {
    registerDefaultUser();

    const LoginOutcome outcome = service_.login("alice", "contrasena-valida");

    ASSERT_TRUE(outcome.success);
    const auto verified = verifier_.verify(outcome.token);
    ASSERT_EQ(verified.error, tecflix::auth::TokenError::None);
    EXPECT_EQ(verified.claims.subject, "id-1");
    EXPECT_EQ(verified.claims.username, "alice");
    EXPECT_EQ(outcome.user.email, "alice@tec.cr");
}

TEST_F(UserServiceTest, LoginIgnoresCaseInTheUsername) {
    registerDefaultUser();

    EXPECT_TRUE(service_.login("ALICE", "contrasena-valida").success);
}

TEST_F(UserServiceTest, LoginFailsWithWrongPasswordOrUnknownUser) {
    registerDefaultUser();

    EXPECT_FALSE(service_.login("alice", "contrasena-incorrecta").success);
    EXPECT_FALSE(service_.login("nadie", "contrasena-valida").success);
    EXPECT_TRUE(service_.login("nadie", "contrasena-valida").token.empty());
}

TEST_F(UserServiceTest, RemovedUserCannotLogIn) {
    registerDefaultUser();
    repository_.remove("id-1");

    EXPECT_FALSE(service_.login("alice", "contrasena-valida").success);
}

TEST_F(UserServiceTest, ChangeEmailUpdatesTheEmailAndKeepsTheRest) {
    registerDefaultUser();

    const ChangeEmailOutcome outcome = service_.changeEmail("id-1", "nuevo@tec.cr");

    ASSERT_EQ(outcome.status, ChangeEmailStatus::Changed);
    EXPECT_EQ(outcome.user.email, "nuevo@tec.cr");
    EXPECT_EQ(outcome.user.username, "alice");
    EXPECT_EQ(outcome.user.quotaBytes, kDefaultQuota);
    EXPECT_EQ(service_.findByEmail("nuevo@tec.cr")->id, "id-1");
    EXPECT_FALSE(service_.findByEmail("alice@tec.cr").has_value());
}

TEST_F(UserServiceTest, ChangeEmailRejectsInvalidTakenAndUnknown) {
    registerDefaultUser();
    static_cast<void>(service_.registerUser(request("bob", "bob@tec.cr")));

    EXPECT_EQ(service_.changeEmail("id-1", "no-es-email").status, ChangeEmailStatus::InvalidEmail);
    EXPECT_EQ(service_.changeEmail("id-1", "bob@tec.cr").status, ChangeEmailStatus::EmailTaken);
    EXPECT_EQ(service_.changeEmail("id-99", "x@tec.cr").status, ChangeEmailStatus::NotFound);
}

TEST_F(UserServiceTest, RemoveUserReportsWhetherTheUserExisted) {
    registerDefaultUser();

    EXPECT_TRUE(service_.removeUser("id-1"));
    EXPECT_FALSE(service_.removeUser("id-1"));
    EXPECT_FALSE(service_.findById("id-1").has_value());
}

}  // namespace tecflix::users
