#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace reverb::render {

struct ReverseCosmicRateMetrics final {
    double sampleRate {};
    std::size_t onsetFrame {};
    double onsetMilliseconds {};
    double peakTimeMilliseconds {};
    double earlyImpulseEnergy {};
    double lateImpulseEnergy {};
    double finalToMidDecayDb {};
    double earlyChordOctaveDbfs {};
    double lateChordOctaveDbfs {};
    double earlyOctaveToFundamentalDb {};
    double lateOctaveToFundamentalDb {};
    double octaveGrowthDb {};
    double stereoCorrelation {};
    double monoCompatibility {};
    double impulsePeak {};
    double chordPeak {};
    double noisePeak {};
    bool finite {};
    bool causal {};
    std::uint64_t impulseHash {};
    std::uint64_t chordHash {};
    std::uint64_t noiseHash {};
};

struct ReverseCosmicShimmerValidationReport final {
    double durationSeconds {};
    std::vector<ReverseCosmicRateMetrics> rates;
};

[[nodiscard]] ReverseCosmicShimmerValidationReport measureReverseCosmicShimmerValidation();
[[nodiscard]] std::string writeReverseCosmicShimmerValidationJson(
    const ReverseCosmicShimmerValidationReport& report);
void writeReverseCosmicShimmerFixtures(
    const ReverseCosmicShimmerValidationReport& report,
    const std::string& outputDirectory);

} // namespace reverb::render
