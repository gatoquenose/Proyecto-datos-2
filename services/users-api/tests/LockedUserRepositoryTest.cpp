#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "TempFile.hpp"
#include "storage/BTree.hpp"
#include "storage/FileUserRepository.hpp"
#include "storage/LockedUserRepository.hpp"
#include "storage/MemoryPageStore.hpp"
#include "storage/UserFile.hpp"

namespace tecflix::users {

using testing::TempFile;

namespace {

constexpr int kThreads = 8;
constexpr int kUsersPerThread = 60;

User makeUser(int number) {
    const std::string suffix = std::to_string(number);
    return User{"id-" + suffix, "user" + suffix, "user" + suffix + "@tec.cr", "hash", 1000, 0};
}

}  // namespace

TEST(LockedUserRepositoryTest, BehavesLikeTheRepositoryItWraps) {
    const TempFile temp{"locked_basic.dat"};
    UserFile file{temp.path()};
    MemoryPageStore pages;
    BTree index{pages};
    FileUserRepository inner{file, index};
    LockedUserRepository repository{inner};

    EXPECT_EQ(repository.add(makeUser(1)), AddResult::Added);
    EXPECT_EQ(repository.add(makeUser(1)), AddResult::IdTaken);
    EXPECT_EQ(repository.findByUsername("user1")->id, "id-1");
    EXPECT_EQ(repository.modify("id-1", [](User& user) { user.usedBytes = 5; }), UpdateResult::Updated);
    EXPECT_EQ(repository.findById("id-1")->usedBytes, 5U);
    EXPECT_TRUE(repository.remove("id-1"));
    EXPECT_FALSE(repository.findByEmail("user1@tec.cr").has_value());
}

TEST(LockedUserRepositoryTest, ManyThreadsCanAddUsersAtTheSameTime) {
    const TempFile temp{"locked_threads.dat"};
    UserFile file{temp.path()};
    MemoryPageStore pages;
    BTree index{pages};
    FileUserRepository inner{file, index};
    LockedUserRepository repository{inner};

    std::vector<std::thread> workers;
    for (int worker = 0; worker < kThreads; ++worker) {
        workers.emplace_back([&repository, worker] {
            for (int i = 0; i < kUsersPerThread; ++i) {
                repository.add(makeUser(worker * kUsersPerThread + i));
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    for (int number = 0; number < kThreads * kUsersPerThread; ++number) {
        ASSERT_EQ(repository.findByEmail("user" + std::to_string(number) + "@tec.cr")->id, "id-" + std::to_string(number));
    }
    EXPECT_EQ(file.recordCount(), static_cast<std::uint64_t>(kThreads * kUsersPerThread));
}

TEST(LockedUserRepositoryTest, ConcurrentModificationsDoNotLoseUpdates) {
    const TempFile temp{"locked_modify.dat"};
    UserFile file{temp.path()};
    MemoryPageStore pages;
    BTree index{pages};
    FileUserRepository inner{file, index};
    LockedUserRepository repository{inner};
    repository.add(makeUser(1));

    std::vector<std::thread> workers;
    for (int worker = 0; worker < kThreads; ++worker) {
        workers.emplace_back([&repository] {
            for (int i = 0; i < kUsersPerThread; ++i) {
                repository.modify("id-1", [](User& user) { user.usedBytes += 1; });
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_EQ(repository.findById("id-1")->usedBytes, static_cast<std::uint64_t>(kThreads * kUsersPerThread));
}

}  // namespace tecflix::users
