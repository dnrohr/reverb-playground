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

struct ProcessorFamilyCost final {
    std::string family;
    std::size_t nodeCount {};
    std::size_t modelUnitsPerSample {};
    double attributedMedianMicroseconds {};
    double attributedPercentile95Microseconds {};
    double attributedSharePercent {};
};

struct PerformanceCaseResult final {
    PerformanceCaseRequest request;
    CallbackDistribution normal;
    CallbackDistribution telemetryEnabled;
    CallbackDistribution topologyCrossfade;
    double telemetryMedianOverheadRatio {};
    double crossfadeMedianOverheadRatio {};
    std::size_t graphLatencySamples {};
    std::size_t preparedMemoryBytes {};
    std::size_t delayMemoryBytes {};
    std::size_t nodeCount {};
    std::size_t connectionCount {};
    std::size_t feedbackRegionCount {};
    std::size_t blockWiseRegionCount {};
    std::size_t sampleWiseRegionCount {};
    std::size_t logicalAudioBufferCount {};
    std::size_t logicalSignalCount {};
    std::size_t elidedNonAudioBufferCount {};
    std::size_t physicalAudioBufferCount {};
    std::size_t peakLiveBufferCount {};
    std::size_t bufferBytesSaved {};
    std::size_t inPlaceAliasCount {};
    std::size_t copiesAvoided {};
    std::size_t fusedKernelCount {};
    std::size_t fusedNodeCount {};
    std::size_t sampleWiseFusedKernelCount {};
    std::size_t simdKernelCount {};
    std::size_t estimatedOperationsPerSample {};
    std::vector<ProcessorFamilyCost> processorFamilies;
    std::string dominantProcessorFamily;
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

struct BarrExecutionComparison final {
    double sampleRate {};
    std::size_t blockSize {};
    std::size_t measuredBlocks {};
    CallbackDistribution directReference;
    CallbackDistribution optimizedGeneric;
    double genericToDirectP95Ratio {};
    bool sampleEquivalent {};
    bool finiteOutput {};
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
[[nodiscard]] BarrExecutionComparison measureBarrExecutionComparison(
    double sampleRate, std::size_t blockSize, std::size_t measuredBlocks = 2'000);
[[nodiscard]] std::string barrExecutionComparisonJson(
    const std::vector<BarrExecutionComparison>& results,
    std::string machineLabel,
    std::string toolchain,
    std::string buildCommit);

} // namespace reverb::render
