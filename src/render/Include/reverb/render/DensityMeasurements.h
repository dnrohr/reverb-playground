#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace reverb::render {

struct DensityWindow final {
    double startSeconds {};
    double echoDensity {};
    std::size_t activePeakCount {};
    double crestFactor {};
    double kurtosis {};
    double energyVariation {};
    double recurrence {};
    double recurrenceMilliseconds {};
    double spectralFlatness {};
    double stereoCorrelation {};
};

struct DensityRegion final {
    std::string name;
    double startSeconds {};
    double endSeconds {};
    double echoDensity {};
    double activePeaksPerSecond {};
    double crestFactor {};
    double kurtosis {};
    double energyVariation {};
    double recurrence {};
    double recurrenceMilliseconds {};
    double spectralFlatness {};
    double stereoCorrelation {};
};

struct DensityMeasurements final {
    std::string engineVersion;
    std::string patchId;
    double sampleRate {};
    std::size_t frameCount {};
    double windowMilliseconds { 40.0 };
    double hopMilliseconds { 20.0 };
    std::vector<DensityWindow> windows;
    std::vector<DensityRegion> regions;
};

[[nodiscard]] DensityMeasurements measureDensity(
    std::span<const float> left,
    std::span<const float> right,
    double sampleRate,
    double windowMilliseconds = 40.0,
    double hopMilliseconds = 20.0);

[[nodiscard]] std::string writeDensityMeasurementsJson(const DensityMeasurements& measurements);

} // namespace reverb::render
