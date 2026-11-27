#pragma once

#include <filesystem>
#include <memory>

#include "tecflix/telemetry/GaugeRegistry.hpp"
#include "tecflix/telemetry/ProcessSampler.hpp"

namespace tecflix::telemetry {

// Medidores que todo servicio publica (nombres del SPEC): service_cpu_usage (núcleos),
// service_memory_usage (bytes) y service_disk_usage (bytes que ocupan sus archivos de datos).
// Devuelve el medidor de proceso, que debe vivir mientras viva el registro.
std::unique_ptr<ProcessSampler> registerProcessGauges(GaugeRegistry& registry, const std::filesystem::path& dataDirectory);

}  // namespace tecflix::telemetry
