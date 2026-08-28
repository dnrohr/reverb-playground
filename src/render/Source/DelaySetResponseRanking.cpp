#include <reverb/render/DelaySetResponseRanking.h>

#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/FourLineFdnGraph.h>
#include <reverb/render/DensityMeasurements.h>
#include <reverb/render/ResponseMeasurements.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <ranges>
#include <span>
#include <stdexcept>

namespace reverb::render {
namespace {

constexpr std::size_t blockSize = 128;

reverb::graph::GraphDocument graphFor(const reverb::graph::DelaySetCandidate& candidate,
    const double rt60Seconds)
{
    reverb::graph::FourLineFdnControls controls;
    controls.rt60Seconds = rt60Seconds;
    controls.delayMilliseconds = candidate.delayMilliseconds;
    return reverb::graph::makeFourLineFdnGraph(controls);
}

std::pair<std::vector<float>, std::vector<float>> fixtureInput(
    const TuningFixture fixture, const double sampleRate, const std::size_t frames)
{
    std::vector<float> left(frames), right(frames);
    std::uint32_t random = 0x22fd4a71U;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const auto seconds = static_cast<double>(frame) / sampleRate;
        switch (fixture) {
        case TuningFixture::impulse:
            if (frame == 0) left[frame] = right[frame] = 0.1F;
            break;
        case TuningFixture::noiseBurst:
            if (seconds < 0.12) {
                random = random * 1'664'525U + 1'013'904'223U;
                left[frame] = right[frame] = static_cast<float>(
                    ((random >> 8U) / 16'777'215.0 * 2.0 - 1.0) * 0.08);
            }
            break;
        case TuningFixture::percussion: {
            constexpr std::array<double, 5> hits { 0.0, 0.19, 0.43, 0.71, 0.96 };
            for (const auto hit : hits) {
                const auto offset = seconds - hit;
                if (offset >= 0.0 && offset < 0.035) {
                    random = random * 1'664'525U + 1'013'904'223U;
                    const auto noise = (random >> 8U) / 16'777'215.0 * 2.0 - 1.0;
                    left[frame] += static_cast<float>(0.09 * noise * std::exp(-offset * 120.0));
                    right[frame] = left[frame];
                }
            }
            break;
        }
        case TuningFixture::tonal:
            if (seconds < 0.8) {
                const auto fade = std::min(1.0, std::min(seconds * 40.0, (0.8 - seconds) * 40.0));
                const auto value = 0.035 * fade * (std::sin(2.0 * std::numbers::pi * 220.0 * seconds)
                    + std::sin(2.0 * std::numbers::pi * 329.63 * seconds));
                left[frame] = right[frame] = static_cast<float>(value);
            }
            break;
        case TuningFixture::count: break;
        }
    }
    return { std::move(left), std::move(right) };
}

double peak(const RenderResult& audio) noexcept
{
    double result {};
    for (const auto channel : { std::span(audio.left), std::span(audio.right) })
        for (const auto value : channel) result = std::max(result, std::abs(static_cast<double>(value)));
    return result;
}

bool finite(const RenderResult& audio) noexcept
{
    return std::ranges::all_of(audio.left, [](const auto value) { return std::isfinite(value); })
        && std::ranges::all_of(audio.right, [](const auto value) { return std::isfinite(value); });
}

} // namespace

bool RenderedDelaySetPasses::all() const noexcept
{
    return stable && density && recurrence && coloration && decay && stereo;
}

RenderResult renderTuningFixture(const reverb::graph::DelaySetCandidate& candidate,
    const TuningFixture fixture, const double sampleRate, const double rt60Seconds)
{
    const auto frames = static_cast<std::size_t>(sampleRate * (fixture == TuningFixture::impulse ? 5.0 : 3.0));
    auto compiled = reverb::graph::compileFeedbackGraph(graphFor(candidate, rt60Seconds), sampleRate, blockSize);
    if (!compiled.valid()) throw std::runtime_error("candidate graph did not compile");
    auto [inputLeft, inputRight] = fixtureInput(fixture, sampleRate, frames);
    RenderResult output { std::vector<float>(frames), std::vector<float>(frames) };
    for (std::size_t offset = 0; offset < frames; offset += blockSize) {
        const auto count = std::min(blockSize, frames - offset);
        compiled.runtime->process(std::span(inputLeft).subspan(offset, count),
            std::span(inputRight).subspan(offset, count), std::span(output.left).subspan(offset, count),
            std::span(output.right).subspan(offset, count));
    }
    return output;
}

DelaySetResponseRanking rankRenderedDelaySets(const reverb::graph::DelaySetSearchReport& search,
    const std::size_t candidateLimit, const double sampleRate, const double rt60Seconds)
{
    DelaySetResponseRanking result { search.config, sampleRate, rt60Seconds };
    const auto count = std::min(candidateLimit, search.ranked.size());
    result.renderedCandidateCount = count;
    result.ranked.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        RankedRenderedDelaySet ranked;
        ranked.candidate = search.ranked[index];
        ranked.sourceRank = index + 1;
        const auto arithmeticallyEligible = ranked.candidate.requiredDelayMemoryBytes
                <= search.config.delayMemoryBudgetBytes
            && std::ranges::all_of(ranked.candidate.delayMilliseconds, [&](const auto value) {
                return std::isfinite(value) && value >= search.config.minimumMilliseconds
                    && value + search.config.modulationMarginMilliseconds
                        <= search.config.maximumMilliseconds;
            });
        if (!arithmeticallyEligible) {
            ranked.metrics.finite = false;
            ranked.metrics.maximumPeak = std::numeric_limits<double>::infinity();
            ranked.passes = {};
            ranked.aggregateScore = std::numeric_limits<double>::infinity();
            result.ranked.push_back(ranked);
            continue;
        }
        try {
            const auto impulse = renderTuningFixture(ranked.candidate, TuningFixture::impulse, sampleRate, rt60Seconds);
            const auto noise = renderTuningFixture(ranked.candidate, TuningFixture::noiseBurst, sampleRate, rt60Seconds);
            const auto percussion = renderTuningFixture(ranked.candidate, TuningFixture::percussion, sampleRate, rt60Seconds);
            const auto tonal = renderTuningFixture(ranked.candidate, TuningFixture::tonal, sampleRate, rt60Seconds);
            ranked.metrics.finite = finite(impulse) && finite(noise) && finite(percussion) && finite(tonal);
            ranked.metrics.maximumPeak = std::max({ peak(impulse), peak(noise), peak(percussion), peak(tonal) });
            const auto density = measureDensity(impulse.left, impulse.right, sampleRate);
            const auto& late = density.regions.back();
            ranked.metrics.lateEchoDensity = late.echoDensity;
            ranked.metrics.lateRecurrence = late.recurrence;
            ranked.metrics.colorationPenalty = 1.0 - late.spectralFlatness;
            ranked.metrics.absoluteStereoCorrelation = std::abs(late.stereoCorrelation);
            const auto response = measureResponse(impulse.left, impulse.right, sampleRate);
            ranked.metrics.decayRelativeError = response.rt60Seconds
                ? std::abs(*response.rt60Seconds - rt60Seconds) / rt60Seconds : 1.0;
        } catch (...) {
            ranked.metrics.finite = false;
            ranked.metrics.maximumPeak = std::numeric_limits<double>::infinity();
        }
        ranked.passes = {
            ranked.metrics.finite && ranked.metrics.maximumPeak < 1.0,
            ranked.metrics.lateEchoDensity >= 0.80,
            ranked.metrics.lateRecurrence <= 0.80,
            ranked.metrics.colorationPenalty <= 0.90,
            ranked.metrics.decayRelativeError <= 0.75,
            ranked.metrics.absoluteStereoCorrelation <= 0.98,
        };
        ranked.aggregateScore = ((1.0 - ranked.metrics.lateEchoDensity)
            + ranked.metrics.lateRecurrence + ranked.metrics.colorationPenalty
            + ranked.metrics.decayRelativeError + ranked.metrics.absoluteStereoCorrelation) / 5.0;
        result.ranked.push_back(ranked);
    }
    std::ranges::sort(result.ranked, [](const auto& left, const auto& right) {
        if (left.passes.all() != right.passes.all()) return left.passes.all() > right.passes.all();
        if (left.aggregateScore != right.aggregateScore) return left.aggregateScore < right.aggregateScore;
        return left.sourceRank < right.sourceRank;
    });
    return result;
}

std::string writeDelaySetResponseRankingJson(const DelaySetResponseRanking& ranking)
{
    nlohmann::ordered_json json {
        { "schemaVersion", 1 }, { "algorithm", "rendered-delay-set-ranking-v1" },
        { "sourceSearch", { { "algorithm", "deterministic-delay-set-search-v1" },
            { "seed", ranking.sourceConfig.seed }, { "attempts", ranking.sourceConfig.attempts },
            { "minimumMilliseconds", ranking.sourceConfig.minimumMilliseconds },
            { "maximumMilliseconds", ranking.sourceConfig.maximumMilliseconds },
            { "minimumSpacingMilliseconds", ranking.sourceConfig.minimumSpacingMilliseconds },
            { "modulationMarginMilliseconds", ranking.sourceConfig.modulationMarginMilliseconds },
            { "planningSampleRate", ranking.sourceConfig.sampleRate },
            { "delayMemoryBudgetBytes", ranking.sourceConfig.delayMemoryBudgetBytes } } },
        { "sampleRate", ranking.sampleRate }, { "targetRt60Seconds", ranking.targetRt60Seconds },
        { "fixtures", { "impulse", "noise-burst", "percussion", "tonal" } },
        { "fixtureDefinitions", {
            { "impulse", "0.1 peak dual-mono impulse; 5 seconds" },
            { "noise-burst", "fixed-seed 0.08 peak dual-mono noise for 120 ms; 3 seconds" },
            { "percussion", "fixed-seed 35 ms exponential noise hits at 0, 190, 430, 710, and 960 ms; 3 seconds" },
            { "tonal", "220 Hz plus 329.63 Hz dual-mono tone for 800 ms with 25 ms fades; 3 seconds" },
        } },
        { "eligibilityThresholds", {
            { "maximumPeakExclusive", 1.0 }, { "minimumLateEchoDensity", 0.80 },
            { "maximumLateRecurrence", 0.80 }, { "maximumColorationPenalty", 0.90 },
            { "maximumDecayRelativeError", 0.75 },
            { "maximumAbsoluteStereoCorrelation", 0.98 },
        } },
        { "normalization", "listening WAV peak normalized to 0.5 (-6.02 dBFS); scores use unnormalized audio" },
        { "renderedCandidateCount", ranking.renderedCandidateCount },
        { "ranked", nlohmann::ordered_json::array() },
    };
    for (const auto& ranked : ranking.ranked) {
        json["ranked"].push_back({
            { "sourceArithmeticRank", ranked.sourceRank },
            { "eligible", ranked.passes.all() },
            { "delayMilliseconds", ranked.candidate.delayMilliseconds },
            { "aggregateScore", ranked.aggregateScore },
            { "metrics", {
                { "lateEchoDensity", ranked.metrics.lateEchoDensity },
                { "lateRecurrence", ranked.metrics.lateRecurrence },
                { "colorationPenalty", ranked.metrics.colorationPenalty },
                { "decayRelativeError", ranked.metrics.decayRelativeError },
                { "absoluteStereoCorrelation", ranked.metrics.absoluteStereoCorrelation },
                { "maximumPeak", ranked.metrics.maximumPeak }, { "finite", ranked.metrics.finite },
            } },
            { "passes", {
                { "stable", ranked.passes.stable }, { "density", ranked.passes.density },
                { "recurrence", ranked.passes.recurrence }, { "coloration", ranked.passes.coloration },
                { "decay", ranked.passes.decay }, { "stereo", ranked.passes.stereo },
            } },
        });
    }
    return json.dump(2) + '\n';
}

void normalizeListeningFixture(RenderResult& audio, const double targetPeak) noexcept
{
    const auto found = peak(audio);
    if (!std::isfinite(found) || found <= 0.0) return;
    const auto gain = static_cast<float>(std::clamp(targetPeak, 0.0, 1.0) / found);
    for (auto& value : audio.left) value *= gain;
    for (auto& value : audio.right) value *= gain;
}

std::string tuningFixtureName(const TuningFixture fixture)
{
    switch (fixture) {
    case TuningFixture::impulse: return "impulse";
    case TuningFixture::noiseBurst: return "noise-burst";
    case TuningFixture::percussion: return "percussion";
    case TuningFixture::tonal: return "tonal";
    case TuningFixture::count: break;
    }
    return "unknown";
}

} // namespace reverb::render
