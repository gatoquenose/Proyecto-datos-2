#include "storage/MemoryPageStore.hpp"

#include <stdexcept>

namespace tecflix::users {

PageId MemoryPageStore::pageCount() const {
    return static_cast<PageId>(pages_.size());
}

PageId MemoryPageStore::allocate() {
    pages_.emplace_back();
    pages_.back().fill(0);
    return static_cast<PageId>(pages_.size() - 1);
}

void MemoryPageStore::read(PageId id, PageBuffer& buffer) const {
    if (id >= pages_.size()) {
        throw std::out_of_range("página inexistente");
    }
    buffer = pages_[id];
}

void MemoryPageStore::write(PageId id, const PageBuffer& buffer) {
    if (id >= pages_.size()) {
        throw std::out_of_range("página inexistente");
    }
    pages_[id] = buffer;
}

}  // namespace tecflix::users
