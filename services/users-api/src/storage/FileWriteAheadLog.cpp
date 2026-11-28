#include "storage/FileWriteAheadLog.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

#include "storage/ByteCodec.hpp"
#include "storage/Checksum.hpp"

namespace tecflix::users {

namespace {

constexpr std::string_view kMagic = "TFXWAL01";

constexpr std::size_t kOpOffset = 0;
constexpr std::size_t kPositionOffset = 1;
constexpr std::size_t kBeforeOffset = kPositionOffset + sizeof(std::uint64_t);
constexpr std::size_t kAfterOffset = kBeforeOffset + kRecordBytes;
constexpr std::size_t kChecksumOffset = kAfterOffset + kRecordBytes;
constexpr std::size_t kEntryBytes = kChecksumOffset + kChecksumBytes;

using EntryBuffer = std::array<std::uint8_t, kEntryBytes>;

constexpr std::uint8_t kLastKnownOp = static_cast<std::uint8_t>(WalOp::Remove);

EntryBuffer encode(const WalEntry& entry) {
    EntryBuffer buffer{};
    buffer[kOpOffset] = static_cast<std::uint8_t>(entry.op);
    putLittleEndian<std::uint64_t>(buffer, kPositionOffset, entry.offset);
    std::ranges::copy(entry.before, buffer.begin() + static_cast<std::ptrdiff_t>(kBeforeOffset));
    std::ranges::copy(entry.after, buffer.begin() + static_cast<std::ptrdiff_t>(kAfterOffset));
    sealChecksum(buffer, kChecksumOffset);
    return buffer;
}

}  // namespace

FileWriteAheadLog::FileWriteAheadLog(const std::filesystem::path& path) : file_(path, kMagic, kEntryBytes) {
    armed_ = pending().has_value();
}

void FileWriteAheadLog::store(const WalEntry& entry) {
    const EntryBuffer buffer = encode(entry);
    if (file_.recordCount() == 0) {
        file_.append(buffer);
    } else {
        file_.write(file_.offsetOfRecord(0), buffer);
    }
}

void FileWriteAheadLog::log(const WalEntry& entry) {
    if (armed_) {
        throw std::logic_error("hay una operación pendiente sin rehacer");
    }
    store(entry);
    armed_ = true;
}

void FileWriteAheadLog::clear() {
    store(WalEntry{});
    armed_ = false;
}

std::optional<WalEntry> FileWriteAheadLog::pending() const {
    if (file_.recordCount() == 0) {
        return std::nullopt;
    }
    EntryBuffer buffer{};
    file_.read(file_.offsetOfRecord(0), buffer);
    const std::uint8_t op = buffer[kOpOffset];
    if (!checksumMatches(buffer, kChecksumOffset) || op == static_cast<std::uint8_t>(WalOp::None) ||
        op > kLastKnownOp) {
        return std::nullopt;  // vacía, o una anotación que se cortó a la mitad: la operación no empezó
    }
    WalEntry entry;
    entry.op = static_cast<WalOp>(op);
    entry.offset = getLittleEndian<std::uint64_t>(buffer, kPositionOffset);
    std::copy_n(buffer.begin() + static_cast<std::ptrdiff_t>(kBeforeOffset), kRecordBytes, entry.before.begin());
    std::copy_n(buffer.begin() + static_cast<std::ptrdiff_t>(kAfterOffset), kRecordBytes, entry.after.begin());
    return entry;
}

}  // namespace tecflix::users
