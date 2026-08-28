#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace reverb::graph {

struct DelaySetSearchConfig final {
    double minimumMilliseconds { 35.0 };
    double maximumMilliseconds { 120.0 };
    double minimumSpacingMilliseconds { 4.0 };
    double modulationMarginMilliseconds { 2.0 };
    double sampleRate { 96'000.0 };
    double rt60Seconds { 2.4 };
    std::size_t delayMemoryBudgetBytes { 4U * 1024U * 1024U };
    std::size_t attempts { 4'096 };
    std::size_t resultLimit { 32 };
    std::uint64_t seed { 0x4b41525246444eULL };
};

struct DelaySetScore final {
    double commonFactorPenalty {};
    double repeatedDifferencePenalty {};
    double nearPeriodPenalty {};
    double totalPenalty {};
};

struct DelaySetCandidate final {
    std::array<double, 4> delayMilliseconds {};
    std::array<double, 4> rt60Gains {};
    DelaySetScore score;
    std::size_t requiredDelayMemoryBytes {};
};

struct DelaySetRejections final {
    std::size_t invalidConfiguration {};
    std::size_t duplicateOrUnordered {};
    std::size_t insufficientSpacing {};
    std::size_t modulationMargin {};
    std::size_t memoryBudget {};
};

struct DelaySetSearchReport final {
    DelaySetSearchConfig config;
    DelaySetRejections rejections;
    std::size_t attempted {};
    std::vector<DelaySetCandidate> ranked;
};

[[nodiscard]] DelaySetScore scoreDelaySet(
    const std::array<double, 4>& milliseconds, double sampleRate) noexcept;
[[nodiscard]] DelaySetSearchReport searchDelaySets(const DelaySetSearchConfig& config);
[[nodiscard]] std::string writeDelaySetSearchJson(const DelaySetSearchReport& report);

} // namespace reverb::graph
