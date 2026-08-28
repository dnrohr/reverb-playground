#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/DelaySetTuning.h>
#include <reverb/render/DelaySetResponseRanking.h>

#include <algorithm>
#include <cmath>
#include <limits>

TEST_CASE("Rendered delay-set ranking keeps every perceptual dimension explicit")
{
    const auto search = reverb::graph::searchDelaySets({});
    const auto ranking = reverb::render::rankRenderedDelaySets(search, 4);
    REQUIRE(ranking.renderedCandidateCount == 4);
    REQUIRE(ranking.ranked.size() == 4);
    REQUIRE(std::ranges::all_of(ranking.ranked, [](const auto& candidate) {
        return candidate.metrics.finite && std::isfinite(candidate.metrics.maximumPeak)
            && std::isfinite(candidate.metrics.lateEchoDensity)
            && std::isfinite(candidate.metrics.lateRecurrence)
            && std::isfinite(candidate.metrics.colorationPenalty)
            && std::isfinite(candidate.metrics.decayRelativeError)
            && std::isfinite(candidate.metrics.absoluteStereoCorrelation);
    }));
    REQUIRE(std::ranges::any_of(ranking.ranked, [](const auto& candidate) {
        return candidate.passes.all();
    }));
    const auto json = reverb::render::writeDelaySetResponseRankingJson(ranking);
    for (const auto dimension : { "stable", "density", "recurrence", "coloration", "decay", "stereo" })
        REQUIRE(json.find(dimension) != std::string::npos);
}

TEST_CASE("Rendered delay-set ranking cannot admit an unstable pre-render candidate")
{
    auto search = reverb::graph::searchDelaySets({});
    search.ranked.resize(1);
    search.ranked.front().delayMilliseconds[0] = std::numeric_limits<double>::quiet_NaN();
    const auto ranking = reverb::render::rankRenderedDelaySets(search, 1);
    REQUIRE(ranking.ranked.size() == 1);
    REQUIRE_FALSE(ranking.ranked.front().passes.stable);
    REQUIRE_FALSE(ranking.ranked.front().passes.all());
    REQUIRE_FALSE(ranking.ranked.front().metrics.finite);
}

TEST_CASE("Listening fixture normalization is deterministic and bounded")
{
    const auto search = reverb::graph::searchDelaySets({});
    auto first = reverb::render::renderTuningFixture(
        search.ranked.front(), reverb::render::TuningFixture::percussion);
    auto second = first;
    reverb::render::normalizeListeningFixture(first);
    reverb::render::normalizeListeningFixture(second);
    REQUIRE(first.left == second.left);
    REQUIRE(first.right == second.right);
    double peak {};
    for (const auto value : first.left) peak = std::max(peak, std::abs(static_cast<double>(value)));
    for (const auto value : first.right) peak = std::max(peak, std::abs(static_cast<double>(value)));
    REQUIRE(peak <= 0.500001);
    REQUIRE(peak >= 0.499);
}
