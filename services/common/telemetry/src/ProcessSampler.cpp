#include "tecflix/telemetry/ProcessSampler.hpp"

#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <time.h>
#include <unistd.h>

#include <fstream>
#endif

namespace tecflix::telemetry {

namespace {

#ifdef _WIN32

constexpr double kFileTimeTicksPerSecond = 1e7;  // FILETIME cuenta de 100 ns en 100 ns

double fileTimeSeconds(const FILETIME& time) {
    ULARGE_INTEGER ticks;
    ticks.LowPart = time.dwLowDateTime;
    ticks.HighPart = time.dwHighDateTime;
    return static_cast<double>(ticks.QuadPart) / kFileTimeTicksPerSecond;
}

double processCpuSeconds() {
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user) == 0) {
        return 0.0;
    }
    return fileTimeSeconds(kernel) + fileTimeSeconds(user);
}

std::int64_t processMemoryBytes() {
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) == 0) {
        return 0;
    }
    return static_cast<std::int64_t>(counters.WorkingSetSize);
}

#else

constexpr double kNanosecondsPerSecond = 1e9;

double processCpuSeconds() {
    timespec time{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time) != 0) {
        return 0.0;
    }
    return static_cast<double>(time.tv_sec) + static_cast<double>(time.tv_nsec) / kNanosecondsPerSecond;
}

std::int64_t processMemoryBytes() {
    // /proc/self/statm: tamaño total y páginas residentes (la segunda cifra).
    std::ifstream statm{"/proc/self/statm"};
    std::int64_t totalPages = 0;
    std::int64_t residentPages = 0;
    if (!(statm >> totalPages >> residentPages)) {
        return 0;
    }
    return residentPages * static_cast<std::int64_t>(sysconf(_SC_PAGESIZE));
}

#endif

}  // namespace

ProcessSampler::ProcessSampler() : lastCpuSeconds_(processCpuSeconds()), lastWall_(std::chrono::steady_clock::now()) {}

std::int64_t ProcessSampler::memoryBytes() const {
    return processMemoryBytes();
}

double ProcessSampler::cpuCores() {
    const std::scoped_lock lock{mutex_};
    const double cpuSeconds = processCpuSeconds();
    const auto now = std::chrono::steady_clock::now();
    const double wallSeconds = std::chrono::duration<double>(now - lastWall_).count();
    const double cores = wallSeconds > 0.0 ? (cpuSeconds - lastCpuSeconds_) / wallSeconds : 0.0;
    lastCpuSeconds_ = cpuSeconds;
    lastWall_ = now;
    return cores < 0.0 ? 0.0 : cores;
}

std::int64_t directorySizeBytes(const std::filesystem::path& directory) {
    std::error_code error;
    std::int64_t total = 0;
    for (std::filesystem::recursive_directory_iterator it{directory, error}, end; !error && it != end; it.increment(error)) {
        std::error_code fileError;
        if (it->is_regular_file(fileError) && !fileError) {
            total += static_cast<std::int64_t>(it->file_size(fileError));
        }
    }
    return total;
}

}  // namespace tecflix::telemetry
