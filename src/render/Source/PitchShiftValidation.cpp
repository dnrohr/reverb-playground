#include <reverb/render/PitchShiftValidation.h>

#include <reverb/dsp/PitchShift.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

namespace reverb::render {
namespace {

using Clock = std::chrono::steady_clock;

struct ToneRender final {
    std::vector<float> samples;
    std::size_t latency {};
};

ToneRender renderTone(
    const double sampleRate,
    const double frequency,
    const reverb::dsp::pitch_shift::GrainDirection direction)
{
    reverb::dsp::PitchShift shifter;
    shifter.prepare(sampleRate, { 12.0, 60.0, 0.5, direction });
    const auto frames = shifter.latencySamples() + static_cast<std::size_t>(sampleRate);
    std::vector<float> samples(frames);
    for (std::size_t frame = 0; frame < frames; ++frame)
        samples[frame] = static_cast<float>(0.5 * std::sin(
            2.0 * std::numbers::pi * frequency * static_cast<double>(frame) / sampleRate));
    shifter.process(samples);
    return { std::move(samples), shifter.latencySamples() };
}

double toneAmplitude(
    const std::span<const float> samples,
    const double sampleRate,
    const double frequency)
{
    auto real = 0.0;
    auto imaginary = 0.0;
    auto windowSum = 0.0;
    const auto step = 2.0 * std::numbers::pi * frequency / sampleRate;
    for (std::size_t frame = 0; frame < samples.size(); ++frame) {
        const auto window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi
            * static_cast<double>(frame) / static_cast<double>(samples.size() - 1));
        const auto phase = step * static_cast<double>(frame);
        real += static_cast<double>(samples[frame]) * window * std::cos(phase);
        imaginary -= static_cast<double>(samples[frame]) * window * std::sin(phase);
        windowSum += window;
    }
    return windowSum > 0.0 ? 2.0 * std::hypot(real, imaginary) / windowSum : 0.0;
}

struct BandPeak final {
    double frequency {};
    double amplitude {};
};

BandPeak bandPeak(
    const std::span<const float> samples,
    const double sampleRate,
    const double centerFrequency,
    const double relativeHalfWidth = 0.04)
{
    BandPeak best;
    constexpr auto steps = 160;
    for (auto index = 0; index <= steps; ++index) {
        const auto position = -1.0 + 2.0 * static_cast<double>(index) / steps;
        const auto frequency = centerFrequency * (1.0 + relativeHalfWidth * position);
        const auto amplitude = toneAmplitude(samples, sampleRate, frequency);
        if (amplitude > best.amplitude) best = { frequency, amplitude };
    }
    return best;
}

double dbfs(const double amplitude) noexcept
{
    return 20.0 * std::log10(std::max(amplitude, 1.0e-12));
}

double cents(const double measured, const double expected) noexcept
{
    return 1'200.0 * std::log2(measured / expected);
}

PitchShiftDirectionMetrics measureDirection(
    const double sampleRate,
    const reverb::dsp::pitch_shift::GrainDirection direction)
{
    const auto reference = renderTone(sampleRate, 400.0, direction);
    const auto referenceSamples = std::span<const float>(reference.samples).subspan(
        reference.latency + static_cast<std::size_t>(0.2 * sampleRate),
        static_cast<std::size_t>(0.6 * sampleRate));
    const auto referencePeak = bandPeak(referenceSamples, sampleRate, 800.0);

    const auto highInputFrequency = sampleRate * 0.35;
    const auto alias = renderTone(sampleRate, highInputFrequency, direction);
    const auto aliasSamples = std::span<const float>(alias.samples).subspan(
        alias.latency + static_cast<std::size_t>(0.2 * sampleRate),
        static_cast<std::size_t>(0.6 * sampleRate));
    const auto foldedAliasFrequency = sampleRate - highInputFrequency * 2.0;
    const auto aliasPeak = bandPeak(aliasSamples, sampleRate, foldedAliasFrequency);

    constexpr std::size_t blockSize = 256;
    const auto processedFrames = static_cast<std::uint64_t>(sampleRate);
    reverb::dsp::PitchShift processor;
    processor.prepare(sampleRate, { 12.0, 60.0, 0.5, direction });
    std::array<float, blockSize> block {};
    for (std::size_t frame = 0; frame < block.size(); ++frame)
        block[frame] = static_cast<float>(0.25 * std::sin(
            2.0 * std::numbers::pi * 440.0 * static_cast<double>(frame) / sampleRate));
    for (auto warmup = 0; warmup < 32; ++warmup) processor.process(block);
    const auto started = Clock::now();
    std::uint64_t rendered = 0;
    while (rendered < processedFrames) {
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(blockSize, processedFrames - rendered));
        processor.process(std::span(block).first(count));
        rendered += count;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - started).count();
    const auto audioMicroseconds = 1'000'000.0 * static_cast<double>(processedFrames) / sampleRate;
    const auto aliasDbfs = dbfs(aliasPeak.amplitude);
    const auto referenceDbfs = dbfs(referencePeak.amplitude);
    return {
        direction == reverb::dsp::pitch_shift::GrainDirection::forward ? "forward" : "reverse",
        cents(referencePeak.frequency, 800.0), referenceDbfs, aliasDbfs,
        aliasDbfs - referenceDbfs,
        100.0 * static_cast<double>(elapsed) / audioMicroseconds,
        processedFrames, static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsed)),
    };
}

nlohmann::ordered_json directionJson(const PitchShiftDirectionMetrics& metrics)
{
    return {
        { "direction", metrics.direction },
        { "measuredOctaveCents", metrics.measuredOctaveCents },
        { "referenceTargetDbfs", metrics.referenceTargetDbfs },
        { "foldedAliasDbfs", metrics.foldedAliasDbfs },
        { "aliasRelativeToReferenceDb", metrics.aliasRelativeToReferenceDb },
        { "measuredCpuRealtimeLoadPercent", metrics.measuredCpuRealtimeLoadPercent },
        { "processedFrames", metrics.processedFrames },
        { "elapsedMicroseconds", metrics.elapsedMicroseconds },
    };
}

} // namespace

PitchShiftValidationReport measurePitchShiftValidation()
{
    PitchShiftValidationReport report {
        "dual-grain-linear-v1", "linear", 60.0, 0.5, {},
    };
    for (const auto sampleRate : reverb::dsp::pitch_shift::qualificationSampleRates) {
        const auto latency = reverb::dsp::pitch_shift::reportedLatencySamples(sampleRate);
        report.rates.push_back({
            sampleRate, latency, 1'000.0 * static_cast<double>(latency) / sampleRate,
            reverb::dsp::pitch_shift::preparedStorageSamples(sampleRate),
            reverb::dsp::pitch_shift::preparedStorageBytes(sampleRate),
            measureDirection(sampleRate, reverb::dsp::pitch_shift::GrainDirection::forward),
            measureDirection(sampleRate, reverb::dsp::pitch_shift::GrainDirection::reverse),
        });
    }
    return report;
}

std::string writePitchShiftValidationJson(const PitchShiftValidationReport& report)
{
    auto rates = nlohmann::ordered_json::array();
    for (const auto& rate : report.rates) {
        rates.push_back({
            { "sampleRate", rate.sampleRate },
            { "latencySamples", rate.latencySamples },
            { "latencyMilliseconds", rate.latencyMilliseconds },
            { "storageSamples", rate.storageSamples },
            { "storageBytes", rate.storageBytes },
            { "directions", nlohmann::ordered_json::array({
                directionJson(rate.forward), directionJson(rate.reverse),
            }) },
        });
    }
    return nlohmann::ordered_json {
        { "formatVersion", 1 },
        { "measurement", "pitch-shift-validation" },
        { "quality", {
            { "id", report.qualityId }, { "interpolation", report.interpolation },
            { "grainMilliseconds", report.grainMilliseconds }, { "overlap", report.overlap },
        } },
        { "spectralMethod", {
            { "referenceInputHz", 400.0 }, { "expectedOctaveHz", 800.0 },
            { "analysisBandRelativeHalfWidth", 0.04 },
            { "aliasInputFractionOfSampleRate", 0.35 },
            { "expectedFoldedAliasFractionOfSampleRate", 0.30 },
            { "window", "Hann" }, { "analysisSeconds", 0.6 },
        } },
        { "cpuMethod", {
            { "basis", "measured-steady-clock" }, { "audioSecondsPerDirection", 1.0 },
            { "blockSize", 256 },
            { "warning", "Workstation measurement; compare trends, not an absolute cross-machine guarantee." },
        } },
        { "rates", std::move(rates) },
    }.dump(2);
}

} // namespace reverb::render
