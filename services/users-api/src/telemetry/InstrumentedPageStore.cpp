#include "telemetry/InstrumentedPageStore.hpp"

namespace tecflix::users {

InstrumentedPageStore::InstrumentedPageStore(IPageStore& inner, IUsersMetrics& metrics)
    : inner_(inner), metrics_(metrics) {}

PageId InstrumentedPageStore::pageCount() const {
    return inner_.pageCount();
}

PageId InstrumentedPageStore::allocate() {
    const PageId id = inner_.allocate();
    metrics_.pageWritten();  // crear una página es escribirla
    return id;
}

void InstrumentedPageStore::read(PageId id, PageBuffer& buffer) const {
    inner_.read(id, buffer);
    metrics_.pageRead();
}

void InstrumentedPageStore::write(PageId id, const PageBuffer& buffer) {
    inner_.write(id, buffer);
    metrics_.pageWritten();
}

}  // namespace tecflix::users
