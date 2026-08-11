#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/Allpass.h>
#include <reverb/dsp/BarrReference.h>
#include <reverb/dsp/Delay.h>
#include <reverb/dsp/Gain.h>
#include <reverb/dsp/OnePoleLowPass.h>
#include <reverb/dsp/Sum.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <numbers>
#include <vector>

namespace {

double responseMagnitude(const std::span<const float> impulse, const double radiansPerSample)
{
    double real = 0.0;
    double imaginary = 0.0;
    for (std::size_t index = 0; index < impulse.size(); ++index) {
        const auto phase = radiansPerSample * static_cast<double>(index);
        real += static_cast<double>(impulse[index]) * std::cos(phase);
        imaginary -= static_cast<double>(impulse[index]) * std::sin(phase);
    }
    return std::hypot(real, imaginary);
}

} // namespace

TEST_CASE("Barr reference accepts unusual positive host sample rates")
{
    reverb::dsp::BarrReference reference;
    REQUIRE_NOTHROW(reference.prepare(1'234.5678));
    REQUIRE_NOTHROW(reference.prepare(1'234'567.8));
}

TEST_CASE("Delay timing follows milliseconds at multiple sample rates")
{
    for (const auto sampleRate : { 44'100.0, 96'000.0 }) {
        reverb::dsp::Delay delay;
        delay.prepare(sampleRate, 10.0);
        const auto expected = static_cast<std::size_t>(std::llround(sampleRate * 0.01));
        REQUIRE(delay.delaySamples() == expected);

        std::vector<float> samples(expected + 2, 0.0F);
        samples.front() = 1.0F;
        delay.process(samples);
        REQUIRE(samples[expected] == 1.0F);
        REQUIRE(std::accumulate(samples.begin(), samples.end(), 0.0F) == 1.0F);

        delay.reset();
        std::ranges::fill(samples, 0.0F);
        delay.process(samples);
        REQUIRE(std::ranges::all_of(samples, [](const float sample) { return sample == 0.0F; }));
    }
}

TEST_CASE("General and coefficient-half allpasses preserve impulse energy")
{
    for (const auto coefficient : { 0.5F, -0.73F }) {
        reverb::dsp::Allpass allpass;
        allpass.prepare(48'000.0, 1.0, coefficient);
        REQUIRE(allpass.coefficient() == coefficient);

        std::vector<float> impulse(8'192, 0.0F);
        impulse.front() = 1.0F;
        allpass.process(impulse);
        const auto energy = std::inner_product(impulse.begin(), impulse.end(), impulse.begin(), 0.0);
        REQUIRE(energy == Catch::Approx(1.0).margin(1.0e-5));
        for (const auto normalizedFrequency : { 0.0, 0.1, 0.37, 0.9 }) {
            const auto magnitude = responseMagnitude(
                impulse, normalizedFrequency * std::numbers::pi);
            REQUIRE(magnitude == Catch::Approx(1.0).margin(1.0e-5));
        }

        allpass.reset();
        std::ranges::fill(impulse, 0.0F);
        allpass.process(impulse);
        REQUIRE(std::ranges::all_of(impulse, [](const float sample) { return sample == 0.0F; }));
    }
}

TEST_CASE("Explicit sum and polarity match signed reference vectors")
{
    const std::array left { 1.0F, -2.0F, 3.0F };
    const std::array invertedRight { -0.5F, -2.0F, 1.0F };
    std::array<float, 3> output {};

    reverb::dsp::Sum::process(left, invertedRight, output);

    REQUIRE(output == std::array { 0.5F, -4.0F, 4.0F });
}

TEST_CASE("One-pole low-pass has bounded step response and deterministic reset")
{
    reverb::dsp::OnePoleLowPass filter;
    filter.prepare(48'000.0, 2'000.0);
    std::array<float, 64> step;
    step.fill(1.0F);
    filter.process(step);

    REQUIRE(step.front() > 0.0F);
    REQUIRE(step.back() < 1.0F);
    REQUIRE(std::ranges::is_sorted(step));

    filter.reset();
    step.fill(0.0F);
    filter.process(step);
    REQUIRE(std::ranges::all_of(step, [](const float sample) { return sample == 0.0F; }));
}

TEST_CASE("Gain and low-pass targets smooth continuous edits")
{
    reverb::dsp::Gain gain;
    gain.prepare(48'000.0, 0.0F, 20.0);
    gain.setTargetLinear(1.0F);
    std::vector<float> gainSignal(960, 1.0F);
    gain.process(gainSignal);
    REQUIRE(gainSignal.front() > 0.0F);
    REQUIRE(gainSignal.front() < 0.01F);
    REQUIRE(gainSignal.back() == Catch::Approx(1.0F));
    REQUIRE(std::ranges::is_sorted(gainSignal));

    reverb::dsp::OnePoleLowPass filter;
    filter.prepare(48'000.0, 500.0);
    std::vector<float> sine(4'800);
    for (std::size_t index = 0; index < sine.size(); ++index)
        sine[index] = static_cast<float>(
            std::sin(2.0 * std::numbers::pi * 1'000.0 * static_cast<double>(index) / 48'000.0));
    filter.process(std::span<float>(sine).first(2'400));
    filter.setCutoffHertz(12'000.0);
    filter.process(std::span<float>(sine).subspan(2'400));
    double maximumStep = 0.0;
    for (std::size_t index = 1; index < sine.size(); ++index)
        maximumStep = std::max(maximumStep, std::abs(static_cast<double>(sine[index] - sine[index - 1])));
    REQUIRE(maximumStep < 0.2);
    REQUIRE(std::ranges::all_of(sine, [](const float sample) { return std::isfinite(sample); }));
}

TEST_CASE("Allpass delay edits crossfade with finite bounded output")
{
    reverb::dsp::Allpass allpass;
    allpass.prepare(48'000.0, 5.0, 0.7F, 100.0);
    std::vector<float> signal(9'600);
    for (std::size_t index = 0; index < signal.size(); ++index)
        signal[index] = 0.25F * static_cast<float>(
            std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(index) / 48'000.0));
    allpass.process(std::span<float>(signal).first(4'800));
    allpass.setDelayMilliseconds(73.0);
    allpass.setCoefficient(-0.4F);
    allpass.process(std::span<float>(signal).subspan(4'800));

    REQUIRE(allpass.delayMilliseconds() == 73.0);
    REQUIRE(std::ranges::all_of(signal, [](const float sample) { return std::isfinite(sample); }));
    const auto maximum = std::transform_reduce(
        signal.begin(), signal.end(), 0.0F,
        [](const float left, const float right) { return std::max(left, right); },
        [](const float sample) { return std::abs(sample); });
    REQUIRE(maximum < 1.0F);
}

TEST_CASE("Constant modulated delay matches the equivalent static integer delay")
{
    constexpr auto sampleRate = 48'000.0;
    constexpr auto delayMilliseconds = 7.5;
    reverb::dsp::Delay fixed;
    fixed.prepare(sampleRate, delayMilliseconds);

    const auto capacity = static_cast<std::size_t>(
        std::ceil(sampleRate * 20.0 / 1000.0)) + 1;
    std::vector<float> storage(capacity);
    reverb::dsp::Delay modulated;
    modulated.prepareModulated(sampleRate, delayMilliseconds, 20.0, storage);

    std::vector<float> expected(2'048);
    for (std::size_t index = 0; index < expected.size(); ++index)
        expected[index] = static_cast<float>(std::sin(0.017 * static_cast<double>(index)));
    auto actual = expected;
    std::vector<double> control(actual.size(), delayMilliseconds);
    fixed.process(expected);
    modulated.processModulated(actual, control);
    REQUIRE(actual == expected);
}

TEST_CASE("Linear delay interpolation remains bounded at minimum maximum and full-depth modulation")
{
    constexpr auto sampleRate = 48'000.0;
    constexpr auto maximumMilliseconds = 100.0;
    const auto capacity = static_cast<std::size_t>(
        std::ceil(sampleRate * maximumMilliseconds / 1000.0)) + 1;
    std::vector<float> storage(capacity);
    reverb::dsp::Delay delay;
    delay.prepareModulated(sampleRate, 1.0, maximumMilliseconds, storage);

    std::vector<float> signal(24'000);
    std::vector<double> control(signal.size());
    for (std::size_t index = 0; index < signal.size(); ++index) {
        signal[index] = 0.2F * static_cast<float>(std::sin(0.11 * static_cast<double>(index)));
        const auto sweep = static_cast<double>(index) / static_cast<double>(signal.size() - 1);
        control[index] = index % 2 == 0
            ? 1000.0 / sampleRate
            : maximumMilliseconds * sweep;
    }
    delay.processModulated(signal, control);
    REQUIRE(std::ranges::all_of(signal, [](const float sample) { return std::isfinite(sample); }));
    REQUIRE(delay.delayMilliseconds() >= 1000.0 / sampleRate);
    REQUIRE(delay.delayMilliseconds() <= maximumMilliseconds);
}

TEST_CASE("Constant modulated allpass matches static processing and clamps extreme controls")
{
    constexpr auto sampleRate = 48'000.0;
    constexpr auto delayMilliseconds = 5.0;
    constexpr auto coefficient = 0.65;
    reverb::dsp::Allpass fixed;
    fixed.prepare(sampleRate, delayMilliseconds, static_cast<float>(coefficient), 100.0);
    reverb::dsp::Allpass modulated;
    modulated.prepare(sampleRate, delayMilliseconds, static_cast<float>(coefficient), 100.0);

    std::vector<float> expected(8'192, 0.0F);
    expected.front() = 1.0F;
    auto actual = expected;
    std::vector<double> delays(actual.size(), delayMilliseconds);
    std::vector<double> coefficients(actual.size(), coefficient);
    fixed.process(expected);
    modulated.processModulated(actual, delays, coefficients);
    REQUIRE(std::ranges::equal(actual, expected, [](const float left, const float right) {
        return std::abs(left - right) <= 1.0e-7F;
    }));

    std::ranges::fill(actual, 0.1F);
    for (std::size_t index = 0; index < actual.size(); ++index) {
        delays[index] = index % 2 == 0 ? -1'000.0 : 1'000.0;
        coefficients[index] = index % 2 == 0 ? -100.0 : 100.0;
    }
    modulated.processModulated(actual, delays, coefficients);
    REQUIRE(std::ranges::all_of(actual, [](const float sample) { return std::isfinite(sample); }));
    REQUIRE(std::abs(modulated.coefficient()) <= 0.95F);
    REQUIRE(modulated.delayMilliseconds() <= 100.0);
}
