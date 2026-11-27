#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "util/Crc32c.hpp"

namespace tecflix::users {

namespace {

std::vector<std::uint8_t> bytesOf(std::string_view text) {
    return {text.begin(), text.end()};
}

}  // namespace

TEST(Crc32cTest, MatchesTheStandardCheckValue) {
    // Valor de referencia publicado para CRC32C sobre el texto "123456789".
    EXPECT_EQ(Crc32c::of(bytesOf("123456789")), 0xE3069283U);
}

TEST(Crc32cTest, EmptyInputHasCrcZero) {
    EXPECT_EQ(Crc32c::of({}), 0U);
}

TEST(Crc32cTest, UpdatingInPartsGivesTheSameResultAsAllAtOnce) {
    const std::vector<std::uint8_t> all = bytesOf("123456789");
    Crc32c crc;
    crc.update(std::span{all}.first(4));
    crc.update(std::span{all}.subspan(4));

    EXPECT_EQ(crc.value(), Crc32c::of(all));
}

TEST(Crc32cTest, ChangingOneBitChangesTheResult) {
    std::vector<std::uint8_t> data = bytesOf("tecflix");
    const std::uint32_t before = Crc32c::of(data);
    data[3] ^= 0x01U;

    EXPECT_NE(Crc32c::of(data), before);
}

}  // namespace tecflix::users
