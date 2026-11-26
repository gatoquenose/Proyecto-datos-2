#include "tecflix/telemetry/ServerSpan.hpp"

#include <array>
#include <map>

#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/tracer.h"

namespace tecflix::telemetry {

namespace trace_api = opentelemetry::trace;
namespace nostd = opentelemetry::nostd;

namespace {

constexpr int kFirstServerError = 500;

// Deja que OpenTelemetry lea los headers "traceparent" y "tracestate" de la petición.
// OpenTelemetry pide ambos seguidos y conserva el primero mientras pide el segundo, así que cada
// valor se guarda por separado (los nodos de std::map no cambian de lugar).
class HeaderCarrier final : public opentelemetry::context::propagation::TextMapCarrier {
public:
    explicit HeaderCarrier(const HeaderLookup& lookup) : lookup_(lookup) {}

    nostd::string_view Get(nostd::string_view key) const noexcept override {
        try {
            const std::string name{key.data(), key.size()};
            auto found = values_.find(name);
            if (found == values_.end()) {
                found = values_.emplace(name, lookup_(name)).first;
            }
            return found->second;
        } catch (...) {
            return {};
        }
    }
    void Set(nostd::string_view /*key*/, nostd::string_view /*value*/) noexcept override {}

private:
    const HeaderLookup& lookup_;
    mutable std::map<std::string, std::string> values_;
};

nostd::string_view view(std::string_view text) {
    return {text.data(), text.size()};
}

nostd::shared_ptr<trace_api::Span> startSpan(std::string_view instrumentationName, std::string_view method,
                                             std::string_view route, std::string_view path, const HeaderLookup& header) {
    const auto tracer = trace_api::Provider::GetTracerProvider()->GetTracer(view(instrumentationName));

    const HeaderCarrier carrier{header};
    const auto propagator = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
    auto current = opentelemetry::context::RuntimeContext::GetCurrent();
    const auto context = propagator->Extract(carrier, current);

    trace_api::StartSpanOptions options;
    options.kind = trace_api::SpanKind::kServer;
    options.parent = trace_api::GetSpan(context)->GetContext();

    const std::string name = std::string{method} + " " + std::string{route};
    return tracer->StartSpan(view(name),
                             {{"http.request.method", view(method)}, {"http.route", view(route)}, {"url.path", view(path)}},
                             options);
}

std::string toHex(const trace_api::SpanContext& context, bool trace) {
    if (!context.IsValid()) {
        return {};
    }
    if (trace) {
        std::array<char, 2 * trace_api::TraceId::kSize> buffer{};
        context.trace_id().ToLowerBase16(buffer);
        return {buffer.data(), buffer.size()};
    }
    std::array<char, 2 * trace_api::SpanId::kSize> buffer{};
    context.span_id().ToLowerBase16(buffer);
    return {buffer.data(), buffer.size()};
}

}  // namespace

ServerSpan::ServerSpan(std::string_view instrumentationName, std::string_view method, std::string_view route,
                       std::string_view path, const HeaderLookup& header)
    : span_(startSpan(instrumentationName, method, route, path, header)), scope_(span_) {}

ServerSpan::~ServerSpan() {
    span_->End();
}

void ServerSpan::finish(int httpStatus) {
    span_->SetAttribute("http.response.status_code", httpStatus);
    if (httpStatus >= kFirstServerError) {
        span_->SetStatus(trace_api::StatusCode::kError);
    }
}

std::string ServerSpan::traceId() const {
    return toHex(span_->GetContext(), true);
}

std::string ServerSpan::spanId() const {
    return toHex(span_->GetContext(), false);
}

std::string currentTraceId() {
    return toHex(trace_api::Tracer::GetCurrentSpan()->GetContext(), true);
}

}  // namespace tecflix::telemetry
