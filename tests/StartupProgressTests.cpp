#include "StartupProgress.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Standalone startup phases advance monotonically")
{
    reverb::app::StartupProgress progress;
    using reverb::app::StartupPhase;

    REQUIRE(progress.phase() == StartupPhase::showingShell);
    REQUIRE(progress.advanceTo(StartupPhase::connectingAudio));
    REQUIRE_FALSE(progress.advanceTo(StartupPhase::showingShell));
    REQUIRE(progress.advanceTo(StartupPhase::openingEditor));
    REQUIRE(progress.advanceTo(StartupPhase::ready));
    REQUIRE_FALSE(progress.advanceTo(StartupPhase::failed));
}

TEST_CASE("Standalone startup can fail from an unfinished phase")
{
    reverb::app::StartupProgress progress;
    using reverb::app::StartupPhase;

    REQUIRE(progress.advanceTo(StartupPhase::connectingAudio));
    REQUIRE(progress.advanceTo(StartupPhase::failed));
    REQUIRE_FALSE(progress.advanceTo(StartupPhase::openingEditor));
}

TEST_CASE("Standalone loading presentation fills over eight seconds then welcomes")
{
    const auto before = reverb::app::startupPresentation(-1.0);
    const auto middle = reverb::app::startupPresentation(4.0);
    const auto boundary = reverb::app::startupPresentation(8.0);
    const auto after = reverb::app::startupPresentation(12.0);

    REQUIRE(before.progress == 0.0);
    REQUIRE_FALSE(before.welcomed);
    REQUIRE(middle.progress == 0.5);
    REQUIRE_FALSE(middle.welcomed);
    REQUIRE(boundary.progress == 1.0);
    REQUIRE(boundary.welcomed);
    REQUIRE(after.progress == 1.0);
    REQUIRE(after.welcomed);
}
