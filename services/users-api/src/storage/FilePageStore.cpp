#include "storage/FilePageStore.hpp"

#include <stdexcept>

namespace tecflix::users {

namespace {

std::uint64_t offsetOf(PageId id) {
    return static_cast<std::uint64_t>(id) * kPageSize;
}

}  // namespace

FilePageStore::FilePageStore(const std::filesystem::path& path) : file_(path) {
    const std::uint64_t size = file_.size();
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
    file_.writeAt(offsetOf(pageCount_), zeros);
    return pageCount_++;
}

void FilePageStore::read(PageId id, PageBuffer& buffer) const {
    checkRange(id);
    file_.readAt(offsetOf(id), buffer);
}

void FilePageStore::write(PageId id, const PageBuffer& buffer) {
    checkRange(id);
    file_.writeAt(offsetOf(id), buffer);
}

}  // namespace tecflix::users
