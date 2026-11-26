#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>

namespace tecflix::telemetry {

// Mide cuánto consume este proceso. Funciona en Windows y en Linux.
class ProcessSampler {
public:
    ProcessSampler();

    // Memoria física que usa el proceso, en bytes.
    [[nodiscard]] std::int64_t memoryBytes() const;

    // CPU usada desde la medición anterior, en núcleos: 1.0 = un núcleo completo ocupado
    // todo el tiempo. La primera llamada mide desde que se creó el objeto.
    [[nodiscard]] double cpuCores();

private:
    std::mutex mutex_;
    double lastCpuSeconds_;
    std::chrono::steady_clock::time_point lastWall_;
};

// Tamaño total de los archivos de una carpeta (0 si no existe).
[[nodiscard]] std::int64_t directorySizeBytes(const std::filesystem::path& directory);

}  // namespace tecflix::telemetry
