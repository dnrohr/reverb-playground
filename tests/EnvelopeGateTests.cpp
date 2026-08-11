#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/EnvelopeFollower.h>
#include <reverb/dsp/HoldGate.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

TEST_CASE("Envelope follower attack release and reset are deterministic across sample rates")
{
    for (const auto sampleRate : { 44'100.0, 96'000.0 }) {
        reverb::dsp::EnvelopeFollower follower;
        follower.prepare(sampleRate, 10.0, 20.0);
        const auto attackSamples = static_cast<std::size_t>(std::llround(sampleRate * 0.01));
        float attackValue = 0.0F;
        for (std::size_t index = 0; index < attackSamples; ++index)
            attackValue = follower.processSample(1.0F);
        REQUIRE(attackValue == Catch::Approx(1.0 - std::exp(-1.0)).margin(0.001));

        const auto releaseSamples = static_cast<std::size_t>(std::llround(sampleRate * 0.02));
        float releaseValue = attackValue;
        for (std::size_t index = 0; index < releaseSamples; ++index)
            releaseValue = follower.processSample(0.0F);
        REQUIRE(releaseValue == Catch::Approx(attackValue * std::exp(-1.0)).margin(0.001));

        follower.reset();
        REQUIRE(follower.processSample(0.0F) == 0.0F);
        REQUIRE(follower.processSample(std::numeric_limits<float>::quiet_NaN()) == 0.0F);
    }
}

TEST_CASE("Hold gate has exact attack hold and release samples")
{
    reverb::dsp::HoldGate gate;
    gate.prepare(1'000.0, 0.5, 4.0, 3.0, 2.0);
    std::array<float, 9> input {}; input.fill(1.0F);
    const std::array control { 1.0F, 1.0F, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F };
    std::array<float, 9> output {};
    gate.process(input, control, output);
    REQUIRE(output == std::array { 0.25F, 0.5F, 0.75F, 1.0F, 1.0F, 1.0F, 1.0F, 0.5F, 0.0F });

    gate.reset();
    REQUIRE(gate.gain() == 0.0F);

    gate.prepare(1'000.0, 0.0, 1.0, 1.0, 1.0);
    REQUIRE(gate.processSample(1.0F, 0.0F) == 0.0F);
    REQUIRE(gate.processSample(1.0F, std::numeric_limits<float>::quiet_NaN()) == 0.0F);
}

TEST_CASE("Hold gate millisecond timing is sample-rate consistent and cannot amplify")
{
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0 }) {
        reverb::dsp::HoldGate gate;
        gate.prepare(sampleRate, 0.5, 10.0, 20.0, 10.0);
        const auto attack = static_cast<std::size_t>(std::llround(sampleRate * 0.01));
        const auto hold = static_cast<std::size_t>(std::llround(sampleRate * 0.02));
        const auto release = static_cast<std::size_t>(std::llround(sampleRate * 0.01));
        std::vector<float> output;
        output.reserve(attack + hold + release);
        for (std::size_t index = 0; index < attack; ++index)
            output.push_back(gate.processSample(-0.75F, 1.0F));
        for (std::size_t index = 0; index < hold + release; ++index)
            output.push_back(gate.processSample(-0.75F, 0.0F));
        REQUIRE(output[attack - 1] == Catch::Approx(-0.75F).margin(0.0001));
        REQUIRE(output[attack + hold - 1] == Catch::Approx(-0.75F).margin(0.0001));
        REQUIRE(output.back() == Catch::Approx(0.0F).margin(0.0001));
        REQUIRE(std::ranges::all_of(output, [](const auto sample) { return std::abs(sample) <= 0.75F; }));
    }
}
