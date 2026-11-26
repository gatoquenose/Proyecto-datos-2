#include <gtest/gtest.h>

#include <chrono>

#include "FakeMetrics.hpp"
#include "TempFile.hpp"
#include "TestClock.hpp"
#include "auth/UuidV4Generator.hpp"
#include "storage/BTree.hpp"
#include "storage/FileUserRepository.hpp"
#include "storage/MemoryPageStore.hpp"
#include "storage/UserFile.hpp"
#include "telemetry/ActivityTracker.hpp"
#include "telemetry/InstrumentedPageStore.hpp"
#include "telemetry/InstrumentedUserRepository.hpp"
#include "telemetry/TraceAwareIdGenerator.hpp"

namespace tecflix::users {

using std::chrono::minutes;
using std::chrono::seconds;
using testing::FakeUsersMetrics;
using testing::TempFile;
using testing::TestClock;

namespace {

User makeUser(int number) {
    const std::string suffix = std::to_string(number);
    return User{"id-" + suffix, "user" + suffix, "user" + suffix + "@tec.cr", "hash", 1000, 0};
}

}  // namespace

TEST(InstrumentedPageStoreTest, CountsReadsAndWritesAndKeepsTheData) {
    MemoryPageStore inner;
    FakeUsersMetrics metrics;
    InstrumentedPageStore store{inner, metrics};
    PageBuffer page{};
    page[0] = 42;

    const PageId id = store.allocate();
    store.write(id, page);
    PageBuffer readBack{};
    store.read(id, readBack);
    store.read(id, readBack);

    EXPECT_EQ(readBack[0], 42);
    EXPECT_EQ(store.pageCount(), 1U);
    EXPECT_EQ(metrics.pagesRead, 2);
    EXPECT_EQ(metrics.pagesWritten, 2);  // crear la página y escribirla
}

TEST(InstrumentedPageStoreTest, TheBTreeReadsPagesThroughIt) {
    MemoryPageStore inner;
    FakeUsersMetrics metrics;
    InstrumentedPageStore store{inner, metrics};
    BTree tree{store};
    tree.insert("alice", 1);
    const int readsBefore = metrics.pagesRead;

    EXPECT_EQ(tree.find("alice"), 1U);

    EXPECT_GT(metrics.pagesRead, readsBefore);
}

TEST(InstrumentedUserRepositoryTest, MeasuresSearchesAndCountsCreationsAndDeletions) {
    const TempFile temp{"instrumented_repo.dat"};
    UserFile file{temp.path()};
    MemoryPageStore pages;
    BTree index{pages};
    FileUserRepository inner{file, index};
    FakeUsersMetrics metrics;
    InstrumentedUserRepository repository{inner, metrics};

    EXPECT_EQ(repository.add(makeUser(1)), AddResult::Added);
    EXPECT_EQ(repository.add(makeUser(1)), AddResult::IdTaken);  // rechazado: no cuenta
    static_cast<void>(repository.findById("id-1"));
    static_cast<void>(repository.findByUsername("user1"));
    static_cast<void>(repository.findByEmail("nadie@tec.cr"));  // sin resultado: también se mide
    EXPECT_TRUE(repository.remove("id-1"));
    EXPECT_FALSE(repository.remove("id-1"));  // ya no existe: no cuenta

    EXPECT_EQ(metrics.created, 1);
    EXPECT_EQ(metrics.deleted, 1);
    EXPECT_EQ(metrics.searches, (std::vector<std::string>{"id", "username", "email"}));
    EXPECT_FALSE(metrics.negativeDurations);
}

TEST(InstrumentedUserRepositoryTest, ResultsAreTheSameAsTheWrappedRepository) {
    const TempFile temp{"instrumented_same.dat"};
    UserFile file{temp.path()};
    MemoryPageStore pages;
    BTree index{pages};
    FileUserRepository inner{file, index};
    FakeUsersMetrics metrics;
    InstrumentedUserRepository repository{inner, metrics};
    repository.add(makeUser(1));

    EXPECT_EQ(repository.modify("id-1", [](User& user) { user.usedBytes = 7; }), UpdateResult::Updated);
    EXPECT_EQ(repository.findById("id-1")->usedBytes, 7U);
    EXPECT_EQ(repository.usageTotals().usedBytes, 7U);
}

TEST(ActivityTrackerTest, CountsEachUserOnceWhileTheyAreRecent) {
    TestClock clock{seconds{1'800'000'000}};
    ActivityTracker tracker{clock};

    tracker.touch("alice");
    tracker.touch("alice");
    tracker.touch("bob");

    EXPECT_EQ(tracker.activeCount(), 2U);
}

TEST(ActivityTrackerTest, UsersBecomeInactiveAfterFifteenMinutes) {
    TestClock clock{seconds{1'800'000'000}};
    ActivityTracker tracker{clock};
    tracker.touch("alice");
    clock.advance(minutes{10});
    tracker.touch("bob");

    clock.advance(minutes{6});  // alice: 16 minutos sin actividad; bob: 6
    EXPECT_EQ(tracker.activeCount(), 1U);

    clock.advance(minutes{10});
    EXPECT_EQ(tracker.activeCount(), 0U);
}

TEST(ActivityTrackerTest, ActivityRenewsTheWindow) {
    TestClock clock{seconds{1'800'000'000}};
    ActivityTracker tracker{clock};
    tracker.touch("alice");
    clock.advance(minutes{14});
    tracker.touch("alice");
    clock.advance(minutes{14});

    EXPECT_EQ(tracker.activeCount(), 1U);
}

TEST(TraceAwareIdGeneratorTest, FallsBackToANewIdWhenThereIsNoActiveTrace) {
    const UuidV4Generator fallback;
    const TraceAwareIdGenerator generator{fallback};

    const std::string first = generator.next();
    const std::string second = generator.next();

    EXPECT_EQ(first.size(), 36U);
    EXPECT_NE(first, second);
}

}  // namespace tecflix::users
