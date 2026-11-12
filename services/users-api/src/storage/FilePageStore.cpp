#include "storage/FilePageStore.hpp"

#include <ios>
#include <stdexcept>

namespace tecflix::users {

namespace {

std::streamoff offsetOf(PageId id) {
    return static_cast<std::streamoff>(id) * static_cast<std::streamoff>(kPageSize);
}

}  // namespace

FilePageStore::FilePageStore(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        const std::ofstream create{path, std::ios::binary};
        if (!create) {
            throw std::runtime_error("no se pudo crear el archivo de páginas");
        }
    }
    file_.open(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_) {
        throw std::runtime_error("no se pudo abrir el archivo de páginas");
    }
    const auto size = std::filesystem::file_size(path);
    if (size % kPageSize != 0) {
        throw std::runtime_error("el archivo de páginas está corrupto (tamaño inválido)");
    }
    pageCount_ = static_cast<PageId>(size / kPageSize);
}

PageId FilePageStore::pageCount() const {
    return pageCount_;
}

void FilePageStore::checkRange(PageId id) const {
    if (id >= pageCount_) {
        throw std::out_of_range("página inexistente");
    }
}

PageId FilePageStore::allocate() {
    const PageBuffer zeros{};
    file_.clear();
    file_.seekp(offsetOf(pageCount_));
    file_.write(reinterpret_cast<const char*>(zeros.data()),
                static_cast<std::streamsize>(zeros.size()));
    file_.flush();
    if (!file_) {
        throw std::runtime_error("no se pudo ampliar el archivo de páginas");
    }
    return pageCount_++;
}

void FilePageStore::read(PageId id, PageBuffer& buffer) const {
    checkRange(id);
    file_.clear();
    file_.seekg(offsetOf(id));
    file_.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    if (!file_) {
        throw std::runtime_error("no se pudo leer la página");
    }
}

void FilePageStore::write(PageId id, const PageBuffer& buffer) {
    checkRange(id);
    file_.clear();
    file_.seekp(offsetOf(id));
    file_.write(reinterpret_cast<const char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
    file_.flush();
    if (!file_) {
        throw std::runtime_error("no se pudo escribir la página");
    }
}

}  // namespace tecflix::users
