#pragma once

#include <array>
#include <algorithm>
#include <compare>

namespace reverb::ui {

struct LayoutRect final {
    int x {};
    int y {};
    int width {};
    int height {};

    auto operator<=>(const LayoutRect&) const = default;

    [[nodiscard]] constexpr bool isEmpty() const noexcept { return width <= 0 || height <= 0; }
    [[nodiscard]] constexpr int right() const noexcept { return x + width; }
    [[nodiscard]] constexpr int bottom() const noexcept { return y + height; }
    [[nodiscard]] constexpr bool contains(const LayoutRect& other) const noexcept
    {
        return other.isEmpty()
            || (other.x >= x && other.y >= y && other.right() <= right() && other.bottom() <= bottom());
    }
    [[nodiscard]] constexpr bool intersects(const LayoutRect& other) const noexcept
    {
        return !isEmpty() && !other.isEmpty()
            && x < other.right() && other.x < right() && y < other.bottom() && other.y < bottom();
    }
};

struct AuditionDeckLayout final {
    int topBarHeight {};
    int bottomDeckHeight {};
    LayoutRect browserBounds;
    LayoutRect deckBounds;
    LayoutRect sourceMode;
    LayoutRect loadFile;
    LayoutRect playPause;
    LayoutRect summary;
    LayoutRect exportWav;
    LayoutRect drawerToggle;
    LayoutRect wetGain;
    LayoutRect dryGain;
    LayoutRect quickImpulse;
    LayoutRect waveform;
    LayoutRect stop;
    LayoutRect loop;
    LayoutRect exportMode;
    LayoutRect exportProgress;
    LayoutRect seek;
    LayoutRect loopRange;
    LayoutRect mixDisclosure;

    [[nodiscard]] std::array<LayoutRect, 9> compactControls() const noexcept
    {
        return { sourceMode, loadFile, playPause, stop, summary, wetGain, dryGain,
            quickImpulse, drawerToggle, };
    }

    [[nodiscard]] std::array<LayoutRect, 7> drawerControls() const noexcept
    {
        return { waveform, loop, exportWav, exportMode, exportProgress, seek, loopRange };
    }
};

[[nodiscard]] constexpr int globalControlHeightForWidth(const int width) noexcept
{
    return width < 900 ? 87 : 37;
}

[[nodiscard]] inline AuditionDeckLayout calculateAuditionDeckLayout(
    const int width, const bool expanded, const int height = 720) noexcept
{
    constexpr int outerInset = 14;
    constexpr int gap = 6;
    constexpr int compactHeight = 30;
    const auto availableWidth = width > outerInset * 2 ? width - outerInset * 2 : 0;
    const auto narrow = width < 1200;
    const auto compactRows = narrow ? 2 : 1;
    const auto compactAreaHeight = compactRows * compactHeight + (compactRows - 1) * gap + 12;
    const auto drawerAreaHeight = expanded ? 108 : 0;
    AuditionDeckLayout result;
    result.topBarHeight = narrow ? 69 : 47;
    result.bottomDeckHeight = compactAreaHeight + drawerAreaHeight;
    const auto deckY = std::max(result.topBarHeight, height - result.bottomDeckHeight);
    result.deckBounds = { 0, deckY, width, result.bottomDeckHeight };
    result.browserBounds = { 0, result.topBarHeight, width,
        std::max(0, deckY - result.topBarHeight) };

    const auto compactY = height - compactAreaHeight + 6;
    const auto sourceWidth = narrow ? 105 : 116;
    const auto loadWidth = narrow ? 88 : 98;
    const auto playWidth = narrow ? 62 : 68;
    const auto stopWidth = narrow ? 58 : 66;
    constexpr int toggleWidth = 34;
    constexpr int controlGap = 6;
    const auto impulseWidth = narrow ? 96 : 108;
    const auto gainWidth = narrow ? 0 : 172;
    const auto fixedWidth = sourceWidth + loadWidth + playWidth + stopWidth + impulseWidth + toggleWidth
        + gainWidth * 2 + controlGap * 8;
    const auto summaryWidth = availableWidth > fixedWidth ? availableWidth - fixedWidth : 0;
    auto x = outerInset;
    result.sourceMode = { x, compactY, sourceWidth, compactHeight }; x += sourceWidth + controlGap;
    result.loadFile = { x, compactY, loadWidth, compactHeight }; x += loadWidth + controlGap;
    result.playPause = { x, compactY, playWidth, compactHeight }; x += playWidth + controlGap;
    result.stop = { x, compactY, stopWidth, compactHeight }; x += stopWidth + controlGap;
    result.summary = { x, compactY, summaryWidth, compactHeight }; x += summaryWidth + controlGap;
    if (!narrow) {
        result.wetGain = { x, compactY, gainWidth, compactHeight }; x += gainWidth + controlGap;
        result.dryGain = { x, compactY, gainWidth, compactHeight }; x += gainWidth + controlGap;
    }
    result.quickImpulse = { x, compactY, impulseWidth, compactHeight }; x += impulseWidth + controlGap;
    result.drawerToggle = { x, compactY, toggleWidth, compactHeight };

    if (narrow) {
        const auto gainY = compactY + compactHeight + gap;
        const auto gainAndImpulseWidth = availableWidth - toggleWidth - controlGap;
        const auto compactGainWidth = (gainAndImpulseWidth - impulseWidth - controlGap * 2) / 2;
        result.wetGain = { outerInset, gainY, compactGainWidth, compactHeight };
        result.dryGain = { outerInset + compactGainWidth + controlGap, gainY, compactGainWidth, compactHeight };
        result.quickImpulse = { outerInset + compactGainWidth * 2 + controlGap * 2,
            gainY, impulseWidth, compactHeight };
        result.drawerToggle = { width - outerInset - toggleWidth, gainY, toggleWidth, compactHeight };
    }

    if (!expanded) return result;

    const auto waveformY = deckY + 6;
    result.waveform = { outerInset, waveformY, availableWidth, 38 };
    const auto controlY = waveformY + 42;
    const auto loopWidth = narrow ? 58 : 66;
    const auto exportWidth = narrow ? 88 : 108;
    const auto exportModeWidth = narrow ? 108 : 128;
    const auto controlsFixed = loopWidth + exportWidth + exportModeWidth + controlGap * 3;
    const auto progressWidth = availableWidth > controlsFixed ? availableWidth - controlsFixed : 0;
    x = outerInset;
    result.loop = { x, controlY, loopWidth, 26 }; x += loopWidth + controlGap;
    result.exportWav = { x, controlY, exportWidth, 26 }; x += exportWidth + controlGap;
    result.exportMode = { x, controlY, exportModeWidth, 26 }; x += exportModeWidth + controlGap;
    result.exportProgress = { x, controlY, progressWidth, 26 };
    const auto sliderY = controlY + 30;
    const auto seekWidth = static_cast<int>(availableWidth * 0.52);
    result.seek = { outerInset, sliderY, seekWidth, 18 };
    const auto loopWidthRemaining = static_cast<int>(availableWidth * 0.28);
    result.loopRange = { outerInset + seekWidth + controlGap, sliderY, loopWidthRemaining, 18 };
    result.mixDisclosure = { result.loopRange.right() + controlGap, sliderY,
        std::max(0, width - outerInset - result.loopRange.right() - controlGap), 18 };
    return result;
}

} // namespace reverb::ui
