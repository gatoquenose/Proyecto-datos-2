#include "storage/Checksum.hpp"

#include <string>

#include "storage/ByteCodec.hpp"
#include "storage/CorruptionError.hpp"
#include "util/Crc32c.hpp"

namespace tecflix::users {

namespace {

std::uint32_t computeAround(std::span<const std::uint8_t> block, std::size_t checksumOffset) {
    Crc32c crc;
    crc.update(block.first(checksumOffset));
    crc.update(block.subspan(checksumOffset + kChecksumBytes));
    return crc.value();
}

}  // namespace

void sealChecksum(std::span<std::uint8_t> block, std::size_t checksumOffset) {
    putLittleEndian<std::uint32_t>(block, checksumOffset, computeAround(block, checksumOffset));
}

bool checksumMatches(std::span<const std::uint8_t> block, std::size_t checksumOffset) {
    return getLittleEndian<std::uint32_t>(block, checksumOffset) == computeAround(block, checksumOffset);
}

void requireChecksum(std::span<const std::uint8_t> block, std::size_t checksumOffset, const char* what) {
    if (!checksumMatches(block, checksumOffset)) {
        throw CorruptionError(std::string(what) + " corrupto: el checksum CRC32C no coincide");
    }
}

}  // namespace tecflix::users
