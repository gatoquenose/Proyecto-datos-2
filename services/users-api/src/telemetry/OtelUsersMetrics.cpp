#include "telemetry/OtelUsersMetrics.hpp"

#include "opentelemetry/context/context.h"

namespace tecflix::users {

OtelUsersMetrics::OtelUsersMetrics(const opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>& meter)
    : pagesRead_(meter->CreateUInt64Counter("users_disk_pages_read_total", "Páginas del índice leídas del disco", "{page}")),
      pagesWritten_(
          meter->CreateUInt64Counter("users_disk_pages_written_total", "Páginas del índice escritas en el disco", "{page}")),
      created_(meter->CreateUInt64Counter("users_created_total", "Usuarios registrados", "{user}")),
      deleted_(meter->CreateUInt64Counter("users_deleted_total", "Usuarios eliminados", "{user}")),
      searchDuration_(meter->CreateDoubleHistogram("users_btree_search_duration_seconds",
                                                   "Duración de las búsquedas de usuarios en el índice de árbol B", "s")) {}

void OtelUsersMetrics::pageRead() {
    pagesRead_->Add(1);
}

void OtelUsersMetrics::pageWritten() {
    pagesWritten_->Add(1);
}

void OtelUsersMetrics::searchFinished(std::string_view lookup, double durationSeconds) {
    const std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue> labels[] = {
        {"lookup", opentelemetry::nostd::string_view{lookup.data(), lookup.size()}}};
    searchDuration_->Record(durationSeconds, opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels},
                            opentelemetry::context::Context{});
}

void OtelUsersMetrics::userCreated() {
    created_->Add(1);
}

void OtelUsersMetrics::userDeleted() {
    deleted_->Add(1);
}

}  // namespace tecflix::users
