#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/PitchShift.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

namespace {

std::vector<float> renderSine(
    const double sampleRate,
    const reverb::dsp::PitchShiftParameters parameters,
    const double inputFrequency = 400.0,
    const double secondsAfterLatency = 0.8)
{
    reverb::dsp::PitchShift shifter;
    shifter.prepare(sampleRate, parameters);
    const auto frames = shifter.latencySamples()
        + static_cast<std::size_t>(std::llround(secondsAfterLatency * sampleRate));
    std::vector<float> samples(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        samples[frame] = static_cast<float>(std::sin(
            2.0 * std::numbers::pi * inputFrequency * static_cast<double>(frame) / sampleRate));
    }
    shifter.process(samples);
    return samples;
}

double strongestFrequency(
    const std::span<const float> samples,
    const double sampleRate,
    const double expectedFrequency)
{
    auto bestFrequency = 0.0;
    auto bestPower = -1.0;
    constexpr auto steps = 80;
    for (auto step = 0; step <= steps; ++step) {
        const auto frequency = expectedFrequency * (0.98 + 0.04 * static_cast<double>(step) / steps);
        const auto phaseStep = 2.0 * std::numbers::pi * frequency / sampleRate;
        auto real = 0.0;
        auto imaginary = 0.0;
        for (std::size_t frame = 0; frame < samples.size(); ++frame) {
            const auto phase = phaseStep * static_cast<double>(frame);
            real += static_cast<double>(samples[frame]) * std::cos(phase);
            imaginary -= static_cast<double>(samples[frame]) * std::sin(phase);
        }
        const auto power = real * real + imaginary * imaginary;
        if (power > bestPower) {
            bestPower = power;
            bestFrequency = frequency;
        }
    }
    return bestFrequency;
}

double centsFromRatio(const double measured, const double expected) noexcept
{
    return 1'200.0 * std::log2(measured / expected);
}

} // namespace

TEST_CASE("Prepared pitch shift measures the requested octave at qualified rates")
{
    using reverb::dsp::PitchShiftParameters;
    using reverb::dsp::pitch_shift::GrainDirection;
    using reverb::dsp::pitch_shift::qualificationSampleRates;
    for (const auto sampleRate : qualificationSampleRates) {
        for (const auto semitones : { -12.0, 12.0 }) {
            CAPTURE(sampleRate, semitones);
            const PitchShiftParameters parameters {
                semitones, 60.0, 0.5, GrainDirection::forward
            };
            const auto rendered = renderSine(sampleRate, parameters);
            const auto latency = reverb::dsp::pitch_shift::reportedLatencySamples(sampleRate);
            const auto analysisStart = latency + static_cast<std::size_t>(sampleRate * 0.2);
            const auto analysisFrames = static_cast<std::size_t>(sampleRate * 0.5);
            const auto expected = 400.0 * std::pow(2.0, semitones / 12.0);
            const auto measured = strongestFrequency(
                std::span<const float>(rendered).subspan(analysisStart, analysisFrames),
                sampleRate,
                expected);
            REQUIRE(std::abs(centsFromRatio(measured, expected)) <= 15.0);
        }
    }
}

TEST_CASE("Pitch shift silence bounds and prepared storage survive every endpoint")
{
    using namespace reverb::dsp;
    constexpr auto sampleRate = 48'000.0;
    const auto required = pitch_shift::preparedStorageSamples(sampleRate);
    std::vector<float> guarded(required + 2, 123.0F);
    for (const auto semitones : { pitch_shift::minimumSemitones, pitch_shift::maximumSemitones }) {
        for (const auto grain : { pitch_shift::minimumGrainMilliseconds, pitch_shift::maximumGrainMilliseconds }) {
            for (const auto overlap : { pitch_shift::minimumOverlap, pitch_shift::maximumOverlap }) {
                for (const auto direction : { pitch_shift::GrainDirection::forward,
                         pitch_shift::GrainDirection::reverse }) {
                    CAPTURE(semitones, grain, overlap, direction);
                    PitchShift shifter;
                    shifter.prepare(sampleRate, { semitones, grain, overlap, direction },
                        std::span<float>(guarded).subspan(1, required));
                    std::vector<float> silence(2'048, 0.0F);
                    shifter.process(silence);
                    REQUIRE(std::ranges::all_of(silence, [](const float sample) { return sample == 0.0F; }));

                    std::vector<float> bounded(shifter.latencySamples() + 12'000);
                    for (std::size_t frame = 0; frame < bounded.size(); ++frame)
                        bounded[frame] = frame % 2 == 0 ? 1.0F : -1.0F;
                    shifter.process(bounded);
                    for (const auto sample : bounded) {
                        REQUIRE(std::isfinite(sample));
                        REQUIRE(std::abs(sample) <= pitch_shift::maximumEqualPowerOutputMagnitude);
                    }
                    REQUIRE(guarded.front() == 123.0F);
                    REQUIRE(guarded.back() == 123.0F);
                    REQUIRE(shifter.storageSamples() == required);
                }
            }
        }
    }
}

TEST_CASE("Pitch shift is causal at its reported latency")
{
    using namespace reverb::dsp;
    PitchShift shifter;
    shifter.prepare(48'000.0);
    std::vector<float> impulse(shifter.latencySamples() + 2, 0.0F);
    impulse.front() = 1.0F;
    shifter.process(impulse);
    REQUIRE(std::ranges::all_of(
        std::span<const float>(impulse).first(shifter.latencySamples()),
        [](const float sample) { return sample == 0.0F; }));
}

TEST_CASE("Pitch shift reset and parameter transitions are deterministic and continuous")
{
    using namespace reverb::dsp;
    constexpr auto sampleRate = 48'000.0;
    const auto required = pitch_shift::preparedStorageSamples(sampleRate);
    std::vector<float> guarded(required + 2, 321.0F);
    PitchShift shifter;
    shifter.prepare(sampleRate,
        { -12.0, 40.0, 0.25, pitch_shift::GrainDirection::forward },
        std::span<float>(guarded).subspan(1, required));
    const auto frames = shifter.latencySamples() + static_cast<std::size_t>(sampleRate);
    std::vector<float> input(frames);
    for (std::size_t frame = 0; frame < frames; ++frame)
        input[frame] = static_cast<float>(std::sin(2.0 * std::numbers::pi * 440.0
            * static_cast<double>(frame) / sampleRate));

    const auto render = [&](PitchShift& processor) {
        auto output = input;
        const auto edit = processor.latencySamples() + 4'000;
        processor.process(std::span<float>(output).first(edit));
        processor.setParameters({ 12.0, 120.0, 1.0, pitch_shift::GrainDirection::reverse });
        processor.process(std::span<float>(output).subspan(edit));
        return output;
    };
    const auto first = render(shifter);
    shifter.reset();
    shifter.setParameters({ -12.0, 40.0, 0.25, pitch_shift::GrainDirection::forward });
    shifter.settleParameters();
    const auto second = render(shifter);
    REQUIRE(first == second);

    PitchShift sameConfiguration;
    sameConfiguration.prepare(sampleRate,
        { -12.0, 40.0, 0.25, pitch_shift::GrainDirection::forward });
    REQUIRE(first == render(sameConfiguration));

    auto maximumStep = 0.0F;
    const auto start = shifter.latencySamples();
    for (auto frame = start + 1; frame < first.size(); ++frame)
        maximumStep = std::max(maximumStep, std::abs(first[frame] - first[frame - 1]));
    REQUIRE(maximumStep < 0.25F);
    REQUIRE(guarded.front() == 321.0F);
    REQUIRE(guarded.back() == 321.0F);
}
