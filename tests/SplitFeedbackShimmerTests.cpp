#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/NumericalSafetyGuard.h>
#include <reverb/dsp/PitchShiftContract.h>
#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/graph/SplitFeedbackShimmerGraph.h>
#include <reverb/render/SplitFeedbackShimmerValidation.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <random>
#include <ranges>
#include <set>
#include <span>
#include <string>
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

double renderTailEnergy(const GraphDocument& graph)
{
    constexpr auto sampleRate = 48'000.0;
    constexpr std::size_t blockSize = 128;
    auto compiled = reverb::graph::compileFeedbackGraph(graph, sampleRate, blockSize);
    REQUIRE(compiled.valid());
    std::array<float, blockSize> input {}, silence {}, left {}, right {};
    auto energy = 0.0;
    const auto blocks = static_cast<std::size_t>(std::ceil(sampleRate * 2.5 / blockSize));
    for (std::size_t block = 0; block < blocks; ++block) {
        input.fill(0.0F);
        if (block == 0) input[0] = 0.1F;
        compiled.runtime->process(input, silence, left, right);
        if (block * blockSize >= static_cast<std::size_t>(sampleRate * 1.25)) {
            for (std::size_t frame = 0; frame < blockSize; ++frame) {
                energy += static_cast<double>(left[frame]) * left[frame]
                    + static_cast<double>(right[frame]) * right[frame];
            }
        }
    }
    return energy;
}

} // namespace

TEST_CASE("Split Feedback Shimmer exposes two legal delayed feedback paths")
{
    const auto graph = reverb::graph::makeSplitFeedbackShimmerGraph();
    const std::set<std::string> publicTypes {
        "stereo-input", "stereo-output", "gain", "sum", "delay", "allpass", "lowpass", "pitch-shift",
    };
    REQUIRE(reverb::graph::validate(graph).valid());
    REQUIRE(graph.nodes.size() == 25);
    REQUIRE(graph.connections.size() == 29);
    REQUIRE(graph.layout.nodes.size() == graph.nodes.size());
    REQUIRE(std::ranges::all_of(graph.nodes, [&](const auto& item) {
        return publicTypes.contains(item.type);
    }));

    const auto compiled = reverb::graph::compileFeedbackGraph(graph, 48'000.0, 256);
    CAPTURE(compiled.errors, compiled.offendingLoops);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.feedbackComponents.size() == 1);
    const auto& component = compiled.feedbackComponents.front();
    for (const auto id : { "normal-feedback", "normal-feedback-delay", "shifted-feedback",
             "shifted-feedback-delay", "shifted-pitch", "feedback-recombine", "tank-delay" }) {
        REQUIRE(std::ranges::find(component, id) != component.end());
    }
    REQUIRE(hasCable(graph, "normal-feedback", "normal-feedback-delay"));
    REQUIRE(hasCable(graph, "shifted-feedback", "shifted-feedback-delay"));
    REQUIRE(hasCable(graph, "normal-feedback-delay", "feedback-recombine"));
    REQUIRE(hasCable(graph, "shifted-feedback-delay", "feedback-recombine"));

    const auto roundTrip = reverb::graph::parsePatchJson(reverb::graph::writePatchJson(graph));
    REQUIRE(roundTrip == graph);
}

TEST_CASE("Split Feedback Shimmer bounds independent loop controls and visible filtering")
{
    const auto graph = reverb::graph::makeSplitFeedbackShimmerGraph({
        .normalFeedback = 0.52,
        .shiftedFeedback = 0.12,
        .preShiftHighpassHertz = 410.0,
        .postShiftLowpassHertz = 4'600.0,
        .wetLevel = 0.70,
        .pitchSemitones = 10.0,
        .sizeMilliseconds = 180.0,
    });
    REQUIRE(parameter(graph, "normal-feedback", "gain") == Catch::Approx(0.52));
    REQUIRE(parameter(graph, "shifted-feedback", "gain") == Catch::Approx(0.12));
    REQUIRE(parameter(graph, "shifted-highpass-lowpass", "cutoff") == Catch::Approx(410.0));
    REQUIRE(parameter(graph, "shifted-highpass-invert", "gain") == Catch::Approx(-1.0));
    REQUIRE(parameter(graph, "shifted-damping", "cutoff") == Catch::Approx(4'600.0));
    REQUIRE(parameter(graph, "shifted-pitch", "semitones") == Catch::Approx(10.0));
    REQUIRE(parameter(graph, "tank-delay", "delay") == Catch::Approx(180.0));
    REQUIRE(parameter(graph, "wet-level", "gain") == Catch::Approx(0.70));
    REQUIRE(hasCable(graph, "tank-damping", "shifted-highpass-sum"));
    REQUIRE(hasCable(graph, "tank-damping", "shifted-highpass-lowpass"));
    REQUIRE(hasCable(graph, "shifted-highpass-lowpass", "shifted-highpass-invert"));
    REQUIRE(hasCable(graph, "shifted-highpass-invert", "shifted-highpass-sum"));
    REQUIRE(hasCable(graph, "shifted-pitch", "shifted-damping"));

    const auto bounded = reverb::graph::makeSplitFeedbackShimmerGraph({ 5.0, 5.0, 20.0, 50'000.0, 5.0 });
    REQUIRE(parameter(bounded, "normal-feedback", "gain")
        == Catch::Approx(reverb::graph::splitShimmerMaximumNormalFeedback));
    REQUIRE(parameter(bounded, "shifted-feedback", "gain")
        == Catch::Approx(reverb::graph::splitShimmerMaximumShiftedFeedback));
    REQUIRE(parameter(bounded, "normal-feedback", "gain")
            + parameter(bounded, "shifted-feedback", "gain")
        == Catch::Approx(reverb::graph::splitShimmerMaximumCombinedFeedback));
    REQUIRE(reverb::graph::splitShimmerMaximumCombinedFeedback <= 0.72);
    REQUIRE(parameter(bounded, "shifted-highpass-lowpass", "cutoff") == Catch::Approx(120.0));
    REQUIRE(parameter(bounded, "shifted-damping", "cutoff") == Catch::Approx(9'000.0));
    REQUIRE(parameter(bounded, "wet-level", "gain") == Catch::Approx(0.75));

    auto boundedPitchAndSize = reverb::graph::SplitFeedbackShimmerControls {};
    boundedPitchAndSize.pitchSemitones = -24.0;
    boundedPitchAndSize.sizeMilliseconds = 10'000.0;
    const auto pitchAndSizeGraph = reverb::graph::makeSplitFeedbackShimmerGraph(boundedPitchAndSize);
    REQUIRE(parameter(pitchAndSizeGraph, "shifted-pitch", "semitones")
        == Catch::Approx(reverb::graph::splitShimmerMinimumPitchSemitones));
    REQUIRE(parameter(pitchAndSizeGraph, "tank-delay", "delay")
        == Catch::Approx(reverb::graph::splitShimmerMaximumSizeMilliseconds));
}

TEST_CASE("Normal feedback sustains decay without shifted feedback")
{
    const auto noReturns = renderTailEnergy(reverb::graph::makeSplitFeedbackShimmerGraph({
        .normalFeedback = 0.0, .shiftedFeedback = 0.0 }));
    const auto normalOnly = renderTailEnergy(reverb::graph::makeSplitFeedbackShimmerGraph({
        .normalFeedback = 0.52, .shiftedFeedback = 0.0 }));
    const auto shiftedOnly = renderTailEnergy(reverb::graph::makeSplitFeedbackShimmerGraph({
        .normalFeedback = 0.0, .shiftedFeedback = 0.12 }));
    CAPTURE(noReturns, normalOnly, shiftedOnly);
    REQUIRE(normalOnly > noReturns * 10.0);
    REQUIRE(shiftedOnly > noReturns * 2.0);
}

TEST_CASE("Split Feedback Shimmer remains bounded and recoverable at extreme settings")
{
    constexpr std::size_t blockSize = 128;
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0 }) {
        const auto graph = reverb::graph::makeSplitFeedbackShimmerGraph({ 5.0, 5.0 });
        auto compiled = reverb::graph::compileFeedbackGraph(graph, sampleRate, blockSize);
        CAPTURE(sampleRate, compiled.errors);
        REQUIRE(compiled.valid());
        REQUIRE(compiled.delayMemory.withinBudget());
        std::mt19937 generator(0x53504c54U);
        std::uniform_real_distribution<float> noise(-0.04F, 0.04F);
        std::array<float, blockSize> input {}, silence {}, left {}, right {};
        auto peak = 0.0F;
        const auto blocks = static_cast<std::size_t>(std::ceil(sampleRate * 3.0 / blockSize));
        for (std::size_t block = 0; block < blocks; ++block) {
            for (auto& sample : input) sample = noise(generator);
            if (block == 0) input[0] = 0.1F;
            compiled.runtime->process(input, silence, left, right);
            for (std::size_t frame = 0; frame < blockSize; ++frame) {
                REQUIRE(std::isfinite(left[frame]));
                REQUIRE(std::isfinite(right[frame]));
                peak = std::max({ peak, std::abs(left[frame]), std::abs(right[frame]) });
            }
        }
        REQUIRE(peak > 0.0F);
        REQUIRE(peak < 1.0F);
    }

    constexpr auto sampleRate = 48'000.0;
    reverb::graph::AcyclicRuntimeHost host;
    const auto valid = reverb::graph::makeSplitFeedbackShimmerGraph();
    REQUIRE(host.compileFeedbackAndPublish(valid, sampleRate, blockSize).valid());
    auto invalid = valid;
    const auto removedDelay = [](const reverb::graph::Node& item) {
        return item.id == "shifted-feedback-delay" || item.id == "tank-delay";
    };
    std::erase_if(invalid.nodes, removedDelay);
    std::erase_if(invalid.layout.nodes, [](const reverb::graph::NodePosition& item) {
        return item.nodeId == "shifted-feedback-delay" || item.nodeId == "tank-delay";
    });
    std::erase_if(invalid.connections, [](const auto& connection) {
        return connection.from.nodeId == "shifted-feedback-delay"
            || connection.to.nodeId == "shifted-feedback-delay"
            || connection.from.nodeId == "tank-delay"
            || connection.to.nodeId == "tank-delay";
    });
    invalid.connections.push_back({ "illegal-shifted-return",
        { "shifted-feedback", "out" }, { "feedback-recombine", "in-b" } });
    invalid.connections.push_back({ "illegal-tank-bypass",
        { "tank-diffusion-a", "out" }, { "tank-diffusion-b", "in" } });
    const auto rejected = host.compileFeedbackAndPublish(invalid, sampleRate, blockSize);
    REQUIRE_FALSE(rejected.valid());
    REQUIRE(host.hasRuntime());

    std::array<float, blockSize> impulse {}, silence {}, left {}, right {};
    impulse[0] = 0.1F;
    auto heardValidRuntime = false;
    for (std::size_t block = 0; block < 200 && !heardValidRuntime; ++block) {
        host.process(impulse, silence, left, right);
        impulse.fill(0.0F);
        heardValidRuntime = std::ranges::any_of(left, [](const auto sample) { return sample != 0.0F; });
    }
    REQUIRE(heardValidRuntime);

    reverb::dsp::NumericalSafetyGuard guard { 0.01F, 0.005F, 1.0 };
    guard.prepare(sampleRate);
    std::array<float, blockSize> runaway {};
    runaway.fill(0.02F);
    auto status = guard.inspectAndMute(runaway);
    REQUIRE(status.violation == reverb::dsp::SafetyViolation::runawayLevel);
    REQUIRE(guard.isMuted());
    guard.reset();
    host.resetActiveRuntimes();
    REQUIRE_FALSE(guard.isMuted());
}

TEST_CASE("Split Feedback Shimmer measurements prove cumulative ascent and disclose quality")
{
    const auto report = reverb::render::measureSplitFeedbackShimmerValidation();
    REQUIRE(std::abs(report.early.octave12CentsError) <= 15.0);
    REQUIRE(std::abs(report.late.octave12CentsError) <= 15.0);
    REQUIRE(std::abs(report.late.octave24CentsError) <= 15.0);
    REQUIRE(report.early.octave12Dbfs > -70.0);
    REQUIRE(report.late.octave12Dbfs > -70.0);
    REQUIRE(report.late.octave24Dbfs > -90.0);
    REQUIRE(report.lateOctave24GrowthDb > 30.0);
    REQUIRE(report.feedbackVsParallelOctave24ContrastDb > 20.0);
    REQUIRE(report.shiftedFeedbackOctave24IncreaseDb > 15.0);
    REQUIRE(report.normalFeedbackTailEnergyIncreaseDb > 20.0);

    REQUIRE(report.strongestForwardGrainSidebandRelativeDb < -40.0);
    REQUIRE(report.foldedAliasRelativeToFirstOctaveDb < -20.0);
    REQUIRE(report.lowDampingOctave24LossDb < -1.0);
    REQUIRE(report.stereoCorrelation > 0.2);
    REQUIRE(report.stereoCorrelation < 0.95);

    REQUIRE(report.automation.size() == reverb::dsp::pitch_shift::qualificationSampleRates.size());
    for (std::size_t index = 0; index < report.automation.size(); ++index) {
        const auto& automation = report.automation[index];
        CAPTURE(automation.sampleRate, automation.peak, automation.maximumAdjacentStep);
        REQUIRE(automation.sampleRate == reverb::dsp::pitch_shift::qualificationSampleRates[index]);
        REQUIRE(automation.finite);
        REQUIRE(automation.peak > 0.0);
        REQUIRE(automation.peak < 1.0);
        REQUIRE(automation.maximumAdjacentStep < 0.01);
        REQUIRE(automation.successfulEdits == 18);
        REQUIRE(automation.completedCrossfades == 18);
    }

    std::ifstream artifactStream(std::filesystem::path { REVERB_MEASUREMENTS_DIR }
        / "split-feedback-shimmer-v1.json", std::ios::binary);
    REQUIRE(artifactStream.good());
    const auto artifact = nlohmann::json::parse(std::string {
        std::istreambuf_iterator<char> { artifactStream }, std::istreambuf_iterator<char> {} });
    const auto generated = nlohmann::json::parse(
        reverb::render::writeSplitFeedbackShimmerValidationJson(report));
    REQUIRE(generated == artifact);
}
