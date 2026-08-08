#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/Allpass.h>
#include <reverb/dsp/Delay.h>
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
