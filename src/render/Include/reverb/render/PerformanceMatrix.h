#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace reverb::render {

struct PerformanceCaseRequest final {
    std::string graphId;
    double sampleRate {};
    std::size_t blockSize {};
    std::size_t measuredBlocks { 200 };
    std::size_t crossfadeRepetitions { 20 };
};

struct CallbackDistribution final {
    std::size_t sampleCount {};
    double medianMicroseconds {};
    double percentile95Microseconds {};
    double peakMicroseconds {};
    double medianLoadPercent {};
    double percentile95LoadPercent {};
    double peakLoadPercent {};
    std::size_t underruns {};
};

struct PerformanceCaseResult final {
    PerformanceCaseRequest request;
    CallbackDistribution normal;
    CallbackDistribution topologyCrossfade;
    double crossfadeMedianOverheadRatio {};
    std::size_t graphLatencySamples {};
    std::size_t preparedMemoryBytes {};
    std::size_t nodeCount {};
    std::size_t connectionCount {};
    std::size_t feedbackRegionCount {};
    std::size_t blockWiseRegionCount {};
    std::size_t sampleWiseRegionCount {};
    std::size_t estimatedOperationsPerSample {};
    std::string executionDomain;
    std::uint64_t validationMicroseconds {};
    std::uint64_t schedulingMicroseconds {};
    std::uint64_t preparationMicroseconds {};
    std::uint64_t compileMicroseconds {};
    std::uint64_t requestToActiveMicroseconds {};
    bool finiteOutput { true };
    bool withinNormalBudget {};
    bool withinCrossfadeBudget {};
};

[[nodiscard]] PerformanceCaseResult measurePerformanceCase(const PerformanceCaseRequest& request);
[[nodiscard]] std::string performanceMatrixJson(
    const std::vector<PerformanceCaseResult>& results,
    std::string machineLabel,
    std::string toolchain,
    std::string buildCommit);
void writePerformanceMatrix(
    const std::filesystem::path& output,
    const std::vector<PerformanceCaseResult>& results,
    std::string machineLabel,
    std::string toolchain,
    std::string buildCommit);

} // namespace reverb::render
