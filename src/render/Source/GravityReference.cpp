#include <reverb/render/GravityReference.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>

namespace reverb::render {
namespace {

constexpr double epsilon = 1.0e-20;

std::vector<double> stereoEnergy(const RenderResult& result)
{
    std::vector<double> energy(result.left.size());
    for (std::size_t frame = 0; frame < energy.size(); ++frame) {
        const auto left = static_cast<double>(result.left[frame]);
        const auto right = static_cast<double>(result.right[frame]);
        energy[frame] = 0.5 * (left * left + right * right);
    }
    return energy;
}

double totalEnergy(const RenderResult& result)
{
    const auto energy = stereoEnergy(result);
    return std::accumulate(energy.begin(), energy.end(), 0.0);
}

std::uint64_t pcmHash(const std::vector<float>& samples)
{
    auto hash = 14695981039346656037ULL;
    for (const auto sample : samples) {
        const auto quantized = static_cast<std::int16_t>(
            std::lrint(std::clamp(sample, -1.0F, 1.0F) * 32'767.0F));
        for (const auto byte : { static_cast<std::uint8_t>(quantized & 0xff),
                 static_cast<std::uint8_t>((static_cast<std::uint16_t>(quantized) >> 8U) & 0xff) }) {
            hash ^= byte;
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

nlohmann::ordered_json controlsJson(const reverb::graph::GravityDiffusionControls& controls)
{
    return { { "gravity", controls.gravity }, { "size", controls.size },
        { "feedback", controls.feedback }, { "damping", controls.damping },
        { "modulation", controls.modulation } };
}

nlohmann::ordered_json metricsJson(const GravityShapeMetrics& metrics)
{
    return { { "onsetFrame", metrics.onsetFrame }, { "timeToPeakMs", metrics.timeToPeakMs },
        { "earlyLateEnergyRatioDb", metrics.earlyLateEnergyRatioDb },
        { "peakLevelDbfs", metrics.peakLevelDbfs },
        { "integratedEnergyDb", metrics.integratedEnergyDb },
        { "postPeakEnergyFraction", metrics.postPeakEnergyFraction },
        { "occupiedTenMsWindowFraction", metrics.occupiedTenMsWindowFraction },
        { "strongestThreeWindowEnergyFraction", metrics.strongestThreeWindowEnergyFraction } };
}

nlohmann::ordered_json envelopeJson(
    const RenderResult& result, const GravityShapeMetrics& metrics, const double sampleRate)
{
    constexpr std::size_t pointCount = 49;
    constexpr double horizonSeconds = 0.52;
    const auto energy = stereoEnergy(result);
    const auto halfWindow = std::max<std::size_t>(1, static_cast<std::size_t>(0.010 * sampleRate));
    std::array<double, pointCount> values {};
    double maximum {};
    for (std::size_t index = 0; index < pointCount; ++index) {
        const auto center = metrics.onsetFrame + static_cast<std::size_t>(std::llround(
            static_cast<double>(index) / static_cast<double>(pointCount - 1) * horizonSeconds * sampleRate));
        const auto first = center > halfWindow ? center - halfWindow : 0;
        const auto last = std::min(energy.size(), center + halfWindow + 1);
        if (first < last)
            values[index] = std::accumulate(energy.begin() + static_cast<std::ptrdiff_t>(first),
                energy.begin() + static_cast<std::ptrdiff_t>(last), 0.0) / static_cast<double>(last - first);
        maximum = std::max(maximum, values[index]);
    }
    nlohmann::ordered_json points = nlohmann::ordered_json::array();
    for (std::size_t index = 0; index < pointCount; ++index)
        points.push_back({ static_cast<double>(index) / static_cast<double>(pointCount - 1),
            values[index] / (maximum + epsilon) });
    return points;
}

} // namespace

GravityShapeMetrics measureGravityShape(
    const RenderResult& result, const double sampleRate, const double comparisonHorizonSeconds)
{
    if (result.left.empty() || result.left.size() != result.right.size()
        || !std::isfinite(sampleRate) || sampleRate <= 0.0)
        return {};
    const auto energy = stereoEnergy(result);
    const auto onsetIterator = std::ranges::find_if(energy, [](const double value) { return value > 1.0e-14; });
    const auto onset = static_cast<std::size_t>(std::distance(energy.begin(), onsetIterator));
    if (onset == energy.size()) return {};
    const auto smoothingFrames = std::max<std::size_t>(1, static_cast<std::size_t>(0.020 * sampleRate));
    const auto half = smoothingFrames / 2;
    double running {};
    double maximumSmoothed = -1.0;
    std::size_t peakFrame {};
    for (std::size_t frame = 0; frame < energy.size(); ++frame) {
        if (frame + half < energy.size()) running += energy[frame + half];
        if (frame > half) running -= energy[frame - half - 1];
        const auto first = frame > half ? frame - half : 0;
        const auto last = std::min(energy.size(), frame + half + 1);
        const auto smoothed = running / static_cast<double>(last - first);
        if (smoothed > maximumSmoothed) { maximumSmoothed = smoothed; peakFrame = frame; }
    }
    const auto horizon = std::min(energy.size() - onset,
        static_cast<std::size_t>(comparisonHorizonSeconds * sampleRate));
    const auto quarter = horizon / 4;
    const auto early = std::accumulate(energy.begin() + static_cast<std::ptrdiff_t>(onset),
        energy.begin() + static_cast<std::ptrdiff_t>(onset + quarter), 0.0);
    const auto late = std::accumulate(energy.begin() + static_cast<std::ptrdiff_t>(onset + 3 * quarter),
        energy.begin() + static_cast<std::ptrdiff_t>(onset + horizon), 0.0);
    const auto total = std::accumulate(energy.begin(), energy.end(), 0.0);
    const auto postPeak = peakFrame + 1 < energy.size()
        ? std::accumulate(energy.begin() + static_cast<std::ptrdiff_t>(peakFrame + 1), energy.end(), 0.0)
        : 0.0;
    const auto windowFrames = std::max<std::size_t>(1, static_cast<std::size_t>(0.010 * sampleRate));
    const auto densityEnd = std::min(energy.size(), onset + static_cast<std::size_t>(0.700 * sampleRate));
    std::vector<double> windowEnergies;
    for (auto first = onset; first < densityEnd; first += windowFrames) {
        const auto last = std::min(densityEnd, first + windowFrames);
        windowEnergies.push_back(std::accumulate(
            energy.begin() + static_cast<std::ptrdiff_t>(first),
            energy.begin() + static_cast<std::ptrdiff_t>(last), 0.0));
    }
    const auto maximumWindow = windowEnergies.empty() ? 0.0 : std::ranges::max(windowEnergies);
    const auto occupied = std::ranges::count_if(windowEnergies, [&](const double value) {
        return value > maximumWindow * 1.0e-6;
    });
    std::ranges::sort(windowEnergies, std::greater {});
    const auto windowTotal = std::accumulate(windowEnergies.begin(), windowEnergies.end(), 0.0);
    const auto strongestThree = std::accumulate(
        windowEnergies.begin(), windowEnergies.begin() + std::min<std::size_t>(3, windowEnergies.size()), 0.0);
    double peak {};
    for (std::size_t frame = 0; frame < result.left.size(); ++frame)
        peak = std::max({ peak, std::abs(static_cast<double>(result.left[frame])),
            std::abs(static_cast<double>(result.right[frame])) });
    return { onset, 1'000.0 * static_cast<double>(peakFrame) / sampleRate,
        10.0 * std::log10((early + epsilon) / (late + epsilon)),
        20.0 * std::log10(peak + epsilon), 10.0 * std::log10(total + epsilon),
        postPeak / (total + epsilon),
        windowEnergies.empty() ? 0.0 : static_cast<double>(occupied) / static_cast<double>(windowEnergies.size()),
        strongestThree / (windowTotal + epsilon) };
}

std::vector<GravityReferenceRender> renderGravityReferences(
    const double sampleRate, const double durationSeconds)
{
    const auto frameCount = static_cast<std::size_t>(std::llround(sampleRate * durationSeconds));
    const std::array definitions {
        std::pair { "inverse", gravityInverseReferenceControls },
        std::pair { "bloom", gravityBloomReferenceControls },
        std::pair { "forward", gravityForwardReferenceControls },
    };
    std::vector<GravityReferenceRender> references;
    for (const auto& [id, controls] : definitions) {
        RenderRequest request { reverb::graph::makeGravityDiffusionGraph(controls), InputKind::impulse,
            sampleRate, frameCount };
        auto raw = renderOffline(request);
        references.push_back({ id, controls, raw, raw, measureGravityShape(raw, sampleRate), {}, 1.0 });
    }
    const auto targetEnergy = totalEnergy(references[1].raw);
    for (auto& reference : references) {
        reference.loudnessMatchGain = std::sqrt(targetEnergy / std::max(totalEnergy(reference.raw), epsilon));
        for (auto& sample : reference.loudnessMatched.left) sample *= static_cast<float>(reference.loudnessMatchGain);
        for (auto& sample : reference.loudnessMatched.right) sample *= static_cast<float>(reference.loudnessMatchGain);
        reference.matchedMetrics = measureGravityShape(reference.loudnessMatched, sampleRate);
        reference.leftPcm16Fnv1a = pcmHash(reference.loudnessMatched.left);
        reference.rightPcm16Fnv1a = pcmHash(reference.loudnessMatched.right);
    }
    return references;
}

std::string writeGravityReferenceJson(
    const GravityReferenceRender& reference, const double sampleRate, const std::size_t frameCount)
{
    const nlohmann::ordered_json json {
        { "formatVersion", 1 }, { "engineVersion", "0.1" }, { "referenceId", reference.id },
        { "sampleRate", sampleRate }, { "frameCount", frameCount },
        { "comparisonHorizonMs", 520.0 }, { "smoothingWindowMs", 20.0 },
        { "controls", controlsJson(reference.controls) }, { "raw", metricsJson(reference.rawMetrics) },
        { "loudnessMatchGain", reference.loudnessMatchGain },
        { "matched", metricsJson(reference.matchedMetrics) },
        { "matchedEnvelope", envelopeJson(reference.loudnessMatched, reference.matchedMetrics, sampleRate) },
        { "leftPcm16Fnv1a", std::to_string(reference.leftPcm16Fnv1a) },
        { "rightPcm16Fnv1a", std::to_string(reference.rightPcm16Fnv1a) },
    };
    return json.dump(2) + '\n';
}

} // namespace reverb::render
