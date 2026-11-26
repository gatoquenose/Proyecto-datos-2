#pragma once

#include <cstdint>
#include <string_view>

#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "tecflix/telemetry/IHttpServerMetrics.hpp"

namespace tecflix::telemetry {

// Métricas HTTP del SPEC: http_requests_total, http_request_errors_total (respuestas 4xx y 5xx)
// y http_request_duration_seconds.
class OtelHttpServerMetrics final : public IHttpServerMetrics {
public:
    explicit OtelHttpServerMetrics(const opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>& meter);

    void record(std::string_view method, std::string_view route, int status, double durationSeconds) override;

private:
    opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<std::uint64_t>> requests_;
    opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<std::uint64_t>> errors_;
    opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<double>> duration_;
};

}  // namespace tecflix::telemetry
