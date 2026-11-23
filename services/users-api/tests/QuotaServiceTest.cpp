#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

#include "TempFile.hpp"
#include "TestClock.hpp"
#include "service/QuotaService.hpp"
#include "service/ReservationSweeper.hpp"
#include "storage/BTree.hpp"
#include "storage/FileReservationRepository.hpp"
#include "storage/FileUserRepository.hpp"
#include "storage/LockedUserRepository.hpp"
#include "storage/MemoryPageStore.hpp"
#include "storage/UserFile.hpp"

namespace tecflix::users {

using testing::TempFile;
using testing::TestClock;
using std::chrono::seconds;

namespace {

constexpr std::uint64_t kQuota = 1000;
constexpr seconds kStart{1'800'000'000};
constexpr seconds kLifetime{3600};

class QuotaServiceTest : public ::testing::Test {
protected:
    QuotaServiceTest()
        : usersTemp_(std::string{"quota_users_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name() +
                     ".dat"),
          reservationsTemp_(std::string{"quota_res_"} + ::testing::UnitTest::GetInstance()->current_test_info()->name() +
                            ".dat"),
          usersFile_(usersTemp_.path()),
          userIndex_(userPages_),
          fileUsers_(usersFile_, userIndex_),
          users_(fileUsers_),
          reservationIndex_(reservationPages_),
          reservations_(reservationsTemp_.path(), reservationIndex_),
          quota_(users_, reservations_, clock_, kLifetime) {
        users_.add(User{"user-1", "alice", "alice@tec.cr", "hash", kQuota, 0});
        users_.add(User{"user-2", "bob", "bob@tec.cr", "hash", kQuota, 0});
    }

    [[nodiscard]] ReserveOutcome reserve(const std::string& id, std::uint64_t bytes, const std::string& user = "user-1",
                                         std::optional<std::int64_t> expiresAt = std::nullopt) {
        return quota_.reserve({id, user, bytes, expiresAt});
    }
    [[nodiscard]] std::uint64_t used(const std::string& user = "user-1") const {
        return users_.findById(user)->usedBytes;
    }

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
    TestClock clock_{kStart};
    QuotaService quota_;
};

}  // namespace

TEST_F(QuotaServiceTest, ReserveCountsTheBytesAsUsed) {
    const ReserveOutcome outcome = reserve("res-1", 300);

    ASSERT_EQ(outcome.status, ReserveStatus::Created);
    EXPECT_EQ(outcome.reservation.status, ReservationStatus::Active);
    EXPECT_EQ(outcome.reservation.expiresAt, (kStart + kLifetime).count());
    EXPECT_EQ(used(), 300U);
}

TEST_F(QuotaServiceTest, ReserveIsRejectedWhenTheQuotaDoesNotFit) {
    ASSERT_EQ(reserve("res-1", 600).status, ReserveStatus::Created);

    EXPECT_EQ(reserve("res-2", 401).status, ReserveStatus::QuotaExceeded);
    EXPECT_EQ(used(), 600U);
    EXPECT_EQ(reserve("res-3", 400).status, ReserveStatus::Created);  // justo lo que queda
    EXPECT_EQ(used(), 1000U);
}

TEST_F(QuotaServiceTest, ReserveRejectsUnknownUsersAndPastExpirations) {
    EXPECT_EQ(reserve("res-1", 10, "nadie").status, ReserveStatus::UserNotFound);
    EXPECT_EQ(reserve("res-1", 10, "user-1", kStart.count()).status, ReserveStatus::InvalidExpiry);
    EXPECT_EQ(reserve("res-1", 10, "user-1", kStart.count() + 60).status, ReserveStatus::Created);
}

TEST_F(QuotaServiceTest, ReserveIsIdempotentForTheSameRequest) {
    ASSERT_EQ(reserve("res-1", 300).status, ReserveStatus::Created);

    const ReserveOutcome again = reserve("res-1", 300);

    EXPECT_EQ(again.status, ReserveStatus::AlreadyReserved);
    EXPECT_EQ(again.reservation.bytes, 300U);
    EXPECT_EQ(used(), 300U);  // no se cobró dos veces
}

TEST_F(QuotaServiceTest, ReusedIdWithOtherDataIsAConflict) {
    ASSERT_EQ(reserve("res-1", 300).status, ReserveStatus::Created);

    EXPECT_EQ(reserve("res-1", 301).status, ReserveStatus::Conflict);
    EXPECT_EQ(reserve("res-1", 300, "user-2").status, ReserveStatus::Conflict);
    EXPECT_EQ(used(), 300U);
    EXPECT_EQ(used("user-2"), 0U);
}

TEST_F(QuotaServiceTest, SettleBelowTheReservedSizeGivesBackTheDifference) {
    ASSERT_EQ(reserve("res-1", 500).status, ReserveStatus::Created);

    const SettleOutcome outcome = quota_.settle("res-1", 200);

    ASSERT_EQ(outcome.status, SettleStatus::Settled);
    EXPECT_EQ(outcome.reservation.bytes, 200U);
    EXPECT_EQ(outcome.reservation.status, ReservationStatus::Settled);
    EXPECT_EQ(used(), 200U);
}

TEST_F(QuotaServiceTest, SettleAboveTheReservedSizeChargesTheExtraIfItFits) {
    ASSERT_EQ(reserve("res-1", 500).status, ReserveStatus::Created);

    EXPECT_EQ(quota_.settle("res-1", 1001).status, SettleStatus::QuotaExceeded);
    EXPECT_EQ(used(), 500U);
    EXPECT_EQ(quota_.settle("res-1", 900).status, SettleStatus::Settled);
    EXPECT_EQ(used(), 900U);
}

TEST_F(QuotaServiceTest, SettleIsIdempotentOnlyForTheSameSize) {
    ASSERT_EQ(reserve("res-1", 500).status, ReserveStatus::Created);
    ASSERT_EQ(quota_.settle("res-1", 200).status, SettleStatus::Settled);

    EXPECT_EQ(quota_.settle("res-1", 200).status, SettleStatus::AlreadySettled);
    EXPECT_EQ(quota_.settle("res-1", 250).status, SettleStatus::InvalidState);
    EXPECT_EQ(used(), 200U);
}

TEST_F(QuotaServiceTest, SettleOfUnknownReservationIsNotFound) {
    EXPECT_EQ(quota_.settle("nada", 1).status, SettleStatus::NotFound);
}

TEST_F(QuotaServiceTest, ReleaseGivesTheSpaceBackAndIsIdempotent) {
    ASSERT_EQ(reserve("res-1", 400).status, ReserveStatus::Created);

    EXPECT_EQ(quota_.release("res-1"), ReleaseStatus::Released);
    EXPECT_EQ(used(), 0U);
    EXPECT_EQ(quota_.release("res-1"), ReleaseStatus::AlreadyReleased);
    EXPECT_EQ(used(), 0U);  // no se devolvió dos veces
    EXPECT_EQ(quota_.release("nada"), ReleaseStatus::NotFound);
}

TEST_F(QuotaServiceTest, ReleaseOfAPublishedVideoRefundsItsSize) {
    ASSERT_EQ(reserve("res-1", 500).status, ReserveStatus::Created);
    ASSERT_EQ(quota_.settle("res-1", 300).status, SettleStatus::Settled);

    EXPECT_EQ(quota_.release("res-1"), ReleaseStatus::Released);

    EXPECT_EQ(used(), 0U);
    EXPECT_EQ(quota_.settle("res-1", 300).status, SettleStatus::InvalidState);
}

TEST_F(QuotaServiceTest, SpaceCanBeReservedAgainAfterRelease) {
    ASSERT_EQ(reserve("res-1", 1000).status, ReserveStatus::Created);
    ASSERT_EQ(reserve("res-2", 1).status, ReserveStatus::QuotaExceeded);

    static_cast<void>(quota_.release("res-1"));

    EXPECT_EQ(reserve("res-2", 1000).status, ReserveStatus::Created);
}

TEST_F(QuotaServiceTest, AbandonedReservationsExpireAndReturnTheirSpace) {
    ASSERT_EQ(reserve("abandoned", 400).status, ReserveStatus::Created);
    ASSERT_EQ(reserve("long", 100, "user-1", (kStart + seconds{7200}).count()).status, ReserveStatus::Created);

    clock_.advance(kLifetime);
    EXPECT_EQ(quota_.expireDueReservations(), 1U);

    EXPECT_EQ(used(), 100U);
    EXPECT_EQ(quota_.expireDueReservations(), 0U);  // ya no queda nada por vencer
    EXPECT_EQ(quota_.settle("abandoned", 400).status, SettleStatus::InvalidState);
    EXPECT_EQ(quota_.release("abandoned"), ReleaseStatus::AlreadyReleased);
    EXPECT_EQ(used(), 100U);
}

TEST_F(QuotaServiceTest, ReservationThatExpiredBeforeTheSweepCannotBeSettled) {
    ASSERT_EQ(reserve("res-1", 400).status, ReserveStatus::Created);

    clock_.advance(kLifetime + seconds{1});  // sin que corra el barrido

    EXPECT_EQ(quota_.settle("res-1", 400).status, SettleStatus::InvalidState);
    EXPECT_EQ(used(), 0U);
}

TEST_F(QuotaServiceTest, SettledReservationsNeverExpire) {
    ASSERT_EQ(reserve("res-1", 400).status, ReserveStatus::Created);
    ASSERT_EQ(quota_.settle("res-1", 400).status, SettleStatus::Settled);

    clock_.advance(seconds{10 * 24 * 3600});

    EXPECT_EQ(quota_.expireDueReservations(), 0U);
    EXPECT_EQ(used(), 400U);
}

TEST_F(QuotaServiceTest, ManyThreadsReservingNeverExceedTheQuota) {
    constexpr int kThreads = 8;
    std::vector<std::thread> workers;
    for (int worker = 0; worker < kThreads; ++worker) {
        workers.emplace_back([this, worker] {
            for (int i = 0; i < 20; ++i) {
                static_cast<void>(reserve("res-" + std::to_string(worker) + "-" + std::to_string(i), 10));
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_EQ(used(), kQuota);  // 160 intentos de 10 bytes: caben exactamente 100
}

TEST_F(QuotaServiceTest, SweeperExpiresReservationsByItself) {
    ASSERT_EQ(reserve("abandoned", 400).status, ReserveStatus::Created);
    clock_.advance(kLifetime);

    {
        const ReservationSweeper sweeper{quota_, std::chrono::milliseconds{10}};
        for (int attempt = 0; attempt < 200 && used() != 0; ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
    }  // el destructor detiene el hilo

    EXPECT_EQ(used(), 0U);
}

}  // namespace tecflix::users
