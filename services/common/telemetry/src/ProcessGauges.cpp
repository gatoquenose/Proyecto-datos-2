#include "tecflix/telemetry/ProcessGauges.hpp"

namespace tecflix::telemetry {

std::unique_ptr<ProcessSampler> registerProcessGauges(GaugeRegistry& registry, const std::filesystem::path& dataDirectory) {
    auto sampler = std::make_unique<ProcessSampler>();
    ProcessSampler* process = sampler.get();
    registry.addDecimal("service_cpu_usage", "CPU que usa el servicio, en núcleos", "{core}",
                        [process] { return process->cpuCores(); });
    // Sin unidad en estos dos: con "By", Prometheus les agrega "_bytes" y dejan de llamarse como pide la SPEC.
    // La unidad (bytes) queda en la descripción.
    registry.addInteger("service_memory_usage", "Memoria física que usa el servicio, en bytes", "",
                        [process] { return process->memoryBytes(); });
    registry.addInteger("service_disk_usage", "Espacio que ocupan los archivos de datos del servicio, en bytes", "",
                        [dataDirectory] { return directorySizeBytes(dataDirectory); });
    return sampler;
}

}  // namespace tecflix::telemetry
