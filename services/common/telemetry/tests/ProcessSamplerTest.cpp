#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "tecflix/telemetry/ProcessSampler.hpp"
#include "tecflix/telemetry/ServerSpan.hpp"

namespace tecflix::telemetry {

TEST(ProcessSamplerTest, ReportsTheMemoryOfTheRunningProcess) {
    const ProcessSampler sampler;

    EXPECT_GT(sampler.memoryBytes(), 0);
}

TEST(ProcessSamplerTest, CpuIsHighWhileTheProcessIsBusyAndNeverNegative) {
    ProcessSampler sampler;
    static_cast<void>(sampler.cpuCores());

    // Se mantiene ocupada la CPU unos 100 ms.
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds{100};
    volatile unsigned long long counter = 0;
    while (std::chrono::steady_clock::now() < until) {
        counter = counter + 1;
    }

    EXPECT_GT(sampler.cpuCores(), 0.3);
    EXPECT_GE(sampler.cpuCores(), 0.0);
}

TEST(DirectorySizeTest, AddsUpTheFilesOfAFolderAndItsSubfolders) {
    const auto folder = std::filesystem::temp_directory_path() / "tecflix_dirsize_test";
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder / "sub");
    std::ofstream{folder / "a.bin"} << std::string(100, 'x');
    std::ofstream{folder / "sub" / "b.bin"} << std::string(250, 'y');

    EXPECT_EQ(directorySizeBytes(folder), 350);
    EXPECT_EQ(directorySizeBytes(folder / "no-existe"), 0);

    std::filesystem::remove_all(folder);
}

TEST(CurrentTraceTest, ThereIsNoTraceWhenTelemetryIsOff) {
    EXPECT_TRUE(currentTraceId().empty());
}

}  // namespace tecflix::telemetry
