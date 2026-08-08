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
