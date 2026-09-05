#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/Gain.h>

#include <array>

TEST_CASE("Gain scales a sample span without changing its size")
{
    reverb::dsp::Gain gain;
    gain.setLinear(-0.5F);
    std::array samples { 1.0F, -2.0F, 0.0F, 4.0F };

    gain.process(samples);

    REQUIRE(samples == std::array { -0.5F, 1.0F, -0.0F, -2.0F });
    REQUIRE(gain.getLinear() == -0.5F);
}

TEST_CASE("Gain advances one shared smoothing ramp across paired stereo samples")
{
    reverb::dsp::Gain gain;
    gain.prepare(1'000.0, 1.0F, 2.0);
    gain.setTargetLinear(3.0F);
    std::array left { 1.0F, 1.0F, 1.0F };
    std::array right { -1.0F, -1.0F, -1.0F };

    gain.processStereo(left, right);

    REQUIRE(left == std::array { 2.0F, 3.0F, 3.0F });
    REQUIRE(right == std::array { -2.0F, -3.0F, -3.0F });
}
