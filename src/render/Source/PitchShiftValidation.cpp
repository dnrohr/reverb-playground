#include <reverb/render/PitchShiftValidation.h>

#include <reverb/dsp/PitchShift.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numbers>
#include <span>
#include <tuple>
#include <vector>

namespace reverb::render {
namespace {

using Clock = std::chrono::steady_clock;

struct ToneRender final {
    std::vector<float> samples;
    std::size_t latency {};
};

struct ReverseGrainMetrics final {
    double correlation {};
    bool deterministic {};
    bool causal {};
    double forwardPeakEnvelopeStep {};
    double reversePeakEnvelopeStep {};
    double envelopeDifferenceRms {};
};

ToneRender renderTone(
    const double sampleRate,
    const double frequency,
    const reverb::dsp::pitch_shift::GrainDirection direction,
    const reverb::dsp::PitchShiftQuality quality = reverb::dsp::PitchShiftQuality::normal)
{
    reverb::dsp::PitchShift shifter;
    shifter.prepare(sampleRate, { 12.0, 60.0, 0.5, direction, 0.0, quality });
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
    const reverb::dsp::pitch_shift::GrainDirection direction,
    const reverb::dsp::PitchShiftQuality quality = reverb::dsp::PitchShiftQuality::normal)
{
    const auto reference = renderTone(sampleRate, 400.0, direction, quality);
    const auto referenceSamples = std::span<const float>(reference.samples).subspan(
        reference.latency + static_cast<std::size_t>(0.2 * sampleRate),
        static_cast<std::size_t>(0.6 * sampleRate));
    const auto referencePeak = bandPeak(referenceSamples, sampleRate, 800.0);

    const auto highInputFrequency = sampleRate * 0.35;
    const auto alias = renderTone(sampleRate, highInputFrequency, direction, quality);
    const auto aliasSamples = std::span<const float>(alias.samples).subspan(
        alias.latency + static_cast<std::size_t>(0.2 * sampleRate),
        static_cast<std::size_t>(0.6 * sampleRate));
    const auto foldedAliasFrequency = sampleRate - highInputFrequency * 2.0;
    const auto aliasPeak = bandPeak(aliasSamples, sampleRate, foldedAliasFrequency);

    constexpr std::size_t blockSize = 256;
    const auto processedFrames = static_cast<std::uint64_t>(sampleRate);
    reverb::dsp::PitchShift processor;
    processor.prepare(sampleRate, { 12.0, 60.0, 0.5, direction, 0.0, quality });
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

std::vector<float> deterministicNoise(const std::size_t frames)
{
    std::vector<float> samples(frames);
    std::uint32_t state = 0x4d13'7a2bu;
    for (auto& sample : samples) {
        state = state * 1'664'525u + 1'013'904'223u;
        sample = static_cast<float>((static_cast<double>(state) / 4'294'967'295.0 - 0.5) * 0.4);
    }
    return samples;
}

std::vector<float> renderFixture(
    const std::span<const float> input,
    const double sampleRate,
    const reverb::dsp::pitch_shift::GrainDirection direction,
    const double phase)
{
    reverb::dsp::PitchShift processor;
    processor.prepare(sampleRate, { 12.0, 60.0, 0.5, direction, phase });
    auto output = std::vector<float>(input.begin(), input.end());
    processor.process(output);
    return output;
}

std::vector<double> envelope(const std::span<const float> samples, const double sampleRate)
{
    std::vector<double> values(samples.size());
    const auto coefficient = std::exp(-1.0 / (0.002 * sampleRate));
    auto value = 0.0;
    for (std::size_t frame = 0; frame < samples.size(); ++frame) {
        value = coefficient * value + (1.0 - coefficient) * std::abs(samples[frame]);
        values[frame] = value;
    }
    return values;
}

double peakStep(const std::span<const double> values)
{
    auto peak = 0.0;
    for (std::size_t frame = 1; frame < values.size(); ++frame)
        peak = std::max(peak, std::abs(values[frame] - values[frame - 1]));
    return peak;
}

ReverseGrainMetrics measureReverseGrain(const double sampleRate)
{
    using reverb::dsp::pitch_shift::GrainDirection;
    const auto latency = reverb::dsp::pitch_shift::reportedLatencySamples(sampleRate);
    const auto frames = latency + static_cast<std::size_t>(sampleRate);
    const auto noise = deterministicNoise(frames);
    const auto left = renderFixture(noise, sampleRate, GrainDirection::reverse, 0.0);
    const auto right = renderFixture(noise, sampleRate, GrainDirection::reverse, 0.373);
    const auto repeated = renderFixture(noise, sampleRate, GrainDirection::reverse, 0.373);
    auto cross = 0.0;
    auto leftEnergy = 0.0;
    auto rightEnergy = 0.0;
    for (std::size_t frame = latency; frame < frames; ++frame) {
        cross += static_cast<double>(left[frame]) * right[frame];
        leftEnergy += static_cast<double>(left[frame]) * left[frame];
        rightEnergy += static_cast<double>(right[frame]) * right[frame];
    }
    const auto correlation = cross / std::sqrt(leftEnergy * rightEnergy);
    const auto causal = std::ranges::all_of(std::span(left).first(latency),
        [](const float sample) { return sample == 0.0F; })
        && std::ranges::all_of(std::span(right).first(latency),
            [](const float sample) { return sample == 0.0F; });

    std::vector<float> transients(frames);
    const auto spacing = static_cast<std::size_t>(std::llround(0.137 * sampleRate));
    for (std::size_t frame = 0; frame < frames; frame += spacing) {
        transients[frame] = 0.8F;
        if (frame + 1 < frames) transients[frame + 1] = -0.35F;
    }
    const auto forward = renderFixture(transients, sampleRate, GrainDirection::forward, 0.0);
    const auto reverse = renderFixture(transients, sampleRate, GrainDirection::reverse, 0.0);
    const auto forwardEnvelope = envelope(std::span(forward).subspan(latency), sampleRate);
    const auto reverseEnvelope = envelope(std::span(reverse).subspan(latency), sampleRate);
    auto differenceEnergy = 0.0;
    auto referenceEnergy = 0.0;
    for (std::size_t frame = 0; frame < forwardEnvelope.size(); ++frame) {
        const auto difference = forwardEnvelope[frame] - reverseEnvelope[frame];
        differenceEnergy += difference * difference;
        referenceEnergy += forwardEnvelope[frame] * forwardEnvelope[frame];
    }
    return { correlation, right == repeated, causal,
        peakStep(forwardEnvelope), peakStep(reverseEnvelope),
        std::sqrt(differenceEnergy / std::max(referenceEnergy, 1.0e-24)) };
}

PitchShiftBenchmarkMetrics measureBenchmark(const double sampleRate, const std::string& scenario)
{
    constexpr std::size_t blockSize = 256;
    const auto processedFrames = static_cast<std::uint64_t>(sampleRate);
    reverb::dsp::PitchShift first;
    reverb::dsp::PitchShift second;
    first.prepare(sampleRate, { 12.0, 60.0, 0.5,
        reverb::dsp::pitch_shift::GrainDirection::forward });
    second.prepare(sampleRate, { 7.0, 80.0, 0.65,
        reverb::dsp::pitch_shift::GrainDirection::reverse, 0.373 });
    std::array<float, blockSize> firstBlock {};
    std::array<float, blockSize> secondBlock {};
    for (std::size_t frame = 0; frame < blockSize; ++frame)
        firstBlock[frame] = static_cast<float>(0.25 * std::sin(
            2.0 * std::numbers::pi * 440.0 * static_cast<double>(frame) / sampleRate));
    for (auto warmup = 0; warmup < 32; ++warmup) first.process(firstBlock);

    const auto started = Clock::now();
    std::uint64_t rendered = 0;
    std::size_t blockIndex = 0;
    while (rendered < processedFrames) {
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(blockSize, processedFrames - rendered));
        const auto firstSpan = std::span(firstBlock).first(count);
        if (scenario == "parameter-transition") {
            first.setParameters({ blockIndex % 2 == 0 ? -12.0 : 12.0,
                blockIndex % 2 == 0 ? 40.0 : 120.0,
                blockIndex % 2 == 0 ? 0.25 : 1.0,
                blockIndex % 2 == 0 ? reverb::dsp::pitch_shift::GrainDirection::forward
                                    : reverb::dsp::pitch_shift::GrainDirection::reverse });
        }
        first.process(firstSpan);
        if (scenario == "two-voices" || scenario == "topology-crossfade") {
            std::copy(firstSpan.begin(), firstSpan.end(), secondBlock.begin());
            second.process(std::span(secondBlock).first(count));
            if (scenario == "topology-crossfade") {
                const auto mix = static_cast<float>(rendered) / static_cast<float>(processedFrames);
                for (std::size_t frame = 0; frame < count; ++frame)
                    firstBlock[frame] = std::lerp(firstBlock[frame], secondBlock[frame], mix);
            }
        }
        rendered += count;
        ++blockIndex;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - started).count();
    const auto audioMicroseconds = 1'000'000.0 * static_cast<double>(processedFrames) / sampleRate;
    return { scenario, processedFrames,
        static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsed)),
        100.0 * static_cast<double>(elapsed) / audioMicroseconds };
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
        "dual-grain-linear-v1", "linear", 60.0, 0.5, {}, {},
    };
    for (const auto sampleRate : reverb::dsp::pitch_shift::qualificationSampleRates) {
        const auto latency = reverb::dsp::pitch_shift::reportedLatencySamples(sampleRate);
        const auto reverseGrain = measureReverseGrain(sampleRate);
        report.rates.push_back({
            sampleRate, latency, 1'000.0 * static_cast<double>(latency) / sampleRate,
            reverb::dsp::pitch_shift::preparedStorageSamples(sampleRate),
            reverb::dsp::pitch_shift::preparedStorageBytes(sampleRate),
            measureDirection(sampleRate, reverb::dsp::pitch_shift::GrainDirection::forward),
            measureDirection(sampleRate, reverb::dsp::pitch_shift::GrainDirection::reverse),
            0.0, 0.373, reverseGrain.correlation, reverseGrain.deterministic,
            reverseGrain.causal, reverseGrain.forwardPeakEnvelopeStep,
            reverseGrain.reversePeakEnvelopeStep, reverseGrain.envelopeDifferenceRms,
            { measureBenchmark(sampleRate, "steady-state"),
                measureBenchmark(sampleRate, "parameter-transition"),
                measureBenchmark(sampleRate, "two-voices"),
                measureBenchmark(sampleRate, "topology-crossfade") },
        });
    }
    constexpr auto comparisonRate = 48'000.0;
    for (const auto [id, interpolation, quality] : {
             std::tuple { "draft", "nearest", reverb::dsp::PitchShiftQuality::draft },
             std::tuple { "normal", "linear", reverb::dsp::PitchShiftQuality::normal },
             std::tuple { "high", "cubic", reverb::dsp::PitchShiftQuality::high },
         }) {
        report.qualityModes.push_back({ id, interpolation,
            measureDirection(comparisonRate, reverb::dsp::pitch_shift::GrainDirection::forward, quality),
            measureDirection(comparisonRate, reverb::dsp::pitch_shift::GrainDirection::reverse, quality) });
    }
    return report;
}

std::string writePitchShiftValidationJson(const PitchShiftValidationReport& report)
{
    auto rates = nlohmann::ordered_json::array();
    for (const auto& rate : report.rates) {
        auto benchmarks = nlohmann::ordered_json::array();
        for (const auto& benchmark : rate.benchmarks) {
            benchmarks.push_back({
                { "scenario", benchmark.scenario },
                { "processedFrames", benchmark.processedFrames },
                { "elapsedMicroseconds", benchmark.elapsedMicroseconds },
                { "realtimeLoadPercent", benchmark.realtimeLoadPercent },
            });
        }
        rates.push_back({
            { "sampleRate", rate.sampleRate },
            { "latencySamples", rate.latencySamples },
            { "latencyMilliseconds", rate.latencyMilliseconds },
            { "storageSamples", rate.storageSamples },
            { "storageBytes", rate.storageBytes },
            { "directions", nlohmann::ordered_json::array({
                directionJson(rate.forward), directionJson(rate.reverse),
            }) },
            { "reverseGrain", {
                { "pairedPhaseCycles", { rate.pairedPhaseA, rate.pairedPhaseB } },
                { "pairedOutputCorrelation", rate.pairedOutputCorrelation },
                { "resetDeterministic", rate.pairedResetDeterministic },
                { "causalBeforeDeclaredLatency", rate.causalBeforeDeclaredLatency },
                { "transientEnvelope", {
                    { "forwardPeakStep", rate.forwardTransientPeakEnvelopeStep },
                    { "reversePeakStep", rate.reverseTransientPeakEnvelopeStep },
                    { "normalizedDifferenceRms", rate.transientEnvelopeDifferenceRms },
                    { "smoothingMilliseconds", 2.0 },
                    { "fixtureSpacingMilliseconds", 137.0 },
                } },
            } },
            { "benchmarks", std::move(benchmarks) },
        });
    }
    auto qualityModes = nlohmann::ordered_json::array();
    for (const auto& mode : report.qualityModes) {
        qualityModes.push_back({
            { "id", mode.id }, { "interpolation", mode.interpolation },
            { "sampleRate", 48'000.0 },
            { "directions", nlohmann::ordered_json::array({
                directionJson(mode.forward), directionJson(mode.reverse),
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
        { "qualityModes", std::move(qualityModes) },
        { "rates", std::move(rates) },
    }.dump(2);
}

} // namespace reverb::render
