#include <reverb/ui/AuditionWaveformStyle.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Audition waveform selection uses themed contrast and amber loop state")
{
    const auto editable = reverb::ui::auditionWaveformColours(false);
    const auto looping = reverb::ui::auditionWaveformColours(true);

    REQUIRE(editable.unselected == 0xff34414aU);
    REQUIRE(editable.selected == 0xff45d0ccU);
    REQUIRE(looping.unselected == editable.unselected);
    REQUIRE(looping.selected == 0xfff4b54cU);
    REQUIRE(looping.selectionFill != editable.selectionFill);
}
