#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>

#include "domain/User.hpp"
#include "storage/FixedRecordFile.hpp"
#include "storage/UserRecordCodec.hpp"

namespace tecflix::users {

// users.dat: un registro por usuario, uno tras otro. Los registros nuevos se agregan al
// final; borrar solo cambia el estado del registro.
// Qué se encontró al leer un registro del archivo.
enum class RecordState { Active, Deleted, Corrupt };

struct ScannedRecord {
    std::uint64_t offset{};
    RecordState state{};
    std::optional<User> user;  // solo cuando el registro está activo
};

class UserFile {
public:
    // Crea el archivo si no existe. Lanza std::runtime_error si la cabecera no es válida
    // o el tamaño no corresponde a registros completos.
    explicit UserFile(const std::filesystem::path& path);

    // Agrega el usuario al final y devuelve el offset de su registro.
    std::uint64_t append(const User& user);

    // Lanza std::out_of_range si el offset no es el de un registro, y
    // std::runtime_error si el registro está eliminado o dañado.
    [[nodiscard]] User read(std::uint64_t offset) const;

    // Reemplaza el contenido de un registro activo (mismo tamaño, mismo offset).
    void overwrite(std::uint64_t offset, const User& user);

    void markDeleted(std::uint64_t offset);

    // Recorre los usuarios que no están eliminados, en el orden del archivo.
    // Los registros dañados se omiten: scan() es la forma de verlos.
    void forEachActive(const std::function<void(const User&)>& visit) const;

    // Recorre TODOS los registros en orden, sin lanzar por uno dañado: lo marca Corrupt y sigue.
    void scan(const std::function<void(const ScannedRecord&)>& visit) const;

    [[nodiscard]] std::uint64_t recordCount() const;

    // Acceso a los bytes del registro tal cual, sin interpretarlos. Lo usa el repositorio para
    // anotar y rehacer operaciones en el WAL: rehacer debe dejar bytes idénticos, no "equivalentes".
    // Offset donde se agregaría el próximo registro.
    [[nodiscard]] std::uint64_t nextOffset() const;
    [[nodiscard]] RecordBuffer readImage(std::uint64_t offset) const;
    void appendImage(const RecordBuffer& image);
    void writeImage(std::uint64_t offset, const RecordBuffer& image);

private:
    FixedRecordFile file_;
};

}  // namespace tecflix::users
