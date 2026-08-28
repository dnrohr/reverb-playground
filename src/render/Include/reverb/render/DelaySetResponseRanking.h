#pragma once

#include <reverb/graph/DelaySetTuning.h>
#include <reverb/render/OfflineRenderer.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace reverb::render {

enum class TuningFixture : std::size_t { impulse, noiseBurst, percussion, tonal, count };

struct RenderedDelaySetMetrics final {
    double lateEchoDensity {};
    double lateRecurrence {};
    double colorationPenalty {};
    double decayRelativeError {};
    double absoluteStereoCorrelation {};
    double maximumPeak {};
    bool finite {};
};

struct RenderedDelaySetPasses final {
    bool stable {};
    bool density {};
    bool recurrence {};
    bool coloration {};
    bool decay {};
    bool stereo {};
    [[nodiscard]] bool all() const noexcept;
};

struct RankedRenderedDelaySet final {
    reverb::graph::DelaySetCandidate candidate;
    RenderedDelaySetMetrics metrics;
    RenderedDelaySetPasses passes;
    double aggregateScore {};
    std::size_t sourceRank {};
};

struct DelaySetResponseRanking final {
    reverb::graph::DelaySetSearchConfig sourceConfig;
    double sampleRate { 48'000.0 };
    double targetRt60Seconds { 2.4 };
    std::size_t renderedCandidateCount {};
    std::vector<RankedRenderedDelaySet> ranked;
};

[[nodiscard]] RenderResult renderTuningFixture(const reverb::graph::DelaySetCandidate& candidate,
    TuningFixture fixture, double sampleRate = 48'000.0, double rt60Seconds = 2.4);
[[nodiscard]] DelaySetResponseRanking rankRenderedDelaySets(
    const reverb::graph::DelaySetSearchReport& search, std::size_t candidateLimit = 16,
    double sampleRate = 48'000.0, double rt60Seconds = 2.4);
[[nodiscard]] std::string writeDelaySetResponseRankingJson(const DelaySetResponseRanking& ranking);
void normalizeListeningFixture(RenderResult& audio, double targetPeak = 0.5) noexcept;
[[nodiscard]] std::string tuningFixtureName(TuningFixture fixture);

} // namespace reverb::render
