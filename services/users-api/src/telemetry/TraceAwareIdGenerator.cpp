#include "telemetry/TraceAwareIdGenerator.hpp"

#include "tecflix/telemetry/ServerSpan.hpp"

namespace tecflix::users {

TraceAwareIdGenerator::TraceAwareIdGenerator(const IIdGenerator& fallback) : fallback_(fallback) {}

std::string TraceAwareIdGenerator::next() const {
    std::string traceId = tecflix::telemetry::currentTraceId();
    return traceId.empty() ? fallback_.next() : traceId;
}

}  // namespace tecflix::users
