#include "tecflix/telemetry/ProcessGauges.hpp"

namespace tecflix::telemetry {

std::unique_ptr<ProcessSampler> registerProcessGauges(GaugeRegistry& registry, const std::filesystem::path& dataDirectory) {
    auto sampler = std::make_unique<ProcessSampler>();
    ProcessSampler* process = sampler.get();
    registry.addDecimal("service_cpu_usage", "CPU que usa el servicio, en núcleos", "{core}",
                        [process] { return process->cpuCores(); });
    registry.addInteger("service_memory_usage", "Memoria física que usa el servicio", "By",
                        [process] { return process->memoryBytes(); });
    registry.addInteger("service_disk_usage", "Espacio que ocupan los archivos de datos del servicio", "By",
                        [dataDirectory] { return directorySizeBytes(dataDirectory); });
    return sampler;
}

}  // namespace tecflix::telemetry
