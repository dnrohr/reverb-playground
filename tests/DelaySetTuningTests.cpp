#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/DelaySetTuning.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>

TEST_CASE("Delay-set search is deterministic ranked and fully reproducible")
{
    const reverb::graph::DelaySetSearchConfig config {};
    const auto first = reverb::graph::searchDelaySets(config);
    const auto second = reverb::graph::searchDelaySets(config);
    REQUIRE(reverb::graph::writeDelaySetSearchJson(first)
        == reverb::graph::writeDelaySetSearchJson(second));
    std::ifstream artifact(std::string(REVERB_MEASUREMENTS_DIR)
        + "/m22-delay-set-candidates.json", std::ios::binary);
    REQUIRE(artifact.good());
    std::ostringstream bytes;
    bytes << artifact.rdbuf();
    REQUIRE(bytes.str() == reverb::graph::writeDelaySetSearchJson(first));
    REQUIRE(first.attempted == config.attempts);
    REQUIRE(first.ranked.size() == config.resultLimit);
    REQUIRE(std::ranges::is_sorted(first.ranked, {}, [](const auto& candidate) {
        return candidate.score.totalPenalty;
    }));
    for (const auto& candidate : first.ranked) {
        REQUIRE(candidate.requiredDelayMemoryBytes <= config.delayMemoryBudgetBytes);
        REQUIRE(candidate.delayMilliseconds.front() >= config.minimumMilliseconds);
        REQUIRE(candidate.delayMilliseconds.back() + config.modulationMarginMilliseconds
            <= config.maximumMilliseconds);
        REQUIRE(std::ranges::all_of(candidate.rt60Gains, [](const auto gain) {
            return std::isfinite(gain) && gain > 0.0 && gain < 1.0;
        }));
    }
}

TEST_CASE("Delay-set scoring exposes common factors repeated differences and near periods separately")
{
    constexpr double rate = 1'000.0;
    const auto irregular = reverb::graph::scoreDelaySet({ 53.0, 67.0, 79.0, 97.0 }, rate);
    const auto commonFactors = reverb::graph::scoreDelaySet({ 50.0, 60.0, 70.0, 80.0 }, rate);
    const auto repeatedDifferences = reverb::graph::scoreDelaySet({ 41.0, 59.0, 77.0, 95.0 }, rate);
    const auto nearPeriods = reverb::graph::scoreDelaySet({ 30.0, 60.0, 90.0, 120.0 }, rate);
    REQUIRE(commonFactors.commonFactorPenalty > irregular.commonFactorPenalty);
    REQUIRE(repeatedDifferences.repeatedDifferencePenalty > irregular.repeatedDifferencePenalty);
    REQUIRE(nearPeriods.nearPeriodPenalty > irregular.nearPeriodPenalty);
}

TEST_CASE("Delay-set search rejects invalid and over-budget candidates before rendering")
{
    auto invalid = reverb::graph::DelaySetSearchConfig {};
    invalid.maximumMilliseconds = invalid.minimumMilliseconds;
    const auto invalidReport = reverb::graph::searchDelaySets(invalid);
    REQUIRE(invalidReport.ranked.empty());
    REQUIRE(invalidReport.rejections.invalidConfiguration == 1);

    auto overBudget = reverb::graph::DelaySetSearchConfig {};
    overBudget.delayMemoryBudgetBytes = 1;
    const auto budgetReport = reverb::graph::searchDelaySets(overBudget);
    REQUIRE(budgetReport.ranked.empty());
    REQUIRE(budgetReport.rejections.memoryBudget > 0);
    REQUIRE(budgetReport.rejections.memoryBudget + budgetReport.rejections.insufficientSpacing
        + budgetReport.rejections.duplicateOrUnordered == budgetReport.attempted);
}
