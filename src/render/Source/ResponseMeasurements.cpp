#include <reverb/render/ResponseMeasurements.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace reverb::render {
namespace {

std::optional<double> estimateRt60(
    const std::vector<double>& decayDb,
    const std::span<const float> left,
    const std::span<const float> right,
    const double sampleRate,
    const double peak)
{
    if (decayDb.empty() || peak <= 0.0)
        return std::nullopt;

    const auto tailStart = decayDb.size() * 9 / 10;
    double tailEnergy = 0.0;
    for (std::size_t frame = tailStart; frame < decayDb.size(); ++frame) {
        tailEnergy += static_cast<double>(left[frame]) * left[frame];
        tailEnergy += static_cast<double>(right[frame]) * right[frame];
    }
    const auto tailSamples = std::max<std::size_t>(1, (decayDb.size() - tailStart) * 2);
    const auto tailRms = std::sqrt(tailEnergy / static_cast<double>(tailSamples));
    if (tailRms > peak * 1.0e-4)
        return std::nullopt;

    double sumTime = 0.0;
    double sumDb = 0.0;
    double sumTimeSquared = 0.0;
    double sumTimeDb = 0.0;
    std::size_t count = 0;
    for (std::size_t frame = 0; frame < decayDb.size(); ++frame) {
        if (decayDb[frame] <= -5.0 && decayDb[frame] >= -35.0) {
            const auto time = static_cast<double>(frame) / sampleRate;
            sumTime += time;
            sumDb += decayDb[frame];
            sumTimeSquared += time * time;
            sumTimeDb += time * decayDb[frame];
            ++count;
        }
    }
    if (count < 20)
        return std::nullopt;

    const auto countValue = static_cast<double>(count);
    const auto denominator = countValue * sumTimeSquared - sumTime * sumTime;
    if (denominator <= std::numeric_limits<double>::epsilon())
        return std::nullopt;
    const auto slope = (countValue * sumTimeDb - sumTime * sumDb) / denominator;
    if (slope >= -1.0)
        return std::nullopt;
    return -60.0 / slope;
}

} // namespace

ResponseMeasurements measureResponse(
    const std::span<const float> left,
    const std::span<const float> right,
    const double sampleRate,
    const double activeThreshold)
{
    if (left.size() != right.size() || left.empty())
        throw std::invalid_argument("measurement channels must have equal nonzero frame counts");
    if (sampleRate <= 0.0 || activeThreshold < 0.0)
        throw std::invalid_argument("measurement sample rate must be positive and threshold nonnegative");

    ResponseMeasurements result;
    result.frameCount = left.size();
    result.sampleRate = sampleRate;
    result.activeThreshold = activeThreshold;
    double differenceEnergy = 0.0;
    std::vector<double> reverseEnergy(left.size(), 0.0);

    double accumulatedEnergy = 0.0;
    for (std::size_t reverse = left.size(); reverse > 0; --reverse) {
        const auto frame = reverse - 1;
        const auto leftValue = static_cast<double>(left[frame]);
        const auto rightValue = static_cast<double>(right[frame]);
        accumulatedEnergy += leftValue * leftValue + rightValue * rightValue;
        reverseEnergy[frame] = accumulatedEnergy;
    }

    for (std::size_t frame = 0; frame < left.size(); ++frame) {
        const auto leftMagnitude = std::abs(static_cast<double>(left[frame]));
        const auto rightMagnitude = std::abs(static_cast<double>(right[frame]));
        result.peakLeft = std::max(result.peakLeft, leftMagnitude);
        result.peakRight = std::max(result.peakRight, rightMagnitude);
        const auto difference = static_cast<double>(left[frame]) - right[frame];
        differenceEnergy += difference * difference;
        if (std::max(leftMagnitude, rightMagnitude) > activeThreshold) {
            if (!result.onsetFrame)
                result.onsetFrame = frame;
            result.lastActiveFrame = frame;
        }
    }
    result.stereoDifferenceRms = std::sqrt(differenceEnergy / static_cast<double>(left.size()));
    if (result.onsetFrame && result.lastActiveFrame)
        result.impulseLengthFrames = *result.lastActiveFrame - *result.onsetFrame + 1;

    std::vector<double> decayDb(left.size(), -std::numeric_limits<double>::infinity());
    if (reverseEnergy.front() > 0.0) {
        for (std::size_t frame = 0; frame < left.size(); ++frame) {
            if (reverseEnergy[frame] > 0.0)
                decayDb[frame] = 10.0 * std::log10(reverseEnergy[frame] / reverseEnergy.front());
        }
    }

    const auto stride = std::max<std::size_t>(1, left.size() / 256);
    for (std::size_t frame = 0; frame < left.size(); frame += stride)
        result.decayCurve.push_back({ frame, static_cast<double>(frame) / sampleRate, decayDb[frame] });
    if (result.decayCurve.back().frame != left.size() - 1) {
        const auto frame = left.size() - 1;
        result.decayCurve.push_back({ frame, static_cast<double>(frame) / sampleRate, decayDb[frame] });
    }

    result.rt60Seconds = estimateRt60(
        decayDb, left, right, sampleRate, std::max(result.peakLeft, result.peakRight));
    return result;
}

std::string writeMeasurementsJson(const ResponseMeasurements& measurements)
{
    nlohmann::ordered_json curve = nlohmann::ordered_json::array();
    for (const auto& point : measurements.decayCurve) {
        nlohmann::ordered_json item {
            { "frame", point.frame },
            { "seconds", point.seconds },
        };
        item["decibels"] = std::isfinite(point.decibels)
            ? nlohmann::ordered_json(point.decibels)
            : nlohmann::ordered_json(nullptr);
        curve.push_back(std::move(item));
    }

    nlohmann::ordered_json json {
        { "measurementVersion", 1 },
        { "engineVersion", measurements.engineVersion },
        { "patchId", measurements.patchId },
        { "frameCount", measurements.frameCount },
        { "sampleRate", measurements.sampleRate },
        { "activeThreshold", measurements.activeThreshold },
        { "onsetFrame", measurements.onsetFrame ? nlohmann::ordered_json(*measurements.onsetFrame) : nlohmann::ordered_json(nullptr) },
        { "lastActiveFrame", measurements.lastActiveFrame ? nlohmann::ordered_json(*measurements.lastActiveFrame) : nlohmann::ordered_json(nullptr) },
        { "impulseLengthFrames", measurements.impulseLengthFrames ? nlohmann::ordered_json(*measurements.impulseLengthFrames) : nlohmann::ordered_json(nullptr) },
        { "peakLeft", measurements.peakLeft },
        { "peakRight", measurements.peakRight },
        { "stereoDifferenceRms", measurements.stereoDifferenceRms },
        { "rt60Seconds", measurements.rt60Seconds ? nlohmann::ordered_json(*measurements.rt60Seconds) : nlohmann::ordered_json(nullptr) },
        { "decayCurve", std::move(curve) },
    };
    return json.dump(2) + "\n";
}

} // namespace reverb::render
