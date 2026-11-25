#include "storage/FixedRecordFile.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

#include "storage/ByteCodec.hpp"

namespace tecflix::users {

namespace {

constexpr std::uint32_t kFormatVersion = 1;
constexpr std::size_t kVersionOffset = 8;
constexpr std::size_t kRecordSizeOffset = 12;

using HeaderBuffer = std::array<std::uint8_t, FixedRecordFile::kHeaderBytes>;

}  // namespace

FixedRecordFile::FixedRecordFile(const std::filesystem::path& path, std::string_view magic, std::size_t recordBytes)
    : file_(path), magic_(magic), recordBytes_(recordBytes) {
    if (magic_.size() != kMagicBytes || recordBytes_ == 0) {
        throw std::invalid_argument("firma o tamaño de registro inválidos");
    }
    if (file_.size() == 0) {
        writeHeader();
    } else {
        validateHeader();
    }
    const std::uint64_t body = file_.size() - kHeaderBytes;
    if (body % recordBytes_ != 0) {
        throw std::runtime_error("el archivo está corrupto (tamaño inválido)");
    }
    recordCount_ = body / recordBytes_;
}

void FixedRecordFile::writeHeader() {
    HeaderBuffer header{};
    std::ranges::copy(magic_, header.begin());
    putLittleEndian<std::uint32_t>(header, kVersionOffset, kFormatVersion);
    putLittleEndian<std::uint32_t>(header, kRecordSizeOffset, static_cast<std::uint32_t>(recordBytes_));
    file_.writeAt(0, header);
}

void FixedRecordFile::validateHeader() const {
    if (file_.size() < kHeaderBytes) {
        throw std::runtime_error("el archivo no tiene una cabecera válida");
    }
    HeaderBuffer header{};
    file_.readAt(0, header);
    const bool magicMatches = std::ranges::equal(magic_, std::span{header}.first(kMagicBytes));
    const bool formatMatches = getLittleEndian<std::uint32_t>(header, kVersionOffset) == kFormatVersion &&
                               getLittleEndian<std::uint32_t>(header, kRecordSizeOffset) == recordBytes_;
    if (!magicMatches || !formatMatches) {
        throw std::runtime_error("el archivo no tiene una cabecera válida");
    }
}

void FixedRecordFile::checkOffset(std::uint64_t offset) const {
    const bool validPosition = offset >= kHeaderBytes && (offset - kHeaderBytes) % recordBytes_ == 0 &&
                               (offset - kHeaderBytes) / recordBytes_ < recordCount_;
    if (!validPosition) {
        throw std::out_of_range("offset de registro inválido");
    }
}

void FixedRecordFile::checkSize(std::span<const std::uint8_t> bytes) const {
    if (bytes.size() != recordBytes_) {
        throw std::logic_error("el registro no tiene el tamaño del archivo");
    }
}

std::uint64_t FixedRecordFile::append(std::span<const std::uint8_t> record) {
    checkSize(record);
    const std::uint64_t offset = kHeaderBytes + recordCount_ * recordBytes_;
    file_.writeAt(offset, record);
    ++recordCount_;
    return offset;
}

void FixedRecordFile::read(std::uint64_t offset, std::span<std::uint8_t> out) const {
    checkOffset(offset);
    checkSize(out);
    file_.readAt(offset, out);
}

void FixedRecordFile::write(std::uint64_t offset, std::span<const std::uint8_t> record) {
    checkOffset(offset);
    checkSize(record);
    file_.writeAt(offset, record);
}

std::uint64_t FixedRecordFile::recordCount() const {
    return recordCount_;
}

std::uint64_t FixedRecordFile::offsetOfRecord(std::uint64_t ordinal) const {
    return kHeaderBytes + ordinal * recordBytes_;
}

}  // namespace tecflix::users
