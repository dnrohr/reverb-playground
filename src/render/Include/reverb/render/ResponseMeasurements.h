#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace reverb::render {

struct DecayPoint final {
    std::size_t frame {};
    double seconds {};
    double decibels {};
};

struct ResponseMeasurements final {
    std::string engineVersion;
    std::string patchId;
    std::size_t frameCount {};
    double sampleRate {};
    double activeThreshold {};
    std::optional<std::size_t> onsetFrame;
    std::optional<std::size_t> lastActiveFrame;
    std::optional<std::size_t> impulseLengthFrames;
    double peakLeft {};
    double peakRight {};
    double stereoDifferenceRms {};
    std::vector<DecayPoint> decayCurve;
    std::optional<double> rt60Seconds;
};

[[nodiscard]] ResponseMeasurements measureResponse(
    std::span<const float> left,
    std::span<const float> right,
    double sampleRate,
    double activeThreshold = 1.0e-7);

[[nodiscard]] std::string writeMeasurementsJson(const ResponseMeasurements& measurements);

} // namespace reverb::render
