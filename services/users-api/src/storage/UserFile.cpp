#include "storage/UserFile.hpp"

#include <stdexcept>

#include "storage/CorruptionError.hpp"
#include "storage/UserRecordCodec.hpp"

namespace tecflix::users {

namespace {

constexpr std::string_view kMagic = "TFXUSR01";

}  // namespace

UserFile::UserFile(const std::filesystem::path& path) : file_(path, kMagic, kRecordBytes) {}

std::uint64_t UserFile::append(const User& user) {
    RecordBuffer buffer{};
    UserRecordCodec::encode(user, RecordStatus::Active, buffer);
    return file_.append(buffer);
}

User UserFile::read(std::uint64_t offset) const {
    RecordBuffer buffer{};
    file_.read(offset, buffer);
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
    file_.write(offset, buffer);
}

void UserFile::markDeleted(std::uint64_t offset) {
    RecordBuffer buffer{};
    file_.read(offset, buffer);
    UserRecordCodec::setStatus(buffer, RecordStatus::Deleted);
    file_.write(offset, buffer);
}

void UserFile::scan(const std::function<void(const ScannedRecord&)>& visit) const {
    for (std::uint64_t ordinal = 0; ordinal < file_.recordCount(); ++ordinal) {
        const std::uint64_t offset = file_.offsetOfRecord(ordinal);
        RecordBuffer buffer{};
        file_.read(offset, buffer);
        try {
            DecodedRecord decoded = UserRecordCodec::decode(buffer);
            const bool active = decoded.status == RecordStatus::Active;
            visit({offset, active ? RecordState::Active : RecordState::Deleted,
                   active ? std::optional<User>{std::move(decoded.user)} : std::nullopt});
        } catch (const CorruptionError&) {
            visit({offset, RecordState::Corrupt, std::nullopt});
        }
    }
}

void UserFile::forEachActive(const std::function<void(const User&)>& visit) const {
    scan([&visit](const ScannedRecord& record) {
        if (record.state == RecordState::Active) {
            visit(*record.user);
        }
    });
}

std::uint64_t UserFile::recordCount() const {
    return file_.recordCount();
}

}  // namespace tecflix::users
