#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/NumericalSafetyGuard.h>

#include <algorithm>
#include <array>
#include <limits>

TEST_CASE("Numerical safety guard latches mute on non-finite output")
{
    reverb::dsp::NumericalSafetyGuard guard;
    std::array samples { 0.25F, std::numeric_limits<float>::quiet_NaN(), 0.5F };

    const auto status = guard.inspectAndMute(samples);

    REQUIRE(status.violation == reverb::dsp::SafetyViolation::nonFinite);
    REQUIRE(status.sampleIndex == 1);
    REQUIRE(status.peakAbsoluteSample == 0.25F);
    REQUIRE(guard.isMuted());
    REQUIRE(samples == std::array { 0.0F, 0.0F, 0.0F });

    guard.reset();
    samples = { 0.25F, std::numeric_limits<float>::infinity(), 0.5F };
    REQUIRE(guard.inspectAndMute(samples).violation == reverb::dsp::SafetyViolation::nonFinite);
    REQUIRE(samples == std::array { 0.0F, 0.0F, 0.0F });
}

TEST_CASE("Numerical safety guard latches mute on runaway output until reset")
{
    reverb::dsp::NumericalSafetyGuard guard { 4.0F };
    std::array runaway { 0.5F, -4.25F };
    const auto status = guard.inspectAndMute(runaway);
    REQUIRE(status.violation == reverb::dsp::SafetyViolation::runawayLevel);
    REQUIRE(status.clippedSamples == 1);
    REQUIRE(status.peakAbsoluteSample == 4.25F);

    std::array later { 0.25F, -0.25F };
    REQUIRE(guard.inspectAndMute(later).violation == reverb::dsp::SafetyViolation::none);
    REQUIRE(later == std::array { 0.0F, 0.0F });

    guard.reset();
    later = { 0.25F, -0.25F };
    REQUIRE(guard.inspectAndMute(later).violation == reverb::dsp::SafetyViolation::none);
    REQUIRE(later == std::array { 0.25F, -0.25F });
}

TEST_CASE("Numerical safety guard requires a continuous sustained runaway interval")
{
    reverb::dsp::NumericalSafetyGuard guard { 16.0F, 4.0F, 50.0 };
    guard.prepare(1'000.0);

    std::array<float, 49> almostSustained {};
    almostSustained.fill(4.25F);
    REQUIRE(guard.inspectAndMute(almostSustained).violation == reverb::dsp::SafetyViolation::none);

    std::array resetInterval { 0.5F };
    REQUIRE(guard.inspectAndMute(resetInterval).violation == reverb::dsp::SafetyViolation::none);

    std::array<float, 50> sustained {};
    sustained.fill(-4.25F);
    const auto status = guard.inspectAndMute(sustained);
    REQUIRE(status.violation == reverb::dsp::SafetyViolation::runawayLevel);
    REQUIRE(status.sampleIndex == 49);
    REQUIRE(status.clippedSamples == 50);
    REQUIRE(std::ranges::all_of(sustained, [](const auto sample) { return sample == 0.0F; }));
}
