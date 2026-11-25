#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>

namespace tecflix::users {

// Archivo binario con lectura y escritura en posiciones exactas (offsets).
// Lo comparten el índice (FilePageStore) y el archivo de usuarios (UserFile).
class BinaryFile {
public:
    // Abre el archivo (lo crea si no existe). Lanza std::runtime_error si falla.
    explicit BinaryFile(const std::filesystem::path& path);

    [[nodiscard]] std::uint64_t size() const;
    void readAt(std::uint64_t offset, std::span<std::uint8_t> out) const;
    void writeAt(std::uint64_t offset, std::span<const std::uint8_t> data);

private:
    // mutable: leer mueve el cursor del archivo, pero no cambia su contenido.
    mutable std::fstream file_;
};

}  // namespace tecflix::users
