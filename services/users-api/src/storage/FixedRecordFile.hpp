#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

#include "storage/BinaryFile.hpp"

namespace tecflix::users {

// Archivo con una cabecera fija de 64 bytes (firma de 8 caracteres, versión y tamaño de registro)
// seguida de registros de tamaño fijo: el registro N empieza en 64 + N * tamaño.
// Lo usan users.dat y reservations.dat; cada uno convierte sus datos a bytes por su cuenta.
class FixedRecordFile {
public:
    static constexpr std::uint64_t kHeaderBytes = 64;
    static constexpr std::size_t kMagicBytes = 8;

    // Crea el archivo si no existe. Lanza std::runtime_error si la cabecera no coincide
    // o el tamaño no corresponde a registros completos.
    FixedRecordFile(const std::filesystem::path& path, std::string_view magic, std::size_t recordBytes);

    // Agrega un registro al final y devuelve su offset.
    std::uint64_t append(std::span<const std::uint8_t> record);

    // Lanza std::out_of_range si el offset no es el de un registro existente.
    void read(std::uint64_t offset, std::span<std::uint8_t> out) const;
    void write(std::uint64_t offset, std::span<const std::uint8_t> record);

    [[nodiscard]] std::uint64_t recordCount() const;
    [[nodiscard]] std::uint64_t offsetOfRecord(std::uint64_t ordinal) const;

private:
    void writeHeader();
    void validateHeader() const;
    void checkOffset(std::uint64_t offset) const;
    void checkSize(std::span<const std::uint8_t> bytes) const;

    BinaryFile file_;
    std::string magic_;
    std::size_t recordBytes_;
    std::uint64_t recordCount_ = 0;
};

}  // namespace tecflix::users
