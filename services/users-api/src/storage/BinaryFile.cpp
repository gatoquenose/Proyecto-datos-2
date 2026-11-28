#include "storage/BinaryFile.hpp"

#include <ios>
#include <stdexcept>

namespace tecflix::users {

BinaryFile::BinaryFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        const std::ofstream create{path, std::ios::binary};
        if (!create) {
            throw std::runtime_error("no se pudo crear el archivo");
        }
    }
    file_.open(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_) {
        throw std::runtime_error("no se pudo abrir el archivo");
    }
}

std::uint64_t BinaryFile::size() const {
    file_.clear();
    file_.seekg(0, std::ios::end);
    const std::streamoff end = file_.tellg();
    if (!file_ || end < 0) {
        throw std::runtime_error("no se pudo medir el archivo");
    }
    return static_cast<std::uint64_t>(end);
}

void BinaryFile::readAt(std::uint64_t offset, std::span<std::uint8_t> out) const {
    file_.clear();
    file_.seekg(static_cast<std::streamoff>(offset));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): iostream trabaja con char; los bytes son los mismos.
    file_.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    if (!file_) {
        throw std::runtime_error("no se pudo leer el archivo");
    }
}

void BinaryFile::writeAt(std::uint64_t offset, std::span<const std::uint8_t> data) {
    file_.clear();
    file_.seekp(static_cast<std::streamoff>(offset));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): iostream trabaja con char; los bytes son los mismos.
    file_.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    file_.flush();
    if (!file_) {
        throw std::runtime_error("no se pudo escribir el archivo");
    }
}

}  // namespace tecflix::users
