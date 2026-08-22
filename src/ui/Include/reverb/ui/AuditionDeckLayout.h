#pragma once

#include <array>
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
    int headerHeight {};
    LayoutRect deckBounds;
    LayoutRect sourceMode;
    LayoutRect loadFile;
    LayoutRect playPause;
    LayoutRect summary;
    LayoutRect exportWav;
    LayoutRect drawerToggle;
    LayoutRect waveform;
    LayoutRect stop;
    LayoutRect loop;
    LayoutRect processed;
    LayoutRect exportMode;
    LayoutRect exportProgress;
    LayoutRect seek;
    LayoutRect loopRange;

    [[nodiscard]] std::array<LayoutRect, 6> compactControls() const noexcept
    {
        return { sourceMode, loadFile, playPause, summary, exportWav, drawerToggle };
    }

    [[nodiscard]] std::array<LayoutRect, 8> drawerControls() const noexcept
    {
        return { waveform, stop, loop, processed, exportMode, exportProgress, seek, loopRange };
    }
};

[[nodiscard]] constexpr int globalControlHeightForWidth(const int width) noexcept
{
    return width < 900 ? 87 : 65;
}

[[nodiscard]] inline AuditionDeckLayout calculateAuditionDeckLayout(
    const int width, const bool expanded) noexcept
{
    constexpr int outerInset = 14;
    constexpr int topInset = 10;
    constexpr int gap = 6;
    constexpr int compactHeight = 32;
    constexpr int bottomInset = 10;
    const auto availableWidth = width > outerInset * 2 ? width - outerInset * 2 : 0;
    const auto globalHeight = globalControlHeightForWidth(width);
    const auto deckY = topInset + globalHeight + gap;
    const auto deckHeight = expanded ? 128 : compactHeight;
    AuditionDeckLayout result;
    result.headerHeight = deckY + deckHeight + bottomInset;
    result.deckBounds = { outerInset, deckY, availableWidth, deckHeight };

    const auto narrow = width < 900;
    const auto sourceWidth = narrow ? 104 : 122;
    const auto loadWidth = narrow ? 84 : 104;
    const auto playWidth = narrow ? 62 : 72;
    const auto exportWidth = narrow ? 88 : 108;
    constexpr int toggleWidth = 34;
    constexpr int controlGap = 6;
    const auto fixedWidth = sourceWidth + loadWidth + playWidth + exportWidth + toggleWidth + controlGap * 5;
    const auto summaryWidth = availableWidth > fixedWidth ? availableWidth - fixedWidth : 0;
    auto x = outerInset;
    result.sourceMode = { x, deckY, sourceWidth, compactHeight }; x += sourceWidth + controlGap;
    result.loadFile = { x, deckY, loadWidth, compactHeight }; x += loadWidth + controlGap;
    result.playPause = { x, deckY, playWidth, compactHeight }; x += playWidth + controlGap;
    result.summary = { x, deckY, summaryWidth, compactHeight }; x += summaryWidth + controlGap;
    result.exportWav = { x, deckY, exportWidth, compactHeight }; x += exportWidth + controlGap;
    result.drawerToggle = { x, deckY, toggleWidth, compactHeight };

    if (!expanded) return result;

    const auto waveformY = deckY + compactHeight + 6;
    result.waveform = { outerInset, waveformY, availableWidth, 38 };
    const auto controlY = waveformY + 42;
    const auto stopWidth = narrow ? 58 : 66;
    const auto loopWidth = narrow ? 58 : 66;
    const auto processedWidth = narrow ? 98 : 112;
    const auto exportModeWidth = narrow ? 108 : 128;
    const auto controlsFixed = stopWidth + loopWidth + processedWidth + exportModeWidth + controlGap * 4;
    const auto progressWidth = availableWidth > controlsFixed ? availableWidth - controlsFixed : 0;
    x = outerInset;
    result.stop = { x, controlY, stopWidth, 26 }; x += stopWidth + controlGap;
    result.loop = { x, controlY, loopWidth, 26 }; x += loopWidth + controlGap;
    result.processed = { x, controlY, processedWidth, 26 }; x += processedWidth + controlGap;
    result.exportMode = { x, controlY, exportModeWidth, 26 }; x += exportModeWidth + controlGap;
    result.exportProgress = { x, controlY, progressWidth, 26 };
    const auto sliderY = controlY + 30;
    const auto seekWidth = static_cast<int>(availableWidth * 0.66);
    result.seek = { outerInset, sliderY, seekWidth, 18 };
    result.loopRange = { outerInset + seekWidth + controlGap, sliderY,
        availableWidth - seekWidth - controlGap, 18 };
    return result;
}

} // namespace reverb::ui
