#include "auth/UuidV4Generator.hpp"

#include <sodium.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

#include "tecflix/auth/CryptoRuntime.hpp"

namespace tecflix::users {

namespace {

constexpr std::size_t kUuidBytes = 16;
constexpr std::size_t kUuidTextLength = 36;
constexpr std::size_t kVersionByte = 6;
constexpr std::size_t kVariantByte = 8;
constexpr std::uint8_t kVersion4 = 0x40;
constexpr std::uint8_t kVariantRfc4122 = 0x80;
constexpr std::uint8_t kKeepLowNibble = 0x0F;
constexpr std::uint8_t kKeepLowSixBits = 0x3F;
constexpr std::string_view kHexDigits = "0123456789abcdef";

// Posiciones (en bytes) después de las cuales va un guion: 8-4-4-4-12 caracteres.
constexpr std::array<std::size_t, 4> kDashAfterByte{4, 6, 8, 10};

bool hasDashAfter(std::size_t byteIndex) {
    return std::ranges::find(kDashAfterByte, byteIndex) != kDashAfterByte.end();
}

}  // namespace

UuidV4Generator::UuidV4Generator() {
    tecflix::auth::ensureCryptoReady();
}

std::string UuidV4Generator::next() const {
    std::array<std::uint8_t, kUuidBytes> bytes{};
    randombytes_buf(bytes.data(), bytes.size());
    bytes[kVersionByte] = static_cast<std::uint8_t>((bytes[kVersionByte] & kKeepLowNibble) | kVersion4);
    bytes[kVariantByte] = static_cast<std::uint8_t>((bytes[kVariantByte] & kKeepLowSixBits) | kVariantRfc4122);

    std::string uuid;
    uuid.reserve(kUuidTextLength);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (hasDashAfter(i)) {
            uuid.push_back('-');
        }
        uuid.push_back(kHexDigits[bytes[i] >> 4]);
        uuid.push_back(kHexDigits[bytes[i] & kKeepLowNibble]);
    }
    return uuid;
}

}  // namespace tecflix::users
