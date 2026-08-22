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
