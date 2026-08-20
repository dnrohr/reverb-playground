#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/PitchShiftContract.h>
#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/graph/SafeParallelShimmerGraph.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
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

bool canReach(const GraphDocument& graph, const std::string_view from, const std::string_view to)
{
    std::vector<std::string> pending { std::string(from) };
    std::set<std::string> visited;
    while (!pending.empty()) {
        auto current = std::move(pending.back());
        pending.pop_back();
        if (current == to) return true;
        if (!visited.insert(current).second) continue;
        for (const auto& connection : graph.connections) {
            if (connection.from.nodeId == current)
                pending.push_back(connection.to.nodeId);
        }
    }
    return false;
}

bool canReturnTo(const GraphDocument& graph, const std::string_view nodeId)
{
    for (const auto& connection : graph.connections) {
        if (connection.from.nodeId == nodeId && canReach(graph, connection.to.nodeId, nodeId))
            return true;
    }
    return false;
}

bool hasCable(const GraphDocument& graph, const std::string_view from, const std::string_view to)
{
    return std::ranges::any_of(graph.connections, [&](const auto& connection) {
        return connection.from.nodeId == from && connection.to.nodeId == to;
    });
}

} // namespace

TEST_CASE("Safe Parallel Shimmer is a complete public mono-cable construction")
{
    const auto graph = reverb::graph::makeSafeParallelShimmerGraph();
    const std::set<std::string> publicTypes {
        "stereo-input", "stereo-output", "gain", "sum", "delay", "allpass", "lowpass", "pitch-shift",
    };

    REQUIRE(reverb::graph::validate(graph).valid());
    REQUIRE(graph.nodes.size() == 28);
    REQUIRE(graph.connections.size() == 32);
    REQUIRE(graph.layout.nodes.size() == graph.nodes.size());
    REQUIRE(std::ranges::all_of(graph.nodes, [&](const auto& item) {
        return publicTypes.contains(item.type);
    }));
    REQUIRE(std::ranges::count(graph.nodes, std::string("pitch-shift"), &reverb::graph::Node::type) == 1);
    REQUIRE(std::ranges::all_of(graph.connections, [&](const auto& connection) {
        const auto& source = node(graph, connection.from.nodeId);
        const auto& destination = node(graph, connection.to.nodeId);
        const auto fromPort = std::ranges::find(source.ports, connection.from.portId, &reverb::graph::Port::id);
        const auto toPort = std::ranges::find(destination.ports, connection.to.portId, &reverb::graph::Port::id);
        return fromPort != source.ports.end() && toPort != destination.ports.end()
            && fromPort->signal == reverb::graph::SignalType::audio
            && toPort->signal == reverb::graph::SignalType::audio;
    }));

    const auto roundTrip = reverb::graph::parsePatchJson(reverb::graph::writePatchJson(graph));
    REQUIRE(roundTrip == graph);
}

TEST_CASE("Safe Parallel Shimmer keeps the shifted branch structurally outside feedback")
{
    const auto graph = reverb::graph::makeSafeParallelShimmerGraph();
    const auto compiled = reverb::graph::compileFeedbackGraph(graph, 48'000.0, 256);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.feedbackComponents == std::vector<std::vector<std::string>> {
        { "feedback-delay", "reverb-decay", "tank-damping", "tank-delay",
            "tank-diffusion-a", "tank-diffusion-b", "tank-entry" },
    });
    REQUIRE(canReach(graph, "shimmer-pitch", "output"));
    REQUIRE_FALSE(canReturnTo(graph, "shimmer-pitch"));
    REQUIRE_FALSE(canReach(graph, "shimmer-pitch", "tank-entry"));
    REQUIRE_FALSE(canReach(graph, "shimmer-level", "reverb-decay"));

    REQUIRE(hasCable(graph, "tank-damping", "shimmer-highpass-sum"));
    REQUIRE(hasCable(graph, "tank-damping", "shimmer-highpass-lowpass"));
    REQUIRE(hasCable(graph, "shimmer-highpass-lowpass", "shimmer-highpass-invert"));
    REQUIRE(hasCable(graph, "shimmer-highpass-invert", "shimmer-highpass-sum"));
    REQUIRE(parameter(graph, "shimmer-highpass-invert", "gain") == -1.0);
}

TEST_CASE("Safe Parallel Shimmer controls retain separate bounded responsibilities")
{
    const reverb::graph::SafeParallelShimmerControls requested {
        .reverbDecay = 0.63,
        .shimmerLevel = 0.27,
        .shimmerDampingHertz = 4'800.0,
        .wetBalance = 0.70,
    };
    const auto graph = reverb::graph::makeSafeParallelShimmerGraph(requested);
    REQUIRE(parameter(graph, "reverb-decay", "gain") == Catch::Approx(0.63));
    REQUIRE(parameter(graph, "shimmer-level", "gain") == Catch::Approx(0.27));
    REQUIRE(parameter(graph, "shimmer-damping", "cutoff") == Catch::Approx(4'800.0));
    REQUIRE(parameter(graph, "wet-balance", "gain") == Catch::Approx(0.70));
    REQUIRE(parameter(graph, "normal-level", "gain") == Catch::Approx(0.50));
    REQUIRE(parameter(graph, "shimmer-pitch", "semitones") == Catch::Approx(12.0));

    const auto bounded = reverb::graph::makeSafeParallelShimmerGraph({ 5.0, 5.0, 50'000.0, 5.0 });
    REQUIRE(parameter(bounded, "reverb-decay", "gain") == Catch::Approx(0.72));
    REQUIRE(parameter(bounded, "shimmer-level", "gain") == Catch::Approx(0.30));
    REQUIRE(parameter(bounded, "shimmer-damping", "cutoff") == Catch::Approx(12'000.0));
    REQUIRE(parameter(bounded, "wet-balance", "gain") == Catch::Approx(0.80));
}

TEST_CASE("Safe Parallel Shimmer aligns latency and fits the delay-memory budget")
{
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0, 192'000.0 }) {
        const auto graph = reverb::graph::makeSafeParallelShimmerGraph();
        const auto compiled = reverb::graph::compileFeedbackGraph(graph, sampleRate, 256);
        CAPTURE(sampleRate, compiled.errors);
        REQUIRE(compiled.valid());
        const auto alignmentSamples = static_cast<std::size_t>(std::llround(
            reverb::graph::safeParallelShimmerAlignmentMilliseconds * sampleRate / 1'000.0));
        const auto pitchLatency = reverb::dsp::pitch_shift::reportedLatencySamples(sampleRate);
        const auto difference = alignmentSamples > pitchLatency
            ? alignmentSamples - pitchLatency : pitchLatency - alignmentSamples;
        REQUIRE(difference <= 2);
        REQUIRE(compiled.delayMemory.withinBudget());
        REQUIRE(compiled.delayMemory.allocatedBytes < 4U * 1024U * 1024U);
        REQUIRE(compiled.delayMemory.lineCount == 12);
    }
}

TEST_CASE("Safe Parallel Shimmer renders finite bounded decorrelated stereo at qualified rates")
{
    constexpr std::size_t blockSize = 256;
    for (const auto sampleRate : reverb::dsp::pitch_shift::qualificationSampleRates) {
        auto compiled = reverb::graph::compileFeedbackGraph(
            reverb::graph::makeSafeParallelShimmerGraph(), sampleRate, blockSize);
        REQUIRE(compiled.valid());
        std::mt19937 generator(0x5348494dU);
        std::uniform_real_distribution<float> noise(-0.025F, 0.025F);
        std::array<float, blockSize> inputLeft {}, inputRight {}, outputLeft {}, outputRight {};
        const auto blocks = static_cast<std::size_t>(std::ceil(sampleRate * 2.0 / blockSize));
        auto peak = 0.0F;
        auto energy = 0.0;
        auto stereoDifference = 0.0;
        for (std::size_t block = 0; block < blocks; ++block) {
            for (auto& sample : inputLeft) sample = noise(generator);
            for (auto& sample : inputRight) sample = noise(generator);
            if (block == 0) inputLeft[0] = 0.1F;
            compiled.runtime->process(inputLeft, inputRight, outputLeft, outputRight);
            for (std::size_t frame = 0; frame < blockSize; ++frame) {
                REQUIRE(std::isfinite(outputLeft[frame]));
                REQUIRE(std::isfinite(outputRight[frame]));
                peak = std::max({ peak, std::abs(outputLeft[frame]), std::abs(outputRight[frame]) });
                energy += static_cast<double>(outputLeft[frame]) * outputLeft[frame]
                    + static_cast<double>(outputRight[frame]) * outputRight[frame];
                const auto difference = static_cast<double>(outputLeft[frame]) - outputRight[frame];
                stereoDifference += difference * difference;
            }
        }
        CAPTURE(sampleRate, peak, energy, stereoDifference);
        REQUIRE(peak < 1.0F);
        REQUIRE(energy > 0.0);
        REQUIRE(stereoDifference > energy * 0.001);
    }
}
