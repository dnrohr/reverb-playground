#include <catch2/catch_test_macros.hpp>

#include <reverb/ui/AuditionDeckLayout.h>

#include <array>

namespace {

template <std::size_t Size>
void requireContainedAndSeparate(
    const reverb::ui::LayoutRect& parent,
    const std::array<reverb::ui::LayoutRect, Size>& controls)
{
    for (std::size_t index = 0; index < controls.size(); ++index) {
        REQUIRE_FALSE(controls[index].isEmpty());
        REQUIRE(parent.contains(controls[index]));
        for (std::size_t other = index + 1; other < controls.size(); ++other)
            REQUIRE_FALSE(controls[index].intersects(controls[other]));
    }
}

} // namespace

TEST_CASE("Compact audition strip remains in bounds at every supported editor width")
{
    for (const auto width : { 640, 720, 899, 900, 1200, 1536, 1920 }) {
        const auto layout = reverb::ui::calculateAuditionDeckLayout(width, false);
        CAPTURE(width);
        REQUIRE(layout.headerHeight < 150);
        REQUIRE(layout.deckBounds == reverb::ui::LayoutRect {
            14, 10 + reverb::ui::globalControlHeightForWidth(width) + 6, width - 28, 28 });
        requireContainedAndSeparate(layout.deckBounds, layout.compactControls());
        for (const auto& control : layout.drawerControls()) REQUIRE(control.isEmpty());
        REQUIRE(layout.summary.width >= 210);
    }
}

TEST_CASE("Compact wide chrome returns thirty-two native pixels to the schematic")
{
    constexpr auto releasedWideClosedHeaderHeight = 123;
    const auto compact = reverb::ui::calculateAuditionDeckLayout(1'200, false);
    REQUIRE(compact.headerHeight == 91);
    REQUIRE(releasedWideClosedHeaderHeight - compact.headerHeight == 32);
}

TEST_CASE("Expanded audition drawer remains in bounds through resize and transport states")
{
    struct State final {
        bool fileLoaded;
        bool playing;
        bool looping;
        bool exporting;
    };
    constexpr std::array states {
        State { false, false, false, false }, State { true, false, false, false },
        State { true, true, false, false }, State { true, true, true, false },
        State { true, false, true, true }, State { true, true, true, true },
    };

    for (const auto width : { 640, 720, 899, 900, 1200, 1536, 1920 }) {
        const auto layout = reverb::ui::calculateAuditionDeckLayout(width, true);
        for (const auto state : states) {
            CAPTURE(width, state.fileLoaded, state.playing, state.looping, state.exporting);
            REQUIRE(layout.headerHeight <= 241);
            requireContainedAndSeparate(layout.deckBounds, layout.compactControls());
            requireContainedAndSeparate(layout.deckBounds, layout.drawerControls());
            for (const auto& compact : layout.compactControls())
                for (const auto& detail : layout.drawerControls())
                    REQUIRE_FALSE(compact.intersects(detail));
            REQUIRE(layout.exportProgress.width >= 142);
            REQUIRE(layout.loopRange.width >= 202);
        }
    }
}

TEST_CASE("Drawer changes only vertical chrome and preserves compact control geometry")
{
    for (const auto width : { 640, 899, 900, 1200, 1920 }) {
        const auto closed = reverb::ui::calculateAuditionDeckLayout(width, false);
        const auto open = reverb::ui::calculateAuditionDeckLayout(width, true);
        CAPTURE(width);
        REQUIRE(closed.compactControls() == open.compactControls());
        REQUIRE(open.headerHeight - closed.headerHeight == 96);
        REQUIRE(open.headerHeight < 400);
    }
}
