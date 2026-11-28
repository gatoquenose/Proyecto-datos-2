#pragma once

#include <chrono>

#include "opentelemetry/context/context.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/unique_ptr.h"

namespace tecflix::metadata {

// Métricas propias de Metadata (nombres del SPEC): metadata_mongo_query_duration_seconds y
// metadata_video_list_duration_seconds.
class MetadataMetrics {
public:
    explicit MetadataMetrics(const opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>& meter)
        : mongo_(meter->CreateDoubleHistogram("metadata_mongo_query_duration_seconds", "Duración de las consultas a MongoDB", "s")),
          list_(meter->CreateDoubleHistogram("metadata_video_list_duration_seconds", "Duración del listado de videos", "s")) {}

    void recordMongo(double seconds) { mongo_->Record(seconds, opentelemetry::context::Context{}); }
    void recordList(double seconds) { list_->Record(seconds, opentelemetry::context::Context{}); }

private:
    opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<double>> mongo_;
    opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<double>> list_;
};

// Las rutas son funciones libres, así que las métricas se publican por aquí; si no se registran
// (por ejemplo, sin telemetría), medir no hace nada.
inline MetadataMetrics*& currentMetrics() {
    static MetadataMetrics* metrics = nullptr;
    return metrics;
}

inline void setCurrentMetrics(MetadataMetrics* metrics) { currentMetrics() = metrics; }

inline double secondsSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

inline void recordMongoSeconds(double seconds) {
    if (currentMetrics() != nullptr) currentMetrics()->recordMongo(seconds);
}

inline void recordListSeconds(double seconds) {
    if (currentMetrics() != nullptr) currentMetrics()->recordList(seconds);
}

}  // namespace tecflix::metadata
