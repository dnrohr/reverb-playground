#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace reverb::render {

struct PitchShiftDirectionMetrics final {
    std::string direction;
    double measuredOctaveCents {};
    double referenceTargetDbfs {};
    double foldedAliasDbfs {};
    double aliasRelativeToReferenceDb {};
    double measuredCpuRealtimeLoadPercent {};
    std::uint64_t processedFrames {};
    std::uint64_t elapsedMicroseconds {};
};

struct PitchShiftBenchmarkMetrics final {
    std::string scenario;
    std::uint64_t processedFrames {};
    std::uint64_t elapsedMicroseconds {};
    double realtimeLoadPercent {};
};

struct PitchShiftRateMetrics final {
    double sampleRate {};
    std::size_t latencySamples {};
    double latencyMilliseconds {};
    std::size_t storageSamples {};
    std::size_t storageBytes {};
    PitchShiftDirectionMetrics forward;
    PitchShiftDirectionMetrics reverse;
    double pairedPhaseA {};
    double pairedPhaseB {};
    double pairedOutputCorrelation {};
    bool pairedResetDeterministic {};
    bool causalBeforeDeclaredLatency {};
    double forwardTransientPeakEnvelopeStep {};
    double reverseTransientPeakEnvelopeStep {};
    double transientEnvelopeDifferenceRms {};
    std::vector<PitchShiftBenchmarkMetrics> benchmarks;
};

struct PitchShiftValidationReport final {
    std::string qualityId;
    std::string interpolation;
    double grainMilliseconds {};
    double overlap {};
    std::vector<PitchShiftRateMetrics> rates;
};

[[nodiscard]] PitchShiftValidationReport measurePitchShiftValidation();
[[nodiscard]] std::string writePitchShiftValidationJson(const PitchShiftValidationReport& report);

} // namespace reverb::render
