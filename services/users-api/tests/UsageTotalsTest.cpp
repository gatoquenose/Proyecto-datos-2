#include <gtest/gtest.h>

#include "TempFile.hpp"
#include "storage/BTree.hpp"
#include "storage/FileUserRepository.hpp"
#include "storage/LockedUserRepository.hpp"
#include "storage/MemoryPageStore.hpp"
#include "storage/UserFile.hpp"

namespace tecflix::users {

using testing::TempFile;

namespace {

User makeUser(int number, std::uint64_t quota, std::uint64_t used) {
    const std::string suffix = std::to_string(number);
    return User{"id-" + suffix, "user" + suffix, "user" + suffix + "@tec.cr", "hash", quota, used};
}

}  // namespace

TEST(UsageTotalsTest, EmptyRepositoryHasNoUsage) {
    const TempFile temp{"totals_empty.dat"};
    UserFile file{temp.path()};
    MemoryPageStore pages;
    BTree index{pages};
    const FileUserRepository repository{file, index};

    const UsageTotals totals = repository.usageTotals();

    EXPECT_EQ(totals.users, 0U);
    EXPECT_EQ(totals.usedBytes, 0U);
    EXPECT_EQ(totals.quotaBytes, 0U);
}

TEST(UsageTotalsTest, AddsUpOnlyTheActiveUsers) {
    const TempFile temp{"totals_sum.dat"};
    UserFile file{temp.path()};
    MemoryPageStore pages;
    BTree index{pages};
    FileUserRepository inner{file, index};
    LockedUserRepository repository{inner};
    repository.add(makeUser(1, 1000, 100));
    repository.add(makeUser(2, 2000, 300));
    repository.add(makeUser(3, 4000, 50));
    repository.remove("id-3");  // eliminado: ya no cuenta
    repository.modify("id-1", [](User& user) { user.usedBytes = 250; });

    const UsageTotals totals = repository.usageTotals();

    EXPECT_EQ(totals.users, 2U);
    EXPECT_EQ(totals.usedBytes, 550U);
    EXPECT_EQ(totals.quotaBytes, 3000U);
}

}  // namespace tecflix::users
