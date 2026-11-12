#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "storage/BTree.hpp"
#include "storage/BTreeLayout.hpp"
#include "storage/FilePageStore.hpp"
#include "storage/MemoryPageStore.hpp"

namespace tecflix::users {

namespace {

constexpr std::size_t kManyKeys = 5000;
constexpr unsigned kSeed = 12345;

std::string keyFor(std::size_t number) {
    std::string digits = std::to_string(number);
    return "user" + std::string(8 - digits.size(), '0') + digits;
}

// Números 0..count-1 en orden aleatorio pero reproducible.
std::vector<std::size_t> shuffledNumbers(std::size_t count) {
    std::vector<std::size_t> numbers(count);
    std::iota(numbers.begin(), numbers.end(), std::size_t{0});
    std::mt19937 generator{kSeed};
    std::ranges::shuffle(numbers, generator);
    return numbers;
}

}  // namespace

TEST(BTreeTest, FindsInsertedKey) {
    MemoryPageStore store;
    BTree tree{store};

    EXPECT_TRUE(tree.insert("alice", 10));

    EXPECT_EQ(tree.find("alice"), 10U);
    EXPECT_FALSE(tree.find("bob").has_value());
}

TEST(BTreeTest, RejectsDuplicateKeyAndKeepsOriginalValue) {
    MemoryPageStore store;
    BTree tree{store};
    tree.insert("alice", 10);

    EXPECT_FALSE(tree.insert("alice", 99));

    EXPECT_EQ(tree.find("alice"), 10U);
}

TEST(BTreeTest, RejectsInvalidKeys) {
    MemoryPageStore store;
    BTree tree{store};

    EXPECT_THROW(tree.insert("", 1), std::invalid_argument);
    EXPECT_THROW(tree.insert(std::string(kMaxKeyLength + 1, 'x'), 1), std::invalid_argument);
    EXPECT_NO_THROW(tree.insert(std::string(kMaxKeyLength, 'x'), 1));
}

TEST(BTreeTest, KeepsEveryKeyAfterManySplits) {
    MemoryPageStore store;
    BTree tree{store};
    const auto numbers = shuffledNumbers(kManyKeys);

    for (const auto number : numbers) {
        ASSERT_TRUE(tree.insert(keyFor(number), number));
    }

    for (std::size_t number = 0; number < kManyKeys; ++number) {
        ASSERT_EQ(tree.find(keyFor(number)), number);
    }
    EXPECT_FALSE(tree.find(keyFor(kManyKeys)).has_value());
}

TEST(BTreeTest, TraversesKeysInAscendingOrder) {
    MemoryPageStore store;
    BTree tree{store};
    for (const auto number : shuffledNumbers(kManyKeys)) {
        tree.insert(keyFor(number), number);
    }

    std::vector<std::string> visited;
    tree.forEachInOrder([&visited](const std::string& key, std::uint64_t) { visited.push_back(key); });

    EXPECT_EQ(visited.size(), kManyKeys);
    EXPECT_TRUE(std::ranges::is_sorted(visited));
}

TEST(BTreeTest, RejectsStoreThatIsNotAnIndex) {
    MemoryPageStore store;
    store.allocate();  // página 0 llena de ceros: sin la firma del índice
    store.allocate();

    EXPECT_THROW(BTree{store}, std::runtime_error);
}

class BTreeFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        path_ = std::filesystem::temp_directory_path() /
                (std::string{"tecflix_btree_"} +
                 ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".idx");
        std::filesystem::remove(path_);
    }
    void TearDown() override { std::filesystem::remove(path_); }

    std::filesystem::path path_;
};

TEST_F(BTreeFileTest, DataSurvivesClosingAndReopeningTheFile) {
    constexpr std::size_t kCount = 2000;
    {
        FilePageStore store{path_};
        BTree tree{store};
        for (const auto number : shuffledNumbers(kCount)) {
            ASSERT_TRUE(tree.insert(keyFor(number), number * 10));
        }
    }

    FilePageStore store{path_};
    const BTree tree{store};
    for (std::size_t number = 0; number < kCount; ++number) {
        ASSERT_EQ(tree.find(keyFor(number)), number * 10);
    }
}

TEST_F(BTreeFileTest, FileSizeIsAMultipleOfThePageSize) {
    {
        FilePageStore store{path_};
        BTree tree{store};
        for (std::size_t number = 0; number < 500; ++number) {
            tree.insert(keyFor(number), number);
        }
    }

    EXPECT_EQ(std::filesystem::file_size(path_) % kPageSize, 0U);
}

TEST_F(BTreeFileTest, RefusesCorruptFile) {
    {
        FilePageStore store{path_};
        BTree tree{store};
    }
    std::filesystem::resize_file(path_, kPageSize + 17);  // tamaño inválido

    EXPECT_THROW(FilePageStore{path_}, std::runtime_error);
}

}  // namespace tecflix::users
