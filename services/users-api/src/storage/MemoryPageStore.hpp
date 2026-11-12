#pragma once

#include <vector>

#include "storage/IPageStore.hpp"

namespace tecflix::users {

// Almacén en memoria, pensado para pruebas rápidas.
class MemoryPageStore final : public IPageStore {
public:
    [[nodiscard]] PageId pageCount() const override;
    PageId allocate() override;
    void read(PageId id, PageBuffer& buffer) const override;
    void write(PageId id, const PageBuffer& buffer) override;

private:
    std::vector<PageBuffer> pages_;
};

}  // namespace tecflix::users
