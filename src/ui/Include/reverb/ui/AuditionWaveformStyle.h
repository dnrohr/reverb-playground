#pragma once

#include <cstdint>

namespace reverb::ui {

struct AuditionWaveformColours final {
    std::uint32_t unselected;
    std::uint32_t selected;
    std::uint32_t selectionFill;
};

[[nodiscard]] constexpr AuditionWaveformColours auditionWaveformColours(const bool loopEnabled) noexcept
{
    // Slate remains readable against the near-black deck. Cyan identifies an
    // editable range; the product amber identifies the range that will repeat.
    return loopEnabled
        ? AuditionWaveformColours {0xff34414aU, 0xfff4b54cU, 0x30f4b54cU}
        : AuditionWaveformColours {0xff34414aU, 0xff45d0ccU, 0x1845d0ccU};
}

} // namespace reverb::ui
