#include <reverb/render/SplitFeedbackShimmerValidation.h>

#include <reverb/dsp/PitchShiftContract.h>
#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/SafeParallelShimmerGraph.h>
#include <reverb/graph/SplitFeedbackShimmerGraph.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

namespace reverb::render {
namespace {

constexpr std::size_t blockSize = 256;

struct StereoRender final {
    std::vector<float> left;
    std::vector<float> right;
};

StereoRender renderGraphTone(
    const reverb::graph::GraphDocument& graph,
    const double sampleRate,
    const double frequency,
    const double seconds)
{
    auto compiled = reverb::graph::compileFeedbackGraph(
        graph, sampleRate, blockSize);
    if (!compiled.valid()) return {};
    const auto frames = static_cast<std::size_t>(std::ceil(sampleRate * seconds));
    StereoRender render { std::vector<float>(frames), std::vector<float>(frames) };
    std::array<float, blockSize> input {}, silence {}, outputLeft {}, outputRight {};
    for (std::size_t start = 0; start < frames; start += blockSize) {
        const auto count = std::min(blockSize, frames - start);
        for (std::size_t frame = 0; frame < count; ++frame) {
            const auto absoluteFrame = start + frame;
            input[frame] = static_cast<float>(0.05 * std::sin(
                2.0 * std::numbers::pi * frequency * static_cast<double>(absoluteFrame) / sampleRate));
        }
        compiled.runtime->process(
            std::span<const float>(input).first(count), std::span<const float>(silence).first(count),
            std::span<float>(outputLeft).first(count), std::span<float>(outputRight).first(count));
        std::ranges::copy(std::span(outputLeft).first(count), render.left.begin() + static_cast<std::ptrdiff_t>(start));
        std::ranges::copy(std::span(outputRight).first(count), render.right.begin() + static_cast<std::ptrdiff_t>(start));
    }
    return render;
}

StereoRender renderTone(
    const reverb::graph::SplitFeedbackShimmerControls& controls,
    const double sampleRate,
    const double frequency,
    const double seconds)
{
    return renderGraphTone(reverb::graph::makeSplitFeedbackShimmerGraph(controls),
        sampleRate, frequency, seconds);
}

double toneAmplitude(const std::span<const float> samples, const double sampleRate, const double frequency)
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

struct BandPeak final { double frequency {}; double amplitude {}; };

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

double db(const double numerator, const double denominator = 1.0) noexcept
{
    return 20.0 * std::log10(std::max(numerator, 1.0e-12) / std::max(denominator, 1.0e-12));
}

double cents(const double measured, const double expected) noexcept
{
    return 1'200.0 * std::log2(measured / expected);
}

SplitShimmerSpectralWindow spectralWindow(
    const std::span<const float> samples,
    const double sampleRate,
    const double sourceFrequency,
    const double startSeconds,
    const double windowSeconds)
{
    const auto window = samples.subspan(
        static_cast<std::size_t>(startSeconds * sampleRate),
        static_cast<std::size_t>(windowSeconds * sampleRate));
    const auto source = bandPeak(window, sampleRate, sourceFrequency, 0.02);
    const auto octave12 = bandPeak(window, sampleRate, sourceFrequency * 2.0, 0.008);
    const auto octave24 = bandPeak(window, sampleRate, sourceFrequency * 4.0, 0.008);
    return {
        startSeconds, db(source.amplitude), octave12.frequency,
        cents(octave12.frequency, sourceFrequency * 2.0), db(octave12.amplitude),
        octave24.frequency, cents(octave24.frequency, sourceFrequency * 4.0), db(octave24.amplitude),
    };
}

double impulseTailEnergy(const double normalFeedback)
{
    constexpr auto sampleRate = 48'000.0;
    auto controls = reverb::graph::SplitFeedbackShimmerControls {};
    controls.normalFeedback = normalFeedback;
    controls.shiftedFeedback = 0.0;
    auto compiled = reverb::graph::compileFeedbackGraph(
        reverb::graph::makeSplitFeedbackShimmerGraph(controls), sampleRate, blockSize);
    if (!compiled.valid()) return 0.0;
    std::array<float, blockSize> input {}, silence {}, left {}, right {};
    auto energy = 0.0;
    const auto blocks = static_cast<std::size_t>(std::ceil(sampleRate * 3.0 / blockSize));
    for (std::size_t block = 0; block < blocks; ++block) {
        input.fill(0.0F);
        if (block == 0) input[0] = 0.1F;
        compiled.runtime->process(input, silence, left, right);
        if (block * blockSize >= static_cast<std::size_t>(1.5 * sampleRate)) {
            for (std::size_t frame = 0; frame < blockSize; ++frame)
                energy += static_cast<double>(left[frame]) * left[frame]
                    + static_cast<double>(right[frame]) * right[frame];
        }
    }
    return energy;
}

double correlation(
    const std::span<const float> left,
    const std::span<const float> right) noexcept
{
    auto leftEnergy = 0.0;
    auto rightEnergy = 0.0;
    auto cross = 0.0;
    for (std::size_t frame = 0; frame < left.size(); ++frame) {
        leftEnergy += static_cast<double>(left[frame]) * left[frame];
        rightEnergy += static_cast<double>(right[frame]) * right[frame];
        cross += static_cast<double>(left[frame]) * right[frame];
    }
    const auto denominator = std::sqrt(leftEnergy * rightEnergy);
    return denominator > 0.0 ? cross / denominator : 0.0;
}

SplitShimmerAutomationMetrics measureAutomation(const double sampleRate)
{
    constexpr std::size_t automationBlockSize = 128;
    reverb::graph::AcyclicRuntimeHost host;
    auto controls = reverb::graph::SplitFeedbackShimmerControls {};
    const auto initial = host.compileFeedbackAndPublish(
        reverb::graph::makeSplitFeedbackShimmerGraph(controls), sampleRate, automationBlockSize);
    if (!initial.valid()) return { sampleRate, false };

    std::array<float, automationBlockSize> input {}, silence {}, left {}, right {};
    auto phase = 0.0;
    const auto phaseStep = 2.0 * std::numbers::pi * 220.0 / sampleRate;
    auto previous = 0.0F;
    auto hasPrevious = false;
    auto finite = true;
    auto peak = 0.0;
    auto maximumStep = 0.0;
    std::uint64_t processed = 0;
    std::uint64_t edits = 0;
    const auto processBlock = [&] {
        for (auto& sample : input) {
            sample = static_cast<float>(0.03 * std::sin(phase));
            phase += phaseStep;
            if (phase >= 2.0 * std::numbers::pi) phase -= 2.0 * std::numbers::pi;
        }
        host.process(input, silence, left, right);
        for (std::size_t frame = 0; frame < automationBlockSize; ++frame) {
            finite = finite && std::isfinite(left[frame]) && std::isfinite(right[frame]);
            peak = std::max({ peak, std::abs(static_cast<double>(left[frame])),
                std::abs(static_cast<double>(right[frame])) });
            if (hasPrevious) maximumStep = std::max(maximumStep,
                std::abs(static_cast<double>(left[frame] - previous)));
            previous = left[frame];
            hasPrevious = true;
        }
        processed += automationBlockSize;
    };
    const auto blocksPerState = static_cast<std::size_t>(
        std::ceil(0.9 * sampleRate / automationBlockSize));
    for (std::size_t block = 0; block < blocksPerState; ++block) processBlock();

    for (auto edit = 0; edit < 18; ++edit) {
        const auto high = edit % 2 != 0;
        controls.normalFeedback = high ? 0.56 : 0.18;
        controls.shiftedFeedback = high ? 0.13 : 0.02;
        controls.pitchSemitones = high ? 12.0 : 7.0;
        controls.postShiftLowpassHertz = high ? 8'000.0 : 1'600.0;
        controls.sizeMilliseconds = high ? 230.0 : 95.0;
        const auto result = host.compileFeedbackAndPublish(
            reverb::graph::makeSplitFeedbackShimmerGraph(controls), sampleRate, automationBlockSize);
        if (!result.valid()) {
            finite = false;
            break;
        }
        ++edits;
        for (std::size_t block = 0; block < blocksPerState; ++block) processBlock();
    }
    return { sampleRate, finite, peak, maximumStep, processed, edits,
        host.publicationSnapshot().completedCrossfades };
}

nlohmann::ordered_json windowJson(const SplitShimmerSpectralWindow& window)
{
    return {
        { "startSeconds", window.startSeconds }, { "sourceDbfs", window.sourceDbfs },
        { "octave12FrequencyHertz", window.octave12FrequencyHertz },
        { "octave12CentsError", window.octave12CentsError },
        { "octave12Dbfs", window.octave12Dbfs },
        { "octave24FrequencyHertz", window.octave24FrequencyHertz },
        { "octave24CentsError", window.octave24CentsError },
        { "octave24Dbfs", window.octave24Dbfs },
    };
}

} // namespace

SplitFeedbackShimmerValidationReport measureSplitFeedbackShimmerValidation()
{
    constexpr auto sampleRate = 48'000.0;
    constexpr auto sourceFrequency = 400.0;
    constexpr auto seconds = 4.0;
    constexpr auto windowSeconds = 0.5;
    auto referenceControls = reverb::graph::SplitFeedbackShimmerControls {};
    referenceControls.shiftedFeedback = 0.13;
    const auto reference = renderTone(referenceControls, sampleRate, sourceFrequency, seconds);
    const auto early = spectralWindow(reference.left, sampleRate, sourceFrequency, 1.0, windowSeconds);
    const auto late = spectralWindow(reference.left, sampleRate, sourceFrequency, 2.5, windowSeconds);
    const auto parallel = renderGraphTone(reverb::graph::makeSafeParallelShimmerGraph(),
        sampleRate, sourceFrequency, seconds);
    const auto parallelLate = spectralWindow(
        parallel.left, sampleRate, sourceFrequency, 2.5, windowSeconds);
    const auto splitRelative24 = late.octave24Dbfs - late.octave12Dbfs;
    const auto parallelRelative24 = parallelLate.octave24Dbfs - parallelLate.octave12Dbfs;

    auto lowShifted = referenceControls;
    lowShifted.shiftedFeedback = 0.04;
    const auto lowShiftedRender = renderTone(lowShifted, sampleRate, sourceFrequency, seconds);
    const auto lowShiftedLate = spectralWindow(
        lowShiftedRender.left, sampleRate, sourceFrequency, 2.5, windowSeconds);

    auto highDamping = referenceControls;
    highDamping.postShiftLowpassHertz = 9'000.0;
    const auto highDampingRender = renderTone(highDamping, sampleRate, sourceFrequency, seconds);
    const auto highDampingLate = spectralWindow(
        highDampingRender.left, sampleRate, sourceFrequency, 2.5, windowSeconds);
    auto lowDamping = referenceControls;
    lowDamping.postShiftLowpassHertz = 2'000.0;
    const auto lowDampingRender = renderTone(lowDamping, sampleRate, sourceFrequency, seconds);
    const auto lowDampingLate = spectralWindow(
        lowDampingRender.left, sampleRate, sourceFrequency, 2.5, windowSeconds);

    const auto lateWindow = std::span<const float>(reference.left).subspan(
        static_cast<std::size_t>(2.5 * sampleRate), static_cast<std::size_t>(windowSeconds * sampleRate));
    const auto target12 = toneAmplitude(lateWindow, sampleRate, sourceFrequency * 2.0);
    const auto grainRate = 1'000.0 / 60.0;
    const auto sideband = std::max(
        toneAmplitude(lateWindow, sampleRate, sourceFrequency * 2.0 - grainRate),
        toneAmplitude(lateWindow, sampleRate, sourceFrequency * 2.0 + grainRate));

    auto aliasControls = referenceControls;
    aliasControls.postShiftLowpassHertz = 9'000.0;
    const auto aliasRender = renderTone(aliasControls, sampleRate, 7'000.0, seconds);
    const auto aliasWindow = std::span<const float>(aliasRender.left).subspan(
        static_cast<std::size_t>(2.5 * sampleRate), static_cast<std::size_t>(windowSeconds * sampleRate));
    const auto firstAliasOctave = bandPeak(aliasWindow, sampleRate, 14'000.0).amplitude;
    const auto foldedSecondOctave = bandPeak(aliasWindow, sampleRate, 20'000.0).amplitude;

    const auto correlationStart = static_cast<std::size_t>(2.5 * sampleRate);
    const auto correlationFrames = static_cast<std::size_t>(windowSeconds * sampleRate);
    SplitFeedbackShimmerValidationReport report {
        sampleRate, sourceFrequency, windowSeconds, early, late,
        late.octave24Dbfs - early.octave24Dbfs,
        splitRelative24, parallelRelative24, splitRelative24 - parallelRelative24,
        lowShifted.shiftedFeedback, referenceControls.shiftedFeedback,
        lowShiftedLate.octave24Dbfs, late.octave24Dbfs,
        late.octave24Dbfs - lowShiftedLate.octave24Dbfs,
        0.18, 0.56, db(impulseTailEnergy(0.56), impulseTailEnergy(0.18)),
        db(sideband, target12), db(foldedSecondOctave, firstAliasOctave),
        lowDamping.postShiftLowpassHertz, highDamping.postShiftLowpassHertz,
        lowDampingLate.octave24Dbfs - highDampingLate.octave24Dbfs,
        correlation(std::span<const float>(reference.left).subspan(correlationStart, correlationFrames),
            std::span<const float>(reference.right).subspan(correlationStart, correlationFrames)),
        {},
    };
    for (const auto rate : reverb::dsp::pitch_shift::qualificationSampleRates)
        report.automation.push_back(measureAutomation(rate));
    return report;
}

std::string writeSplitFeedbackShimmerValidationJson(
    const SplitFeedbackShimmerValidationReport& report)
{
    auto automation = nlohmann::ordered_json::array();
    for (const auto& rate : report.automation) {
        automation.push_back({
            { "sampleRate", rate.sampleRate }, { "finite", rate.finite },
            { "peak", rate.peak }, { "maximumAdjacentStep", rate.maximumAdjacentStep },
            { "processedFrames", rate.processedFrames }, { "successfulEdits", rate.successfulEdits },
            { "completedCrossfades", rate.completedCrossfades },
        });
    }
    return nlohmann::ordered_json {
        { "formatVersion", 1 },
        { "measurement", "split-feedback-shimmer-validation" },
        { "spectralMethod", {
            { "sampleRate", report.sampleRate }, { "sourceFrequencyHertz", report.sourceFrequencyHertz },
            { "window", "Hann" }, { "windowSeconds", report.windowSeconds },
            { "sourceBandRelativeHalfWidth", 0.02 },
            { "octaveBandRelativeHalfWidth", 0.008 },
            { "expectedOctave12Hertz", report.sourceFrequencyHertz * 2.0 },
            { "expectedOctave24Hertz", report.sourceFrequencyHertz * 4.0 },
        } },
        { "circulation", {
            { "early", windowJson(report.early) }, { "late", windowJson(report.late) },
            { "lateOctave24GrowthDb", report.lateOctave24GrowthDb },
            { "splitLateOctave24RelativeTo12Db", report.splitLateOctave24RelativeTo12Db },
            { "parallelLateOctave24RelativeTo12Db", report.parallelLateOctave24RelativeTo12Db },
            { "feedbackVsParallelOctave24ContrastDb", report.feedbackVsParallelOctave24ContrastDb },
        } },
        { "independence", {
            { "lowShiftedFeedback", report.lowShiftedFeedback },
            { "highShiftedFeedback", report.highShiftedFeedback },
            { "lowFeedbackLateOctave24Dbfs", report.lowFeedbackLateOctave24Dbfs },
            { "highFeedbackLateOctave24Dbfs", report.highFeedbackLateOctave24Dbfs },
            { "shiftedFeedbackOctave24IncreaseDb", report.shiftedFeedbackOctave24IncreaseDb },
            { "lowNormalFeedback", report.lowNormalFeedback },
            { "highNormalFeedback", report.highNormalFeedback },
            { "normalFeedbackTailEnergyIncreaseDb", report.normalFeedbackTailEnergyIncreaseDb },
        } },
        { "qualityDisclosure", {
            { "strongestForwardGrainSidebandRelativeDb", report.strongestForwardGrainSidebandRelativeDb },
            { "grainSidebandOffsetHertz", 1'000.0 / 60.0 },
            { "foldedAliasProbeInputHertz", 7'000.0 },
            { "foldedAliasProbeHertz", 20'000.0 },
            { "foldedAliasRelativeToFirstOctaveDb", report.foldedAliasRelativeToFirstOctaveDb },
            { "lowDampingHertz", report.lowDampingHertz },
            { "highDampingHertz", report.highDampingHertz },
            { "lowDampingOctave24LossDb", report.lowDampingOctave24LossDb },
            { "stereoCorrelation", report.stereoCorrelation },
            { "warning", "Dual-grain linear interpolation is not alias-free; visible filtering reduces but does not remove folded energy or grain sidebands." },
        } },
        { "automation", {
            { "editCountPerRate", 18 },
            { "parameters", nlohmann::ordered_json::array({
                "normal-feedback", "shifted-feedback", "pitch-semitones", "post-shift-damping", "tank-size",
            }) },
            { "transition", "fixed 10 ms two-runtime crossfade" },
            { "rates", std::move(automation) },
        } },
    }.dump(2);
}

} // namespace reverb::render
