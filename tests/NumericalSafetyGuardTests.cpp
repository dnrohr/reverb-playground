#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/NumericalSafetyGuard.h>

#include <array>
#include <limits>

TEST_CASE("Numerical safety guard latches mute on non-finite output")
{
    reverb::dsp::NumericalSafetyGuard guard;
    std::array samples { 0.25F, std::numeric_limits<float>::quiet_NaN(), 0.5F };

    const auto status = guard.inspectAndMute(samples);

    REQUIRE(status.violation == reverb::dsp::SafetyViolation::nonFinite);
    REQUIRE(status.sampleIndex == 1);
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
    REQUIRE(guard.inspectAndMute(runaway).violation == reverb::dsp::SafetyViolation::runawayLevel);

    std::array later { 0.25F, -0.25F };
    REQUIRE(guard.inspectAndMute(later).violation == reverb::dsp::SafetyViolation::none);
    REQUIRE(later == std::array { 0.0F, 0.0F });

    guard.reset();
    later = { 0.25F, -0.25F };
    REQUIRE(guard.inspectAndMute(later).violation == reverb::dsp::SafetyViolation::none);
    REQUIRE(later == std::array { 0.25F, -0.25F });
}
