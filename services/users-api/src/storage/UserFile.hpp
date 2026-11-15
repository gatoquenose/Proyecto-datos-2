#pragma once

#include <cstdint>
#include <filesystem>

#include "domain/User.hpp"
#include "storage/BinaryFile.hpp"

namespace tecflix::users {

// users.dat: una cabecera fija y después un registro por usuario, uno tras otro.
// Los registros nuevos se agregan al final; borrar solo cambia el estado del registro.
class UserFile {
public:
    // Crea el archivo si no existe. Lanza std::runtime_error si la cabecera no es
    // válida o el tamaño no corresponde a registros completos.
    explicit UserFile(const std::filesystem::path& path);

    // Agrega el usuario al final y devuelve el offset de su registro.
    std::uint64_t append(const User& user);

    // Lanza std::out_of_range si el offset no es el de un registro, y
    // std::runtime_error si el registro está eliminado o dañado.
    [[nodiscard]] User read(std::uint64_t offset) const;

    // Reemplaza el contenido de un registro activo (mismo tamaño, mismo offset).
    void overwrite(std::uint64_t offset, const User& user);

    void markDeleted(std::uint64_t offset);

    [[nodiscard]] std::uint64_t recordCount() const;

private:
    void writeHeader();
    void validateHeader() const;
    void checkOffset(std::uint64_t offset) const;

    BinaryFile file_;
    std::uint64_t recordCount_ = 0;
};

}  // namespace tecflix::users
