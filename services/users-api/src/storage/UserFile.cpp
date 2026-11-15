#include "storage/UserFile.hpp"

#include <array>
#include <stdexcept>
#include <string_view>

#include "storage/ByteCodec.hpp"
#include "storage/UserRecordCodec.hpp"

namespace tecflix::users {

namespace {

constexpr std::string_view kMagic = "TFXUSR01";
constexpr std::uint32_t kFormatVersion = 1;
constexpr std::uint64_t kHeaderBytes = 64;
constexpr std::size_t kVersionOffset = 8;
constexpr std::size_t kRecordSizeOffset = 12;

using HeaderBuffer = std::array<std::uint8_t, kHeaderBytes>;

}  // namespace

UserFile::UserFile(const std::filesystem::path& path) : file_(path) {
    if (file_.size() == 0) {
        writeHeader();
    } else {
        validateHeader();
    }
    const std::uint64_t body = file_.size() - kHeaderBytes;
    if (body % kRecordBytes != 0) {
        throw std::runtime_error("users.dat está corrupto (tamaño inválido)");
    }
    recordCount_ = body / kRecordBytes;
}

void UserFile::writeHeader() {
    HeaderBuffer header{};
    std::ranges::copy(kMagic, header.begin());
    putLittleEndian<std::uint32_t>(header, kVersionOffset, kFormatVersion);
    putLittleEndian<std::uint32_t>(header, kRecordSizeOffset, static_cast<std::uint32_t>(kRecordBytes));
    file_.writeAt(0, header);
}

void UserFile::validateHeader() const {
    if (file_.size() < kHeaderBytes) {
        throw std::runtime_error("users.dat no tiene una cabecera válida");
    }
    HeaderBuffer header{};
    file_.readAt(0, header);
    const bool magicMatches = std::ranges::equal(kMagic, std::span{header}.first(kMagic.size()));
    const bool formatMatches = getLittleEndian<std::uint32_t>(header, kVersionOffset) == kFormatVersion &&
                               getLittleEndian<std::uint32_t>(header, kRecordSizeOffset) == kRecordBytes;
    if (!magicMatches || !formatMatches) {
        throw std::runtime_error("users.dat no tiene una cabecera válida");
    }
}

void UserFile::checkOffset(std::uint64_t offset) const {
    const bool validPosition = offset >= kHeaderBytes && (offset - kHeaderBytes) % kRecordBytes == 0 &&
                               (offset - kHeaderBytes) / kRecordBytes < recordCount_;
    if (!validPosition) {
        throw std::out_of_range("offset de registro inválido");
    }
}

std::uint64_t UserFile::append(const User& user) {
    RecordBuffer buffer{};
    UserRecordCodec::encode(user, RecordStatus::Active, buffer);
    const std::uint64_t offset = kHeaderBytes + recordCount_ * kRecordBytes;
    file_.writeAt(offset, buffer);
    ++recordCount_;
    return offset;
}

User UserFile::read(std::uint64_t offset) const {
    checkOffset(offset);
    RecordBuffer buffer{};
    file_.readAt(offset, buffer);
    DecodedRecord decoded = UserRecordCodec::decode(buffer);
    if (decoded.status == RecordStatus::Deleted) {
        throw std::runtime_error("el registro está eliminado");
    }
    return std::move(decoded.user);
}

void UserFile::overwrite(std::uint64_t offset, const User& user) {
    static_cast<void>(read(offset));  // comprueba que el registro existe y está activo
    RecordBuffer buffer{};
    UserRecordCodec::encode(user, RecordStatus::Active, buffer);
    file_.writeAt(offset, buffer);
}

void UserFile::markDeleted(std::uint64_t offset) {
    checkOffset(offset);
    RecordBuffer buffer{};
    file_.readAt(offset, buffer);
    UserRecordCodec::setStatus(buffer, RecordStatus::Deleted);
    file_.writeAt(offset, buffer);
}

std::uint64_t UserFile::recordCount() const {
    return recordCount_;
}

}  // namespace tecflix::users
