#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "opentelemetry/metrics/async_instruments.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/observer_result.h"
#include "opentelemetry/nostd/shared_ptr.h"

namespace tecflix::telemetry {

// Medidores que OpenTelemetry consulta periódicamente (uso de memoria, de disco, etc.).
// Cada medidor se define con una función que devuelve el valor actual. Al destruirse el registro
// se quitan todas las funciones, así el SDK no las llama cuando sus datos ya no existen.
class GaugeRegistry {
public:
    explicit GaugeRegistry(opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter);
    ~GaugeRegistry();

    GaugeRegistry(const GaugeRegistry&) = delete;
    GaugeRegistry& operator=(const GaugeRegistry&) = delete;
    GaugeRegistry(GaugeRegistry&&) = delete;
    GaugeRegistry& operator=(GaugeRegistry&&) = delete;

    void addInteger(const std::string& name, const std::string& description, const std::string& unit,
                    std::function<std::int64_t()> sample);
    void addDecimal(const std::string& name, const std::string& description, const std::string& unit,
                    std::function<double()> sample);

private:
    struct Entry;

    static void observe(opentelemetry::metrics::ObserverResult result, void* state);

    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter_;
    std::vector<std::unique_ptr<Entry>> entries_;
};

}  // namespace tecflix::telemetry
