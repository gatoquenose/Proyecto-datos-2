#pragma once

#include <cstdint>
#include <string_view>

#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "telemetry/IUsersMetrics.hpp"

namespace tecflix::users {

// Medidores del SPEC para usuarios: users_btree_search_duration_seconds, users_disk_pages_read_total,
// users_created_total y users_deleted_total (más users_disk_pages_written_total).
class OtelUsersMetrics final : public IUsersMetrics {
public:
    explicit OtelUsersMetrics(const opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>& meter);

    void pageRead() override;
    void pageWritten() override;
    void searchFinished(std::string_view lookup, double durationSeconds) override;
    void userCreated() override;
    void userDeleted() override;

private:
    opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<std::uint64_t>> pagesRead_;
    opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<std::uint64_t>> pagesWritten_;
    opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<std::uint64_t>> created_;
    opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<std::uint64_t>> deleted_;
    opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<double>> searchDuration_;
};

}  // namespace tecflix::users
