#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "storage/BTreeLayout.hpp"
#include "storage/BTreePage.hpp"

namespace tecflix::users {

TEST(BTreeLayoutTest, OrderIsDerivedFromPageSize) {
    EXPECT_EQ(kMaxKeys, 29U);
    EXPECT_EQ(kMinDegree, 15U);
    EXPECT_EQ(kMaxKeys, 2 * kMinDegree - 1);
}

TEST(BTreePageTest, LeafSurvivesEncodeAndDecode) {
    BTreeNode node;
    node.id = 7;
    node.leaf = true;
    node.keys = {"alice", "bob"};
    node.values = {100, 200};
    PageBuffer buffer{};

    BTreePage::encode(node, buffer);
    const BTreeNode decoded = BTreePage::decode(7, buffer);

    EXPECT_TRUE(decoded.leaf);
    EXPECT_EQ(decoded.keys, node.keys);
    EXPECT_EQ(decoded.values, node.values);
    EXPECT_TRUE(decoded.children.empty());
}

TEST(BTreePageTest, InternalNodeKeepsItsChildren) {
    BTreeNode node;
    node.id = 3;
    node.leaf = false;
    node.keys = {"m"};
    node.values = {42};
    node.children = {10, 11};
    PageBuffer buffer{};

    BTreePage::encode(node, buffer);
    const BTreeNode decoded = BTreePage::decode(3, buffer);

    EXPECT_FALSE(decoded.leaf);
    EXPECT_EQ(decoded.children, node.children);
}

TEST(BTreePageTest, FullNodeWithLongestKeysFitsInOnePage) {
    BTreeNode node;
    node.leaf = true;
    for (std::size_t i = 0; i < kMaxKeys; ++i) {
        node.keys.push_back(std::string(kMaxKeyLength - 2, 'a') + std::to_string(10 + i));
        node.values.push_back(i);
    }
    PageBuffer buffer{};

    BTreePage::encode(node, buffer);

    EXPECT_EQ(BTreePage::decode(0, buffer).keys, node.keys);
}

TEST(BTreePageTest, RejectsInconsistentNode) {
    BTreeNode node;
    node.leaf = true;
    node.keys = {"a"};  // falta el valor
    PageBuffer buffer{};

    EXPECT_THROW(BTreePage::encode(node, buffer), std::logic_error);
}

TEST(BTreePageTest, RejectsCorruptPage) {
    PageBuffer buffer{};
    buffer[2] = 0xFF;  // cantidad de claves imposible
    buffer[3] = 0xFF;

    EXPECT_THROW(BTreePage::decode(0, buffer), std::runtime_error);
}

}  // namespace tecflix::users
