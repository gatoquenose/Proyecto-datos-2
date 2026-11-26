#include "tecflix/telemetry/TelemetrySetup.hpp"

#include <algorithm>
#include <exception>
#include <iostream>

#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/view/instrument_selector_factory.h"
#include "opentelemetry/sdk/metrics/view/meter_selector_factory.h"
#include "opentelemetry/sdk/metrics/view/view_factory.h"
#include "opentelemetry/sdk/metrics/view/view_registry_factory.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/trace/batch_span_processor_options.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/propagation/http_trace_context.h"
#include "opentelemetry/trace/provider.h"

namespace tecflix::telemetry {

namespace otlp = opentelemetry::exporter::otlp;
namespace metrics_api = opentelemetry::metrics;
namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;

namespace {

constexpr const char* kTracesPath = "/v1/traces";
constexpr const char* kMetricsPath = "/v1/metrics";
constexpr std::chrono::milliseconds kMaxExportTimeout{10000};

// Límites de cubeta en segundos: de 1 ms a 10 s.
const std::vector<double> kDurationBoundaries{0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0};

std::string withPath(std::string endpoint, const char* path) {
    while (!endpoint.empty() && endpoint.back() == '/') {
        endpoint.pop_back();
    }
    return endpoint + path;
}

}  // namespace

struct TelemetrySetup::Providers {
    std::shared_ptr<trace_sdk::TracerProvider> tracer;
    std::shared_ptr<metrics_sdk::MeterProvider> meter;
};

TelemetrySetup::TelemetrySetup(const TelemetryConfig& config)
    : serviceName_(config.serviceName), serviceVersion_(config.serviceVersion) {
    if (config.otlpEndpoint.empty()) {
        return;  // desactivada: los proveedores globales siguen siendo los que no hacen nada
    }

    const auto resource = opentelemetry::sdk::resource::Resource::Create(
        {{"service.name", config.serviceName}, {"service.version", config.serviceVersion}});
    providers_ = std::make_unique<Providers>();

    // --- Trazas ---
    otlp::OtlpHttpExporterOptions traceOptions;
    traceOptions.url = withPath(config.otlpEndpoint, kTracesPath);
    auto spanProcessor =
        trace_sdk::BatchSpanProcessorFactory::Create(otlp::OtlpHttpExporterFactory::Create(traceOptions),
                                                     trace_sdk::BatchSpanProcessorOptions{});
    providers_->tracer = std::make_shared<trace_sdk::TracerProvider>(std::move(spanProcessor), resource);
    trace_api::Provider::SetTracerProvider(
        opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(providers_->tracer));

    // --- Métricas ---
    otlp::OtlpHttpMetricExporterOptions metricOptions;
    metricOptions.url = withPath(config.otlpEndpoint, kMetricsPath);
    metrics_sdk::PeriodicExportingMetricReaderOptions readerOptions;
    readerOptions.export_interval_millis = config.metricExportInterval;
    readerOptions.export_timeout_millis = (std::min)(kMaxExportTimeout, config.metricExportInterval / 2);
    auto reader = metrics_sdk::PeriodicExportingMetricReaderFactory::Create(
        otlp::OtlpHttpMetricExporterFactory::Create(metricOptions), readerOptions);

    providers_->meter = std::make_shared<metrics_sdk::MeterProvider>(metrics_sdk::ViewRegistryFactory::Create(), resource);
    providers_->meter->AddMetricReader(std::move(reader));
    for (const std::string& name : config.durationHistograms) {
        auto histogramConfig = std::make_shared<metrics_sdk::HistogramAggregationConfig>();
        histogramConfig->boundaries_ = kDurationBoundaries;
        providers_->meter->AddView(
            metrics_sdk::InstrumentSelectorFactory::Create(metrics_sdk::InstrumentType::kHistogram, name, "s"),
            metrics_sdk::MeterSelectorFactory::Create(config.serviceName, config.serviceVersion, ""),
            metrics_sdk::ViewFactory::Create(name, "", metrics_sdk::AggregationType::kHistogram, histogramConfig));
    }
    metrics_api::Provider::SetMeterProvider(opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider>(providers_->meter));

    // --- Propagación entre servicios: cabecera traceparent de W3C ---
    opentelemetry::context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
        opentelemetry::nostd::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>(
            new trace_api::propagation::HttpTraceContext()));
}

TelemetrySetup::~TelemetrySetup() {
    if (!providers_) {
        return;
    }
    try {
        providers_->tracer->ForceFlush();
        providers_->tracer->Shutdown();
        providers_->meter->ForceFlush();
        providers_->meter->Shutdown();
    } catch (const std::exception& error) {
        std::cerr << "no se pudo cerrar la telemetría: " << error.what() << '\n';
    }
    // Los proveedores globales vuelven a los que no hacen nada, por si alguien mide después.
    trace_api::Provider::SetTracerProvider(
        opentelemetry::nostd::shared_ptr<trace_api::TracerProvider>(new trace_api::NoopTracerProvider()));
    metrics_api::Provider::SetMeterProvider(
        opentelemetry::nostd::shared_ptr<metrics_api::MeterProvider>(new metrics_api::NoopMeterProvider()));
}

bool TelemetrySetup::enabled() const {
    return providers_ != nullptr;
}

opentelemetry::nostd::shared_ptr<metrics_api::Meter> TelemetrySetup::meter() const {
    return metrics_api::Provider::GetMeterProvider()->GetMeter(serviceName_, serviceVersion_);
}

}  // namespace tecflix::telemetry
