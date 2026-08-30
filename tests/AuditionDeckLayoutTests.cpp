#include <catch2/catch_test_macros.hpp>

#include <reverb/ui/AuditionDeckLayout.h>

#include <array>
#include <utility>

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
        const auto layout = reverb::ui::calculateAuditionDeckLayout(width, false, 720);
        CAPTURE(width);
        REQUIRE(layout.bottomDeckHeight <= 78);
        REQUIRE(layout.deckBounds.bottom() == 720);
        REQUIRE(layout.browserBounds.y == layout.topBarHeight);
        REQUIRE(layout.browserBounds.bottom() == layout.deckBounds.y);
        requireContainedAndSeparate(layout.deckBounds, layout.compactControls());
        for (const auto& control : layout.drawerControls()) REQUIRE(control.isEmpty());
        REQUIRE(layout.summary.width >= 121);
    }
}

TEST_CASE("Compact bottom deck leaves the desktop schematic dominant")
{
    const auto compact = reverb::ui::calculateAuditionDeckLayout(1'200, false, 720);
    REQUIRE(compact.topBarHeight == 47);
    REQUIRE(compact.bottomDeckHeight == 42);
    REQUIRE(compact.browserBounds.height == 631);
    REQUIRE(compact.browserBounds.height > 7 * compact.bottomDeckHeight);
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
        const auto layout = reverb::ui::calculateAuditionDeckLayout(width, true, 720);
        for (const auto state : states) {
            CAPTURE(width, state.fileLoaded, state.playing, state.looping, state.exporting);
            REQUIRE(layout.bottomDeckHeight <= 186);
            requireContainedAndSeparate(layout.deckBounds, layout.compactControls());
            requireContainedAndSeparate(layout.deckBounds, layout.drawerControls());
            for (const auto& compact : layout.compactControls())
                for (const auto& detail : layout.drawerControls())
                    REQUIRE_FALSE(compact.intersects(detail));
            REQUIRE(layout.exportProgress.width >= 142);
            REQUIRE(layout.loopRange.width >= 171);
            REQUIRE(layout.mixDisclosure.width >= 111);
        }
    }
}

TEST_CASE("Drawer changes only vertical chrome and preserves compact control geometry")
{
    for (const auto width : { 640, 899, 900, 1200, 1920 }) {
        const auto closed = reverb::ui::calculateAuditionDeckLayout(width, false, 720);
        const auto open = reverb::ui::calculateAuditionDeckLayout(width, true, 720);
        CAPTURE(width);
        REQUIRE(closed.compactControls() == open.compactControls());
        REQUIRE(open.bottomDeckHeight - closed.bottomDeckHeight == 108);
        REQUIRE(open.browserBounds.height == closed.browserBounds.height - 108);
        REQUIRE(open.deckBounds.bottom() == closed.deckBounds.bottom());
    }
}

TEST_CASE("Bottom deck remains bounded at minimum height and common Windows scaling sizes")
{
    for (const auto [width, height] : std::array {
        std::pair { 640, 400 }, std::pair { 960, 576 }, std::pair { 1200, 720 },
        std::pair { 1536, 864 }, std::pair { 1920, 1080 },
    }) {
        for (const auto expanded : { false, true }) {
            const auto layout = reverb::ui::calculateAuditionDeckLayout(width, expanded, height);
            CAPTURE(width, height, expanded);
            REQUIRE(layout.deckBounds.y >= layout.topBarHeight);
            REQUIRE(layout.deckBounds.bottom() == height);
            REQUIRE(layout.browserBounds.height >= 0);
            REQUIRE_FALSE(layout.browserBounds.intersects(layout.deckBounds));
        }
    }
}
