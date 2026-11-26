#pragma once

#include <filesystem>

#include "domain/IReservationRepository.hpp"
#include "storage/BTree.hpp"
#include "storage/FixedRecordFile.hpp"

namespace tecflix::users {

// reservations.dat (los datos, un registro por reserva) + un árbol B propio que busca por id.
// Las reservas nunca se borran: cambian de estado. La limpieza de registros viejos (compactar el
// archivo) queda como mejora pendiente.
class FileReservationRepository final : public IReservationRepository {
public:
    FileReservationRepository(const std::filesystem::path& dataFile, BTree& index);

    bool add(const Reservation& reservation) override;
    void update(const Reservation& reservation) override;
    [[nodiscard]] std::optional<Reservation> find(std::string_view id) const override;
    [[nodiscard]] std::vector<Reservation> findActiveExpiredBy(std::int64_t now) const override;

private:
    [[nodiscard]] Reservation readAt(std::uint64_t offset) const;

    FixedRecordFile file_;
    BTree& index_;
};

}  // namespace tecflix::users
