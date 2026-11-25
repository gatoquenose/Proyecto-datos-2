#include "storage/FileReservationRepository.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

#include "storage/ReservationRecordCodec.hpp"

namespace tecflix::users {

namespace {

constexpr std::string_view kMagic = "TFXRSV01";
constexpr char kCaseOffset = 'a' - 'A';

// Los ids son UUID: se indexan en minúsculas para que "ABC" y "abc" sean la misma reserva.
std::string indexKey(std::string_view id) {
    std::string key{id};
    std::ranges::transform(key, key.begin(), [](char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + kCaseOffset) : c;
    });
    return key;
}

}  // namespace

FileReservationRepository::FileReservationRepository(const std::filesystem::path& dataFile, BTree& index)
    : file_(dataFile, kMagic, kReservationRecordBytes), index_(index) {}

Reservation FileReservationRepository::readAt(std::uint64_t offset) const {
    ReservationBuffer buffer{};
    file_.read(offset, buffer);
    return ReservationRecordCodec::decode(buffer);
}

bool FileReservationRepository::add(const Reservation& reservation) {
    const std::string key = indexKey(reservation.id);
    if (index_.find(key)) {
        return false;
    }
    ReservationBuffer buffer{};
    ReservationRecordCodec::encode(reservation, buffer);
    const std::uint64_t offset = file_.append(buffer);
    index_.insert(key, offset);
    return true;
}

void FileReservationRepository::update(const Reservation& reservation) {
    const auto offset = index_.find(indexKey(reservation.id));
    if (!offset) {
        throw std::logic_error("no existe la reserva que se quiere actualizar");
    }
    ReservationBuffer buffer{};
    ReservationRecordCodec::encode(reservation, buffer);
    file_.write(*offset, buffer);
}

std::optional<Reservation> FileReservationRepository::find(std::string_view id) const {
    const auto offset = index_.find(indexKey(id));
    if (!offset) {
        return std::nullopt;
    }
    return readAt(*offset);
}

std::vector<Reservation> FileReservationRepository::findActiveExpiredBy(std::int64_t now) const {
    std::vector<Reservation> expired;
    for (std::uint64_t ordinal = 0; ordinal < file_.recordCount(); ++ordinal) {
        Reservation reservation = readAt(file_.offsetOfRecord(ordinal));
        if (reservation.status == ReservationStatus::Active && reservation.expiresAt <= now) {
            expired.push_back(std::move(reservation));
        }
    }
    return expired;
}

}  // namespace tecflix::users
