#include <reverb/render/EnvelopeMeasurements.h>

#include <reverb/render/ResponseMeasurements.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace reverb::render {

EnvelopeMeasurements measureEnvelope(
    const std::span<const float> left,
    const std::span<const float> right,
    const double sampleRate,
    const double smoothingWindowMilliseconds,
    const double cutoffDecibels)
{
    if (left.size() != right.size() || left.empty())
        throw std::invalid_argument("envelope channels must have equal nonzero frame counts");
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0
        || !std::isfinite(smoothingWindowMilliseconds) || smoothingWindowMilliseconds <= 0.0
        || !std::isfinite(cutoffDecibels) || cutoffDecibels >= 0.0)
        throw std::invalid_argument("envelope measurement settings must be finite and positive with a negative cutoff");

    EnvelopeMeasurements result;
    result.sampleRate = sampleRate;
    result.smoothingWindowMilliseconds = smoothingWindowMilliseconds;
    result.cutoffDecibels = cutoffDecibels;
    const auto windowFrames = std::max<std::size_t>(1,
        static_cast<std::size_t>(std::llround(sampleRate * smoothingWindowMilliseconds / 1'000.0)));
    const auto bucketCount = (left.size() + windowFrames - 1) / windowFrames;
    std::vector<double> energy(bucketCount, 0.0);
    double totalEnergy = 0.0;
    for (std::size_t frame = 0; frame < left.size(); ++frame) {
        const auto value = static_cast<double>(left[frame]) * left[frame]
            + static_cast<double>(right[frame]) * right[frame];
        energy[frame / windowFrames] += value;
        totalEnergy += value;
    }
    if (totalEnergy <= 0.0)
        return result;

    const auto peak = std::ranges::max_element(energy);
    const auto peakBucket = static_cast<std::size_t>(std::distance(energy.begin(), peak));
    const auto peakEnergy = *peak;
    const auto onsetThreshold = peakEnergy * 1.0e-8;
    const auto onset = std::ranges::find_if(energy, [onsetThreshold](const auto value) {
        return value > onsetThreshold;
    });
    const auto onsetBucket = static_cast<std::size_t>(std::distance(energy.begin(), onset));
    result.onsetFrame = std::min(left.size() - 1, onsetBucket * windowFrames);
    result.peakFrame = std::min(left.size() - 1, peakBucket * windowFrames + windowFrames / 2);
    result.timeToPeakMilliseconds = static_cast<double>(*result.peakFrame - *result.onsetFrame)
        / sampleRate * 1'000.0;

    const auto cutoffEnergy = peakEnergy * std::pow(10.0, cutoffDecibels / 10.0);
    std::optional<std::size_t> cutoffBucket;
    for (std::size_t bucket = peakBucket + 1; bucket < energy.size(); ++bucket) {
        if (energy[bucket] <= cutoffEnergy) {
            cutoffBucket = bucket;
            break;
        }
    }
    if (cutoffBucket) {
        result.cutoffFrame = std::min(left.size() - 1, *cutoffBucket * windowFrames);
        result.peakToCutoffMilliseconds = static_cast<double>(*result.cutoffFrame - *result.peakFrame)
            / sampleRate * 1'000.0;
        double residual = 0.0;
        for (std::size_t bucket = *cutoffBucket; bucket < energy.size(); ++bucket)
            residual += energy[bucket];
        result.residualEnergyRatio = residual / totalEnergy;
    }

    const auto floor = peakEnergy * 1.0e-12;
    const auto lastDropBucket = cutoffBucket.value_or(energy.size() - 1);
    for (std::size_t bucket = peakBucket + 1; bucket <= lastDropBucket; ++bucket) {
        const auto previous = std::max(floor, energy[bucket - 1]);
        const auto current = std::max(floor, energy[bucket]);
        result.maximumDropDecibelsPerWindow = std::max(
            result.maximumDropDecibelsPerWindow, 10.0 * std::log10(previous / current));
    }
    const auto response = measureResponse(left, right, sampleRate);
    result.rt60Meaningful = response.rt60Seconds.has_value()
        && result.maximumDropDecibelsPerWindow < 18.0;
    return result;
}

std::string writeEnvelopeMeasurementsJson(const EnvelopeMeasurements& measurements)
{
    const auto optionalFrame = [](const auto& value) {
        return value ? nlohmann::ordered_json(*value) : nlohmann::ordered_json(nullptr);
    };
    const auto optionalNumber = [](const auto& value) {
        return value ? nlohmann::ordered_json(*value) : nlohmann::ordered_json(nullptr);
    };
    const nlohmann::ordered_json json {
        { "measurementVersion", 1 },
        { "engineVersion", measurements.engineVersion },
        { "patchId", measurements.patchId },
        { "sampleRate", measurements.sampleRate },
        { "smoothingWindowMilliseconds", measurements.smoothingWindowMilliseconds },
        { "cutoffDecibels", measurements.cutoffDecibels },
        { "onsetFrame", optionalFrame(measurements.onsetFrame) },
        { "peakFrame", optionalFrame(measurements.peakFrame) },
        { "cutoffFrame", optionalFrame(measurements.cutoffFrame) },
        { "timeToPeakMilliseconds", optionalNumber(measurements.timeToPeakMilliseconds) },
        { "peakToCutoffMilliseconds", optionalNumber(measurements.peakToCutoffMilliseconds) },
        { "residualEnergyRatio", measurements.residualEnergyRatio },
        { "maximumDropDecibelsPerWindow", measurements.maximumDropDecibelsPerWindow },
        { "rt60Meaningful", measurements.rt60Meaningful },
    };
    return json.dump(2) + "\n";
}

} // namespace reverb::render
