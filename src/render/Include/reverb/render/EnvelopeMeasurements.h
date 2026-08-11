#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>

namespace reverb::render {

struct EnvelopeMeasurements final {
    std::string engineVersion;
    std::string patchId;
    double sampleRate {};
    double smoothingWindowMilliseconds { 10.0 };
    double cutoffDecibels { -40.0 };
    std::optional<std::size_t> onsetFrame;
    std::optional<std::size_t> peakFrame;
    std::optional<std::size_t> cutoffFrame;
    std::optional<double> timeToPeakMilliseconds;
    std::optional<double> peakToCutoffMilliseconds;
    double residualEnergyRatio {};
    double maximumDropDecibelsPerWindow {};
    bool rt60Meaningful {};
};

[[nodiscard]] EnvelopeMeasurements measureEnvelope(
    std::span<const float> left,
    std::span<const float> right,
    double sampleRate,
    double smoothingWindowMilliseconds = 10.0,
    double cutoffDecibels = -40.0);

[[nodiscard]] std::string writeEnvelopeMeasurementsJson(const EnvelopeMeasurements& measurements);

} // namespace reverb::render
