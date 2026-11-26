#include "tecflix/telemetry/GaugeRegistry.hpp"

#include "opentelemetry/metrics/observer_result.h"
#include "opentelemetry/nostd/variant.h"

namespace tecflix::telemetry {

namespace metrics_api = opentelemetry::metrics;
namespace nostd = opentelemetry::nostd;

struct GaugeRegistry::Entry {
    nostd::shared_ptr<metrics_api::ObservableInstrument> instrument;
    std::function<std::int64_t()> integerSample;  // solo uno de los dos tiene valor
    std::function<double()> decimalSample;
};

GaugeRegistry::GaugeRegistry(nostd::shared_ptr<metrics_api::Meter> meter) : meter_(std::move(meter)) {}

GaugeRegistry::~GaugeRegistry() {
    for (const auto& entry : entries_) {
        entry->instrument->RemoveCallback(&GaugeRegistry::observe, entry.get());
    }
}

void GaugeRegistry::observe(metrics_api::ObserverResult result, void* state) {
    const auto* entry = static_cast<const Entry*>(state);
    try {
        if (entry->integerSample) {
            nostd::get<nostd::shared_ptr<metrics_api::ObserverResultT<std::int64_t>>>(result)->Observe(entry->integerSample());
        } else {
            nostd::get<nostd::shared_ptr<metrics_api::ObserverResultT<double>>>(result)->Observe(entry->decimalSample());
        }
    } catch (...) {
        // Si una medición falla, ese valor simplemente no se informa; el servicio sigue funcionando.
    }
}

void GaugeRegistry::addInteger(const std::string& name, const std::string& description, const std::string& unit,
                               std::function<std::int64_t()> sample) {
    auto entry = std::make_unique<Entry>();
    entry->integerSample = std::move(sample);
    entry->instrument = meter_->CreateInt64ObservableGauge(name, description, unit);
    entry->instrument->AddCallback(&GaugeRegistry::observe, entry.get());
    entries_.push_back(std::move(entry));
}

void GaugeRegistry::addDecimal(const std::string& name, const std::string& description, const std::string& unit,
                               std::function<double()> sample) {
    auto entry = std::make_unique<Entry>();
    entry->decimalSample = std::move(sample);
    entry->instrument = meter_->CreateDoubleObservableGauge(name, description, unit);
    entry->instrument->AddCallback(&GaugeRegistry::observe, entry.get());
    entries_.push_back(std::move(entry));
}

}  // namespace tecflix::telemetry
