#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/PitchShiftContract.h>
#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/graph/ReverseCosmicShimmerGraph.h>
#include <reverb/render/ReverseCosmicShimmerValidation.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <random>
#include <ranges>
#include <numbers>
#include <set>
#include <span>
#include <string_view>
#include <vector>

namespace {

using reverb::graph::GraphDocument;

const reverb::graph::Node& node(const GraphDocument& graph, const std::string_view id)
{
    const auto found = std::ranges::find(graph.nodes, id, &reverb::graph::Node::id);
    REQUIRE(found != graph.nodes.end());
    return *found;
}

double parameter(const GraphDocument& graph, const std::string_view nodeId, const std::string_view parameterId)
{
    const auto& foundNode = node(graph, nodeId);
    const auto found = std::ranges::find(foundNode.parameters, parameterId, &reverb::graph::Parameter::id);
    REQUIRE(found != foundNode.parameters.end());
    return found->value;
}

bool hasCable(const GraphDocument& graph, const std::string_view from, const std::string_view to)
{
    return std::ranges::any_of(graph.connections, [&](const auto& connection) {
        return connection.from.nodeId == from && connection.to.nodeId == to;
    });
}

struct RenderMetrics final {
    double earlyEnergy {};
    double lateEnergy {};
    double correlation {};
    double monoCompatibility {};
    float peak {};
    bool finite { true };
};

RenderMetrics renderMetrics(const GraphDocument& graph, const double sampleRate)
{
    constexpr std::size_t blockSize = 128;
    auto compiled = reverb::graph::compileFeedbackGraph(graph, sampleRate, blockSize);
    CAPTURE(compiled.errors, compiled.offendingLoops);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.delayMemory.withinBudget());
    const auto frames = static_cast<std::size_t>(std::ceil(sampleRate * 3.0));
    std::vector<float> left(frames), right(frames);
    std::array<float, blockSize> input {}, silence {}, outputLeft {}, outputRight {};
    std::mt19937 generator(0x52435653U);
    std::uniform_real_distribution<float> noise(-0.012F, 0.012F);
    for (std::size_t offset = 0; offset < frames; offset += blockSize) {
        const auto count = std::min(blockSize, frames - offset);
        input.fill(0.0F);
        if (offset < static_cast<std::size_t>(sampleRate * 0.04)) {
            for (std::size_t frame = 0; frame < count; ++frame) input[frame] = noise(generator);
        }
        if (offset == 0) input[0] += 0.1F;
        compiled.runtime->process(std::span(input).first(count), std::span(silence).first(count),
            std::span(outputLeft).first(count), std::span(outputRight).first(count));
        std::ranges::copy(std::span(outputLeft).first(count), left.begin() + static_cast<std::ptrdiff_t>(offset));
        std::ranges::copy(std::span(outputRight).first(count), right.begin() + static_cast<std::ptrdiff_t>(offset));
    }

    auto metrics = RenderMetrics {};
    auto cross = 0.0;
    auto leftEnergy = 0.0;
    auto rightEnergy = 0.0;
    auto monoEnergy = 0.0;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        metrics.finite = metrics.finite
            && std::isfinite(left[frame]) && std::isfinite(right[frame]);
        metrics.peak = std::max({ metrics.peak, std::abs(left[frame]), std::abs(right[frame]) });
        const auto energy = static_cast<double>(left[frame]) * left[frame]
            + static_cast<double>(right[frame]) * right[frame];
        const auto time = static_cast<double>(frame) / sampleRate;
        if (time >= 0.20 && time < 0.45) metrics.earlyEnergy += energy;
        if (time >= 0.65 && time < 1.10) metrics.lateEnergy += energy;
        if (time >= 0.65) {
            cross += static_cast<double>(left[frame]) * right[frame];
            leftEnergy += static_cast<double>(left[frame]) * left[frame];
            rightEnergy += static_cast<double>(right[frame]) * right[frame];
            const auto mono = 0.5 * (static_cast<double>(left[frame]) + right[frame]);
            monoEnergy += mono * mono;
        }
    }
    metrics.correlation = cross / std::sqrt(leftEnergy * rightEnergy);
    metrics.monoCompatibility = 2.0 * monoEnergy / (leftEnergy + rightEnergy);
    return metrics;
}

double toneAmplitude(const std::span<const float> samples, const double sampleRate, const double frequency)
{
    auto real = 0.0;
    auto imaginary = 0.0;
    auto windowSum = 0.0;
    for (std::size_t frame = 0; frame < samples.size(); ++frame) {
        const auto window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi
            * static_cast<double>(frame) / static_cast<double>(samples.size() - 1));
        const auto phase = 2.0 * std::numbers::pi * frequency * static_cast<double>(frame) / sampleRate;
        real += samples[frame] * window * std::cos(phase);
        imaginary -= samples[frame] * window * std::sin(phase);
        windowSum += window;
    }
    return windowSum > 0.0 ? 2.0 * std::hypot(real, imaginary) / windowSum : 0.0;
}

std::vector<float> renderToneBurst(const GraphDocument& graph, const double sampleRate)
{
    constexpr std::size_t blockSize = 128;
    auto compiled = reverb::graph::compileFeedbackGraph(graph, sampleRate, blockSize);
    REQUIRE(compiled.valid());
    const auto frames = static_cast<std::size_t>(std::ceil(sampleRate * 2.6));
    std::vector<float> mono(frames);
    std::array<float, blockSize> input {}, silence {}, left {}, right {};
    for (std::size_t offset = 0; offset < frames; offset += blockSize) {
        const auto count = std::min(blockSize, frames - offset);
        input.fill(0.0F);
        for (std::size_t frame = 0; frame < count; ++frame) {
            const auto absolute = offset + frame;
            if (absolute < static_cast<std::size_t>(sampleRate * 0.08))
                input[frame] = static_cast<float>(0.05 * std::sin(
                    2.0 * std::numbers::pi * 400.0 * static_cast<double>(absolute) / sampleRate));
        }
        compiled.runtime->process(std::span(input).first(count), std::span(silence).first(count),
            std::span(left).first(count), std::span(right).first(count));
        for (std::size_t frame = 0; frame < count; ++frame)
            mono[offset + frame] = 0.5F * (left[frame] + right[frame]);
    }
    return mono;
}

} // namespace

TEST_CASE("Reverse Cosmic Shimmer is a visible causal dual reverse-grain construction")
{
    const auto graph = reverb::graph::makeReverseCosmicShimmerGraph();
    const std::set<std::string> publicTypes {
        "stereo-input", "stereo-output", "gain", "sum", "delay", "allpass",
        "lowpass", "pitch-shift", "lfo",
    };
    const auto validation = reverb::graph::validate(graph);
    CAPTURE(validation.errors);
    REQUIRE(validation.valid());
    REQUIRE(graph.nodes.size() == 45);
    REQUIRE(graph.connections.size() == 57);
    REQUIRE(graph.layout.nodes.size() == graph.nodes.size());
    REQUIRE(std::ranges::all_of(graph.nodes, [&](const auto& item) {
        return publicTypes.contains(item.type);
    }));
    REQUIRE(std::ranges::count(graph.nodes, std::string("pitch-shift"), &reverb::graph::Node::type) == 2);
    REQUIRE(parameter(graph, "reverse-pitch-left", "direction") == Catch::Approx(1.0));
    REQUIRE(parameter(graph, "reverse-pitch-right", "direction") == Catch::Approx(1.0));
    REQUIRE(parameter(graph, "reverse-pitch-left", "phase") == Catch::Approx(0.0));
    REQUIRE(parameter(graph, "reverse-pitch-right", "phase") == Catch::Approx(0.373));
    REQUIRE(hasCable(graph, "motion-left", "tank-diffusion-a"));
    REQUIRE(hasCable(graph, "motion-right", "tank-diffusion-b"));
    const auto compiled = reverb::graph::compileFeedbackGraph(graph, 48'000.0, 256);
    CAPTURE(compiled.errors, compiled.offendingLoops);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.feedbackComponents.size() == 1);
    for (const auto id : { "tank-delay", "normal-return-delay", "shimmer-return-left",
             "shimmer-return-right", "reverse-pitch-left", "reverse-pitch-right" }) {
        REQUIRE(std::ranges::find(compiled.feedbackComponents.front(), id)
            != compiled.feedbackComponents.front().end());
    }
    REQUIRE(reverb::graph::parsePatchJson(reverb::graph::writePatchJson(graph)) == graph);
}

TEST_CASE("Reverse Cosmic Shimmer keeps every return delayed dark and independently bounded")
{
    const auto graph = reverb::graph::makeReverseCosmicShimmerGraph({
        .riseScale = 1.2, .sizeMilliseconds = 220.0, .normalFeedback = 0.47,
        .shimmerFeedback = 0.11, .dampingHertz = 3'900.0,
        .modulationDepthMilliseconds = 1.5, .wetLevel = 0.67,
    });
    REQUIRE(parameter(graph, "rise-delay-late", "delay") == Catch::Approx(624.0));
    REQUIRE(parameter(graph, "tank-delay", "delay") == Catch::Approx(220.0));
    REQUIRE(parameter(graph, "normal-feedback", "gain") == Catch::Approx(0.47));
    REQUIRE(parameter(graph, "shimmer-feedback-left", "gain") == Catch::Approx(0.055));
    REQUIRE(parameter(graph, "shimmer-feedback-right", "gain") == Catch::Approx(0.055));
    REQUIRE(parameter(graph, "tank-damping", "cutoff") == Catch::Approx(3'900.0));
    REQUIRE(parameter(graph, "shift-damping-left", "cutoff") == Catch::Approx(3'198.0));
    REQUIRE(parameter(graph, "left-extraction", "delay") == Catch::Approx(17.3));
    REQUIRE(node(graph, "left-extraction").parameters.front().modulation->amount == Catch::Approx(1.5));
    REQUIRE(hasCable(graph, "normal-feedback", "normal-return-delay"));
    REQUIRE(hasCable(graph, "shimmer-feedback-left", "shimmer-return-left"));
    REQUIRE(hasCable(graph, "shimmer-feedback-right", "shimmer-return-right"));
    REQUIRE(hasCable(graph, "tank-damping", "normal-feedback"));
    REQUIRE(hasCable(graph, "reverse-pitch-left", "shift-damping-left"));
    REQUIRE(hasCable(graph, "reverse-pitch-right", "shift-damping-right"));

    const auto bounded = reverb::graph::makeReverseCosmicShimmerGraph({
        .normalFeedback = 5.0, .shimmerFeedback = 5.0,
    });
    const auto combined = parameter(bounded, "normal-feedback", "gain")
        + parameter(bounded, "shimmer-feedback-left", "gain")
        + parameter(bounded, "shimmer-feedback-right", "gain");
    REQUIRE(combined == Catch::Approx(reverb::graph::reverseCosmicMaximumCombinedFeedback));
}

TEST_CASE("Reverse Cosmic Shimmer rises late and retains bounded compatible stereo at qualified rates")
{
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0 }) {
        const auto metrics = renderMetrics(reverb::graph::makeReverseCosmicShimmerGraph(), sampleRate);
        CAPTURE(sampleRate, metrics.earlyEnergy, metrics.lateEnergy, metrics.correlation,
            metrics.monoCompatibility, metrics.peak);
        REQUIRE(metrics.lateEnergy > metrics.earlyEnergy * 1.5);
        REQUIRE(metrics.finite);
        REQUIRE(std::abs(metrics.correlation) < 0.98);
        REQUIRE(metrics.monoCompatibility > 0.20);
        REQUIRE(metrics.monoCompatibility < 1.8);
        REQUIRE(metrics.peak > 0.0F);
        REQUIRE(metrics.peak < 1.0F);

        const auto extreme = renderMetrics(reverb::graph::makeReverseCosmicShimmerGraph({
            .riseScale = 1.35, .sizeMilliseconds = 260.0,
            .normalFeedback = 5.0, .shimmerFeedback = 5.0,
            .dampingHertz = 1'800.0, .modulationDepthMilliseconds = 1.8,
            .wetLevel = 0.70,
        }), sampleRate);
        CAPTURE(extreme.peak, extreme.correlation, extreme.monoCompatibility);
        REQUIRE(extreme.finite);
        REQUIRE(extreme.peak > 0.0F);
        REQUIRE(extreme.peak < 1.0F);
    }
}

TEST_CASE("Reverse Cosmic Shimmer delays and sustains octave evolution")
{
    constexpr auto sampleRate = 48'000.0;
    const auto rendered = renderToneBurst(reverb::graph::makeReverseCosmicShimmerGraph(), sampleRate);
    const auto window = static_cast<std::size_t>(0.30 * sampleRate);
    const auto early = toneAmplitude(std::span(rendered).subspan(
        static_cast<std::size_t>(0.30 * sampleRate), window), sampleRate, 800.0);
    const auto late = toneAmplitude(std::span(rendered).subspan(
        static_cast<std::size_t>(1.00 * sampleRate), window), sampleRate, 800.0);
    const auto sustained = toneAmplitude(std::span(rendered).subspan(
        static_cast<std::size_t>(1.65 * sampleRate), window), sampleRate, 800.0);
    CAPTURE(early, late, sustained);
    REQUIRE(late > early * 4.0);
    REQUIRE(sustained > early * 1.5);
}

TEST_CASE("Reverse Cosmic Shimmer checked fixtures prove the publish contract")
{
    const auto report = reverb::render::measureReverseCosmicShimmerValidation();
    REQUIRE(report.rates.size() == reverb::dsp::pitch_shift::qualificationSampleRates.size());
    for (std::size_t index = 0; index < report.rates.size(); ++index) {
        const auto& rate = report.rates[index];
        CAPTURE(rate.sampleRate, rate.onsetMilliseconds, rate.peakTimeMilliseconds,
            rate.finalToMidDecayDb, rate.octaveGrowthDb, rate.stereoCorrelation,
            rate.monoCompatibility, rate.impulsePeak, rate.chordPeak, rate.noisePeak);
        REQUIRE(rate.sampleRate == reverb::dsp::pitch_shift::qualificationSampleRates[index]);
        REQUIRE(rate.causal);
        REQUIRE(rate.onsetMilliseconds >= 250.0);
        REQUIRE(rate.onsetMilliseconds <= 275.0);
        REQUIRE(rate.peakTimeMilliseconds > rate.onsetMilliseconds + 300.0);
        REQUIRE(rate.lateImpulseEnergy > rate.earlyImpulseEnergy * 3.5);
        REQUIRE(rate.finalToMidDecayDb < -30.0);
        REQUIRE(rate.octaveGrowthDb > 20.0);
        REQUIRE(rate.lateChordOctaveDbfs > -90.0);
        REQUIRE(std::abs(rate.stereoCorrelation) < 0.80);
        REQUIRE(rate.monoCompatibility > 0.50);
        REQUIRE(rate.monoCompatibility < 0.80);
        REQUIRE(rate.finite);
        REQUIRE(rate.impulsePeak > 0.0);
        REQUIRE(rate.chordPeak > 0.0);
        REQUIRE(rate.noisePeak > 0.0);
        REQUIRE(rate.impulsePeak < 1.0);
        REQUIRE(rate.chordPeak < 1.0);
        REQUIRE(rate.noisePeak < 1.0);
    }

    std::ifstream artifactStream(std::filesystem::path { REVERB_MEASUREMENTS_DIR }
        / "reverse-cosmic-shimmer-v1.json", std::ios::binary);
    REQUIRE(artifactStream.good());
    const auto artifact = nlohmann::json::parse(std::string {
        std::istreambuf_iterator<char> { artifactStream }, std::istreambuf_iterator<char> {} });
    const auto generated = nlohmann::json::parse(
        reverb::render::writeReverseCosmicShimmerValidationJson(report));
    REQUIRE(generated == artifact);
}
