#include <reverb/graph/DelaySetTuning.h>
#include <reverb/graph/FourLineFdnGraph.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <tuple>

namespace reverb::graph {
namespace {

std::uint64_t nextRandom(std::uint64_t& state) noexcept
{
    state ^= state >> 12U;
    state ^= state << 25U;
    state ^= state >> 27U;
    return state * 2'685'821'657'736'338'717ULL;
}

bool finiteConfig(const DelaySetSearchConfig& config) noexcept
{
    return std::isfinite(config.minimumMilliseconds) && config.minimumMilliseconds > 0.0
        && std::isfinite(config.maximumMilliseconds)
        && config.maximumMilliseconds > config.minimumMilliseconds
        && std::isfinite(config.minimumSpacingMilliseconds) && config.minimumSpacingMilliseconds > 0.0
        && std::isfinite(config.modulationMarginMilliseconds) && config.modulationMarginMilliseconds >= 0.0
        && std::isfinite(config.sampleRate) && config.sampleRate >= 8'000.0 && config.sampleRate <= 384'000.0
        && std::isfinite(config.rt60Seconds) && config.rt60Seconds >= 0.1 && config.rt60Seconds <= 30.0
        && config.attempts > 0 && config.resultLimit > 0;
}

std::size_t delayBytes(const std::array<double, 4>& values,
    const DelaySetSearchConfig& config) noexcept
{
    std::size_t samples {};
    for (const auto value : values)
        samples += static_cast<std::size_t>(std::ceil(
            (value + config.modulationMarginMilliseconds) * config.sampleRate / 1'000.0)) + 4U;
    return samples * sizeof(float);
}

} // namespace

DelaySetScore scoreDelaySet(const std::array<double, 4>& milliseconds,
    const double sampleRate) noexcept
{
    DelaySetScore score;
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0
        || !std::ranges::all_of(milliseconds, [](const auto value) {
            return std::isfinite(value) && value > 0.0;
        })) {
        score.commonFactorPenalty = score.repeatedDifferencePenalty
            = score.nearPeriodPenalty = score.totalPenalty = 1.0;
        return score;
    }
    std::array<std::int64_t, 4> samples {};
    for (std::size_t index = 0; index < samples.size(); ++index)
        samples[index] = std::max<std::int64_t>(1,
            std::llround(milliseconds[index] * sampleRate / 1'000.0));

    double commonFactors {};
    double nearPeriods {};
    std::array<std::int64_t, 6> differences {};
    std::size_t differenceIndex {};
    for (std::size_t left = 0; left < samples.size(); ++left) {
        for (std::size_t right = left + 1; right < samples.size(); ++right) {
            const auto smaller = std::min(samples[left], samples[right]);
            const auto larger = std::max(samples[left], samples[right]);
            commonFactors += static_cast<double>(std::gcd(smaller, larger) - 1)
                / static_cast<double>(std::max<std::int64_t>(1, smaller - 1));
            const auto ratio = static_cast<double>(larger) / static_cast<double>(smaller);
            const auto distance = std::abs(ratio - std::round(ratio));
            nearPeriods += std::max(0.0, 1.0 - distance / 0.035);
            differences[differenceIndex++] = larger - smaller;
        }
    }
    score.commonFactorPenalty = commonFactors / 6.0;
    score.nearPeriodPenalty = nearPeriods / 6.0;
    double repeated {};
    for (std::size_t left = 0; left < differences.size(); ++left)
        for (std::size_t right = left + 1; right < differences.size(); ++right)
            if (std::abs(differences[left] - differences[right]) <= 1) repeated += 1.0;
    score.repeatedDifferencePenalty = repeated / 15.0;
    score.totalPenalty = 0.35 * score.commonFactorPenalty
        + 0.35 * score.repeatedDifferencePenalty + 0.30 * score.nearPeriodPenalty;
    return score;
}

DelaySetSearchReport searchDelaySets(const DelaySetSearchConfig& config)
{
    DelaySetSearchReport report;
    report.config = config;
    if (!finiteConfig(config)) {
        report.rejections.invalidConfiguration = 1;
        return report;
    }
    const auto lowerTick = static_cast<std::int64_t>(std::ceil(config.minimumMilliseconds * 10.0));
    const auto upperTick = static_cast<std::int64_t>(std::floor(
        (config.maximumMilliseconds - config.modulationMarginMilliseconds) * 10.0));
    const auto spacingTicks = static_cast<std::int64_t>(std::ceil(config.minimumSpacingMilliseconds * 10.0));
    if (upperTick - lowerTick < spacingTicks * 3) {
        report.rejections.invalidConfiguration = 1;
        return report;
    }

    std::uint64_t state = config.seed == 0 ? 0x9e3779b97f4a7c15ULL : config.seed;
    std::set<std::array<std::int64_t, 4>> unique;
    std::vector<DelaySetCandidate> accepted;
    accepted.reserve(std::min(config.attempts, config.resultLimit * 8U));
    for (std::size_t attempt = 0; attempt < config.attempts; ++attempt) {
        ++report.attempted;
        std::array<std::int64_t, 4> ticks {};
        const auto range = static_cast<std::uint64_t>(upperTick - lowerTick + 1);
        for (auto& tick : ticks)
            tick = lowerTick + static_cast<std::int64_t>(nextRandom(state) % range);
        std::ranges::sort(ticks);
        if (!unique.insert(ticks).second
            || std::adjacent_find(ticks.begin(), ticks.end()) != ticks.end()) {
            ++report.rejections.duplicateOrUnordered;
            continue;
        }
        if (std::adjacent_find(ticks.begin(), ticks.end(), [spacingTicks](const auto a, const auto b) {
            return b - a < spacingTicks;
        }) != ticks.end()) {
            ++report.rejections.insufficientSpacing;
            continue;
        }
        std::array<double, 4> values {};
        for (std::size_t index = 0; index < values.size(); ++index)
            values[index] = static_cast<double>(ticks[index]) / 10.0;
        if (values.back() + config.modulationMarginMilliseconds > config.maximumMilliseconds + 1.0e-9) {
            ++report.rejections.modulationMargin;
            continue;
        }
        const auto requiredBytes = delayBytes(values, config);
        if (requiredBytes > config.delayMemoryBudgetBytes) {
            ++report.rejections.memoryBudget;
            continue;
        }
        DelaySetCandidate candidate;
        candidate.delayMilliseconds = values;
        candidate.requiredDelayMemoryBytes = requiredBytes;
        candidate.score = scoreDelaySet(values, config.sampleRate);
        for (std::size_t index = 0; index < values.size(); ++index)
            candidate.rt60Gains[index] = fdnLineGainForRt60(values[index], config.rt60Seconds);
        accepted.push_back(candidate);
    }
    std::ranges::sort(accepted, [](const auto& left, const auto& right) {
        return std::tie(left.score.totalPenalty, left.delayMilliseconds)
            < std::tie(right.score.totalPenalty, right.delayMilliseconds);
    });
    if (accepted.size() > config.resultLimit) accepted.resize(config.resultLimit);
    report.ranked = std::move(accepted);
    return report;
}

std::string writeDelaySetSearchJson(const DelaySetSearchReport& report)
{
    const auto& config = report.config;
    nlohmann::ordered_json json {
        { "schemaVersion", 1 },
        { "algorithm", "deterministic-delay-set-search-v1" },
        { "config", {
            { "minimumMilliseconds", config.minimumMilliseconds },
            { "maximumMilliseconds", config.maximumMilliseconds },
            { "minimumSpacingMilliseconds", config.minimumSpacingMilliseconds },
            { "modulationMarginMilliseconds", config.modulationMarginMilliseconds },
            { "sampleRate", config.sampleRate }, { "rt60Seconds", config.rt60Seconds },
            { "delayMemoryBudgetBytes", config.delayMemoryBudgetBytes },
            { "attempts", config.attempts }, { "resultLimit", config.resultLimit },
            { "seed", config.seed },
        } },
        { "attempted", report.attempted },
        { "rejections", {
            { "invalidConfiguration", report.rejections.invalidConfiguration },
            { "duplicateOrUnordered", report.rejections.duplicateOrUnordered },
            { "insufficientSpacing", report.rejections.insufficientSpacing },
            { "modulationMargin", report.rejections.modulationMargin },
            { "memoryBudget", report.rejections.memoryBudget },
        } },
        { "ranked", nlohmann::ordered_json::array() },
    };
    for (const auto& candidate : report.ranked) {
        json["ranked"].push_back({
            { "delayMilliseconds", candidate.delayMilliseconds },
            { "rt60Gains", candidate.rt60Gains },
            { "requiredDelayMemoryBytes", candidate.requiredDelayMemoryBytes },
            { "score", {
                { "commonFactorPenalty", candidate.score.commonFactorPenalty },
                { "repeatedDifferencePenalty", candidate.score.repeatedDifferencePenalty },
                { "nearPeriodPenalty", candidate.score.nearPeriodPenalty },
                { "totalPenalty", candidate.score.totalPenalty },
            } },
        });
    }
    return json.dump(2) + '\n';
}

} // namespace reverb::graph
