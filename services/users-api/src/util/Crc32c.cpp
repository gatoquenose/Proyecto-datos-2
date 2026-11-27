#include "util/Crc32c.hpp"

namespace tecflix::users {

namespace {

// Polinomio de Castagnoli en su forma reflejada.
constexpr std::uint32_t kPolynomial = 0x82F63B78U;
constexpr std::size_t kTableSize = 256;
constexpr unsigned kBitsPerByte = 8;
constexpr std::uint32_t kByteMask = 0xFFU;

constexpr std::array<std::uint32_t, kTableSize> makeTable() {
    std::array<std::uint32_t, kTableSize> table{};
    for (std::uint32_t i = 0; i < kTableSize; ++i) {
        std::uint32_t crc = i;
        for (unsigned bit = 0; bit < kBitsPerByte; ++bit) {
            crc = (crc & 1U) != 0 ? (crc >> 1U) ^ kPolynomial : crc >> 1U;
        }
        table[i] = crc;
    }
    return table;
}

constexpr auto kTable = makeTable();

}  // namespace

void Crc32c::update(std::span<const std::uint8_t> bytes) {
    for (const std::uint8_t byte : bytes) {
        state_ = kTable[(state_ ^ byte) & kByteMask] ^ (state_ >> kBitsPerByte);
    }
}

std::uint32_t Crc32c::value() const {
    return ~state_;
}

std::uint32_t Crc32c::of(std::span<const std::uint8_t> bytes) {
    Crc32c crc;
    crc.update(bytes);
    return crc.value();
}

}  // namespace tecflix::users
