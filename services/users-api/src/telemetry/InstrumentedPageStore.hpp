#pragma once

#include "storage/IPageStore.hpp"
#include "telemetry/IUsersMetrics.hpp"

namespace tecflix::users {

// Cuenta las páginas que el índice lee y escribe en el disco (users_disk_pages_read_total).
class InstrumentedPageStore final : public IPageStore {
public:
    InstrumentedPageStore(IPageStore& inner, IUsersMetrics& metrics);

    [[nodiscard]] PageId pageCount() const override;
    PageId allocate() override;
    void read(PageId id, PageBuffer& buffer) const override;
    void write(PageId id, const PageBuffer& buffer) override;

private:
    IPageStore& inner_;
    IUsersMetrics& metrics_;
};

}  // namespace tecflix::users
