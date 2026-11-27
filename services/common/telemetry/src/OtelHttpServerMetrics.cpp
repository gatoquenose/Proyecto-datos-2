#include "tecflix/telemetry/OtelHttpServerMetrics.hpp"

#include <string>

#include "opentelemetry/context/context.h"

namespace tecflix::telemetry {

namespace {

constexpr int kFirstErrorStatus = 400;

opentelemetry::nostd::string_view view(std::string_view text) {
    return {text.data(), text.size()};
}

}  // namespace

OtelHttpServerMetrics::OtelHttpServerMetrics(const opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>& meter)
    : requests_(meter->CreateUInt64Counter("http_requests_total", "Peticiones HTTP atendidas", "{request}")),
      errors_(meter->CreateUInt64Counter("http_request_errors_total", "Peticiones HTTP respondidas con error (4xx y 5xx)",
                                         "{request}")),
      duration_(meter->CreateDoubleHistogram("http_request_duration_seconds", "Duración de las peticiones HTTP", "s")) {}

void OtelHttpServerMetrics::record(std::string_view method, std::string_view route, int status, double durationSeconds) {
    const std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue> labels[] = {
        {"http.request.method", view(method)}, {"http.route", view(route)}, {"http.response.status_code", status}};
    const opentelemetry::common::KeyValueIterableView<decltype(labels)> attributes{labels};

    requests_->Add(1, attributes);
    if (status >= kFirstErrorStatus) {
        errors_->Add(1, attributes);
    }
    duration_->Record(durationSeconds, attributes, opentelemetry::context::Context{});
}

}  // namespace tecflix::telemetry
