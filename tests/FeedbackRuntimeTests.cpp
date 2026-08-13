#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/GravityDiffusionGraph.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/dsp/NumericalSafetyGuard.h>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace reverb::graph;
Port inputPort(std::string id = "in") { return { std::move(id), SignalType::audio, PortDirection::input }; }
Port outputPort(std::string id = "out") { return { std::move(id), SignalType::audio, PortDirection::output }; }
Port controlInputPort(std::string id) { return { std::move(id), SignalType::control, PortDirection::input }; }
Port controlOutputPort(std::string id = "out") { return { std::move(id), SignalType::control, PortDirection::output }; }
Node stereoInput() { return { "input", "stereo-input", { outputPort("out-l"), outputPort("out-r") }, {} }; }
Node stereoOutput() { return { "output", "stereo-output", { inputPort("in-l"), inputPort("in-r") }, {} }; }
Node sumNode(std::string id) { return { std::move(id), "sum", { inputPort("in-a"), inputPort("in-b"), outputPort() }, {} }; }
Node delayNode(std::string id, const double milliseconds = 1.0) { return { std::move(id), "delay", { inputPort(), outputPort() }, { { "delay", milliseconds, "milliseconds" } } }; }
Node allpassNode(std::string id, const double milliseconds, const double coefficient = 0.5) { return { std::move(id), "allpass", { inputPort(), outputPort() }, { { "delay", milliseconds, "milliseconds" }, { "coefficient", coefficient, "unitless" } } }; }
Node lowpassNode(std::string id, const double cutoff = 6'000.0) { return { std::move(id), "lowpass", { inputPort(), outputPort() }, { { "cutoff", cutoff, "hertz" } } }; }
Node gainNode(std::string id, const double gain = 1.0) { return { std::move(id), "gain", { inputPort(), outputPort() }, { { "gain", gain, "linear" } } }; }
Connection cable(std::string id, std::string fromNode, std::string fromPort, std::string toNode, std::string toPort)
{ return { std::move(id), { std::move(fromNode), std::move(fromPort) }, { std::move(toNode), std::move(toPort) } }; }

GraphDocument simpleFeedbackGraph()
{
    GraphDocument graph;
    graph.nodes = { stereoInput(), sumNode("sum"), gainNode("feedback", 0.5), delayNode("delay"), stereoOutput() };
    graph.connections = {
        cable("input-sum", "input", "out-l", "sum", "in-a"), cable("delay-sum", "delay", "out", "sum", "in-b"),
        cable("sum-gain", "sum", "out", "feedback", "in"), cable("gain-delay", "feedback", "out", "delay", "in"),
        cable("delay-output", "delay", "out", "output", "in-l"), cable("right-output", "input", "out-r", "output", "in-r"),
    };
    return graph;
}

GraphDocument algebraicLoopGraph()
{
    GraphDocument graph;
    graph.nodes = { stereoInput(), gainNode("gain-a"), gainNode("gain-b"), stereoOutput() };
    graph.connections = {
        cable("a-b", "gain-a", "out", "gain-b", "in"), cable("b-a", "gain-b", "out", "gain-a", "in"),
        cable("a-left", "gain-a", "out", "output", "in-l"), cable("right", "input", "out-r", "output", "in-r"),
    };
    return graph;
}

GraphDocument gravityDiffusionDesignGraph()
{
    GraphDocument graph;
    graph.nodes = {
        stereoInput(), gainNode("input-l-half", 0.5), gainNode("input-r-half", 0.5), sumNode("input-sum"),
        allpassNode("input-ap-1", 3.1), allpassNode("input-ap-2", 4.7),
        allpassNode("input-ap-3", 7.9), allpassNode("input-ap-4", 11.3), sumNode("tank-entry"),
    };
    const std::array stageDelays { 23.0, 29.0, 37.0, 43.0, 53.0, 61.0, 71.0, 83.0 };
    const std::array stageAllpasses { 13.7, 17.9, 19.3, 23.1, 29.7, 31.1, 37.1, 41.3 };
    for (std::size_t index = 0; index < stageDelays.size(); ++index) {
        const auto number = std::to_string(index + 1);
        graph.nodes.push_back(delayNode("stage-delay-" + number, stageDelays[index]));
        graph.nodes.push_back(allpassNode("stage-ap-" + number, stageAllpasses[index]));
        graph.nodes.push_back(gainNode("tap-gain-" + number, 0.24));
    }
    graph.nodes.insert(graph.nodes.end(), {
        lowpassNode("feedback-damping", 5'800.0), gainNode("feedback-gain", 0.58),
        delayNode("feedback-delay", 97.0),
        sumNode("left-sum-a"), sumNode("left-sum-b"), sumNode("left-sum"),
        sumNode("right-sum-a"), sumNode("right-sum-b"), sumNode("right-sum"), stereoOutput(),
    });
    graph.connections = {
        cable("input-l-gain", "input", "out-l", "input-l-half", "in"),
        cable("input-r-gain", "input", "out-r", "input-r-half", "in"),
        cable("input-l-sum", "input-l-half", "out", "input-sum", "in-a"),
        cable("input-r-sum", "input-r-half", "out", "input-sum", "in-b"),
        cable("input-ap-1", "input-sum", "out", "input-ap-1", "in"),
        cable("input-ap-2", "input-ap-1", "out", "input-ap-2", "in"),
        cable("input-ap-3", "input-ap-2", "out", "input-ap-3", "in"),
        cable("input-ap-4", "input-ap-3", "out", "input-ap-4", "in"),
        cable("input-tank", "input-ap-4", "out", "tank-entry", "in-a"),
        cable("feedback-tank", "feedback-delay", "out", "tank-entry", "in-b"),
    };
    auto previous = std::string("tank-entry");
    for (std::size_t index = 0; index < stageDelays.size(); ++index) {
        const auto number = std::to_string(index + 1);
        graph.connections.push_back(cable("stage-in-" + number, previous, "out", "stage-delay-" + number, "in"));
        graph.connections.push_back(cable("stage-diffuse-" + number, "stage-delay-" + number, "out", "stage-ap-" + number, "in"));
        graph.connections.push_back(cable("tap-" + number, "stage-ap-" + number, "out", "tap-gain-" + number, "in"));
        previous = "stage-ap-" + number;
    }
    graph.connections.insert(graph.connections.end(), {
        cable("stage-8-damping", "stage-ap-8", "out", "feedback-damping", "in"),
        cable("damping-feedback", "feedback-damping", "out", "feedback-gain", "in"),
        cable("feedback-return-delay", "feedback-gain", "out", "feedback-delay", "in"),
        cable("tap-1-left-a", "tap-gain-1", "out", "left-sum-a", "in-a"),
        cable("tap-3-left-a", "tap-gain-3", "out", "left-sum-a", "in-b"),
        cable("tap-5-left-b", "tap-gain-5", "out", "left-sum-b", "in-a"),
        cable("tap-7-left-b", "tap-gain-7", "out", "left-sum-b", "in-b"),
        cable("left-a-final", "left-sum-a", "out", "left-sum", "in-a"),
        cable("left-b-final", "left-sum-b", "out", "left-sum", "in-b"),
        cable("tap-2-right-a", "tap-gain-2", "out", "right-sum-a", "in-a"),
        cable("tap-4-right-a", "tap-gain-4", "out", "right-sum-a", "in-b"),
        cable("tap-6-right-b", "tap-gain-6", "out", "right-sum-b", "in-a"),
        cable("tap-8-right-b", "tap-gain-8", "out", "right-sum-b", "in-b"),
        cable("right-a-final", "right-sum-a", "out", "right-sum", "in-a"),
        cable("right-b-final", "right-sum-b", "out", "right-sum", "in-b"),
        cable("left-output", "left-sum", "out", "output", "in-l"),
        cable("right-output", "right-sum", "out", "output", "in-r"),
    });
    return graph;
}

struct GravityMetrics {
    double timeToPeakMs {};
    double earlyLateRatioDb {};
    double integratedEnergyDb {};
    std::size_t onsetFrame {};
};

GravityMetrics renderGravity(const double gravity, const double sampleRate = 48'000.0, const double seconds = 3.0)
{
    constexpr std::size_t blockSize = 256;
    auto compiled = compileFeedbackGraph(makeGravityDiffusionGraph(gravity), sampleRate, blockSize);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.runtime->setMacroValue("gravity", gravity));
    std::array<float, blockSize> settleInput {}, settleLeft {}, settleRight {};
    for (int block = 0; block < 8; ++block)
        compiled.runtime->process(settleInput, settleInput, settleLeft, settleRight);
    const auto frameCount = static_cast<std::size_t>(seconds * sampleRate);
    std::vector<float> left(frameCount), right(frameCount), inputLeft(blockSize), inputRight(blockSize);
    for (std::size_t offset = 0; offset < frameCount; offset += blockSize) {
        const auto count = std::min(blockSize, frameCount - offset);
        std::ranges::fill(inputLeft, 0.0F);
        if (offset == 0) inputLeft[0] = 1.0F;
        compiled.runtime->process(std::span(inputLeft).first(count), std::span(inputRight).first(count),
            std::span(left).subspan(offset, count), std::span(right).subspan(offset, count));
    }
    std::vector<double> energy(frameCount);
    for (std::size_t frame = 0; frame < frameCount; ++frame)
        energy[frame] = 0.5 * (static_cast<double>(left[frame]) * left[frame] + static_cast<double>(right[frame]) * right[frame]);
    const auto onset = static_cast<std::size_t>(std::distance(energy.begin(), std::ranges::find_if(energy, [](double value) { return value > 1.0e-14; })));
    const auto window = std::max<std::size_t>(1, static_cast<std::size_t>(0.020 * sampleRate));
    const auto half = window / 2;
    double best = -1.0;
    std::size_t peak = 0;
    double running = 0.0;
    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        if (frame + half < frameCount) running += energy[frame + half];
        if (frame > half) running -= energy[frame - half - 1];
        const auto first = frame > half ? frame - half : 0;
        const auto last = std::min(frameCount, frame + half + 1);
        const auto smoothed = running / static_cast<double>(last - first);
        if (smoothed > best) { best = smoothed; peak = frame; }
    }
    const auto horizon = std::min(frameCount - onset, static_cast<std::size_t>(0.7 * sampleRate));
    const auto quarter = horizon / 4;
    const auto early = std::accumulate(energy.begin() + static_cast<std::ptrdiff_t>(onset),
        energy.begin() + static_cast<std::ptrdiff_t>(onset + quarter), 0.0);
    const auto late = std::accumulate(energy.begin() + static_cast<std::ptrdiff_t>(onset + 3 * quarter),
        energy.begin() + static_cast<std::ptrdiff_t>(onset + horizon), 0.0);
    const auto total = std::accumulate(energy.begin(), energy.begin() + static_cast<std::ptrdiff_t>(onset + horizon), 0.0);
    return { 1'000.0 * static_cast<double>(peak) / sampleRate,
        10.0 * std::log10((early + 1.0e-20) / (late + 1.0e-20)),
        10.0 * std::log10(total + 1.0e-20), onset };
}
}

TEST_CASE("Delay-containing feedback renders a deterministic causal recurrence")
{
    auto first = compileFeedbackGraph(simpleFeedbackGraph(), 1'000.0, 8);
    auto second = compileFeedbackGraph(simpleFeedbackGraph(), 1'000.0, 8);
    auto partitioned = compileFeedbackGraph(simpleFeedbackGraph(), 1'000.0, 8);
    REQUIRE(first.valid()); REQUIRE(second.valid()); REQUIRE(partitioned.valid());
    REQUIRE(first.feedbackComponents == std::vector<std::vector<std::string>> { { "delay", "feedback", "sum" } });
    const std::array inputLeft { 1.0F, 0.0F, 0.0F, 0.0F, 0.0F }; const std::array inputRight { 0.0F, 0.0F, 0.0F, 0.0F, 0.0F };
    std::array<float, 5> firstLeft {}; std::array<float, 5> firstRight {}; std::array<float, 5> secondLeft {}; std::array<float, 5> secondRight {};
    first.runtime->process(inputLeft, inputRight, firstLeft, firstRight); second.runtime->process(inputLeft, inputRight, secondLeft, secondRight);
    std::array<float, 5> partitionedLeft {}; std::array<float, 5> partitionedRight {};
    partitioned.runtime->process(std::span(inputLeft).first(2), std::span(inputRight).first(2), std::span(partitionedLeft).first(2), std::span(partitionedRight).first(2));
    partitioned.runtime->process(std::span(inputLeft).subspan(2), std::span(inputRight).subspan(2), std::span(partitionedLeft).subspan(2), std::span(partitionedRight).subspan(2));
    REQUIRE(firstLeft == std::array { 0.0F, 0.5F, 0.25F, 0.125F, 0.0625F });
    REQUIRE(secondLeft == firstLeft); REQUIRE(secondRight == firstRight); REQUIRE(partitionedLeft == firstLeft);
}

TEST_CASE("Gravity Diffusion design has eight depth taps, twelve allpasses, and bounded delayed feedback")
{
    const auto graph = gravityDiffusionDesignGraph();
    const std::array publicAudioTypes { std::string_view("stereo-input"), std::string_view("stereo-output"),
        std::string_view("gain"), std::string_view("sum"), std::string_view("delay"),
        std::string_view("allpass"), std::string_view("lowpass") };
    REQUIRE(graph.nodes.size() == 43);
    REQUIRE(graph.connections.size() == 51);
    REQUIRE(std::ranges::all_of(graph.nodes, [&](const Node& node) {
        return std::ranges::find(publicAudioTypes, node.type) != publicAudioTypes.end();
    }));
    REQUIRE(std::ranges::count_if(graph.nodes, [](const Node& node) { return node.type == "allpass"; }) == 12);
    REQUIRE(std::ranges::count_if(graph.nodes, [](const Node& node) { return node.id.starts_with("tap-gain-"); }) == 8);
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0, 192'000.0 }) {
        const auto compiled = compileFeedbackGraph(graph, sampleRate, 1'024);
        REQUIRE(compiled.valid());
        REQUIRE(compiled.offendingLoops.empty());
        REQUIRE(compiled.feedbackComponents.size() == 1);
        REQUIRE(compiled.delayMemory.lineCount == 21);
        REQUIRE(compiled.delayMemory.allocatedBytes < 2U * 1024U * 1024U);
        REQUIRE(compiled.delayMemory.withinBudget());
        if (sampleRate == 192'000.0) {
            REQUIRE(compiled.delayMemory.allocatedSamples == 325'836);
            REQUIRE(compiled.delayMemory.allocatedBytes == 1'303'344);
        }
    }
}

TEST_CASE("Gravity tap weighting is exactly normalized and monotonic")
{
    using Catch::Approx;
    std::array<double, 8> previous {};
    for (const auto gravity : { -1.0, -0.5, 0.0, 0.5, 1.0 }) {
        const auto weights = gravityTapWeights(gravity);
        const auto total = std::accumulate(weights.begin(), weights.end(), 0.0);
        const auto left = weights[0] + weights[2] + weights[4] + weights[6];
        const auto right = weights[1] + weights[3] + weights[5] + weights[7];
        REQUIRE(total == Approx(1.0).margin(1.0e-12));
        REQUIRE(left == Approx(0.5).margin(1.0e-12));
        REQUIRE(right == Approx(0.5).margin(1.0e-12));
        REQUIRE(std::ranges::all_of(weights, [](double value) { return value >= 0.0 && value <= 0.25; }));
        if (gravity > -1.0) {
            REQUIRE(weights[0] >= previous[0]);
            REQUIRE(weights[2] >= previous[2]);
            REQUIRE(weights[4] <= previous[4]);
            REQUIRE(weights[6] <= previous[6]);
        }
        previous = weights;
    }
    REQUIRE(gravityTapWeights(-2.0) == gravityTapWeights(-1.0));
    REQUIRE(gravityTapWeights(2.0) == gravityTapWeights(1.0));
}

TEST_CASE("Normalized Gravity graph exposes eight inspectable weighting branches")
{
    const auto graph = makeGravityDiffusionGraph();
    REQUIRE(validate(graph).valid());
    REQUIRE(std::ranges::count_if(graph.nodes, [](const Node& node) { return node.type == "control-map"; }) == 8);
    REQUIRE(std::ranges::count_if(graph.connections, [](const Connection& connection) {
        return connection.from.nodeId == "gravity" && connection.to.nodeId.starts_with("gravity-map-");
    }) == 8);
    REQUIRE(parsePatchJson(writePatchJson(graph)) == graph);
    REQUIRE(std::ranges::count_if(graph.connections, [](const Connection& connection) {
        return connection.from.nodeId.starts_with("gravity-map-") && connection.to.portId == "gain-mod";
    }) == 8);
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0, 192'000.0 }) {
        auto compiled = compileFeedbackGraph(graph, sampleRate, 256);
        REQUIRE(compiled.valid());
        REQUIRE(compiled.feedbackComponents.size() == 1);
        REQUIRE(compiled.delayMemory.withinBudget());
    }
}

TEST_CASE("Gravity moves measured energy monotonically from deep to early taps")
{
    const std::array states { -1.0, -0.5, 0.0, 0.5, 1.0 };
    std::array<GravityMetrics, states.size()> measurements {};
    for (std::size_t index = 0; index < states.size(); ++index) {
        measurements[index] = renderGravity(states[index]);
        CAPTURE(states[index], measurements[index].timeToPeakMs,
            measurements[index].earlyLateRatioDb, measurements[index].integratedEnergyDb);
    }
    for (std::size_t index = 1; index < measurements.size(); ++index) {
        REQUIRE(measurements[index].earlyLateRatioDb > measurements[index - 1].earlyLateRatioDb);
        REQUIRE(measurements[index].timeToPeakMs <= measurements[index - 1].timeToPeakMs);
    }
    REQUIRE(measurements.front().onsetFrame > 0);
    REQUIRE(measurements.front().timeToPeakMs > measurements[2].timeToPeakMs);
    REQUIRE(measurements[2].timeToPeakMs > measurements.back().timeToPeakMs);
    REQUIRE(measurements.front().earlyLateRatioDb < 0.0);
    REQUIRE(measurements.back().earlyLateRatioDb > 0.0);
    const auto [minimum, maximum] = std::ranges::minmax_element(measurements, {}, &GravityMetrics::integratedEnergyDb);
    REQUIRE(maximum->integratedEnergyDb - minimum->integratedEnergyDb < 2.1);
}

TEST_CASE("Gravity renders causally at supported sample rates and reset is deterministic")
{
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0 }) {
        const auto inverse = renderGravity(-1.0, sampleRate);
        const auto forward = renderGravity(1.0, sampleRate);
        REQUIRE(inverse.onsetFrame > 0);
        REQUIRE(forward.onsetFrame > 0);
        REQUIRE(inverse.timeToPeakMs > forward.timeToPeakMs);
        REQUIRE(std::isfinite(inverse.integratedEnergyDb));
        REQUIRE(std::isfinite(forward.integratedEnergyDb));
    }

    constexpr std::size_t frameCount = 65'536;
    auto compiled = compileFeedbackGraph(makeGravityDiffusionGraph(), 48'000.0, frameCount);
    REQUIRE(compiled.valid());
    std::vector<float> input(frameCount), silence(frameCount), firstLeft(frameCount), firstRight(frameCount);
    std::vector<float> secondLeft(frameCount), secondRight(frameCount);
    input.front() = 1.0F;
    compiled.runtime->process(input, silence, firstLeft, firstRight);
    compiled.runtime->reset();
    compiled.runtime->process(input, silence, secondLeft, secondRight);
    REQUIRE(secondLeft == firstLeft);
    REQUIRE(secondRight == firstRight);
}

TEST_CASE("Rapid Gravity automation remains finite and continuously ramped")
{
    constexpr std::size_t blockSize = 64;
    auto compiled = compileFeedbackGraph(makeGravityDiffusionGraph(), 48'000.0, blockSize);
    REQUIRE(compiled.valid());
    std::array<float, blockSize> inputLeft {}, inputRight {}, outputLeft {}, outputRight {};
    std::ranges::fill(inputLeft, 0.05F);
    float previous = 0.0F;
    double largestStep = 0.0;
    bool finite = true;
    for (int block = 0; block < 2'000; ++block) {
        REQUIRE(compiled.runtime->setMacroValue("gravity", block % 2 == 0 ? -1.0 : 1.0));
        compiled.runtime->process(inputLeft, inputRight, outputLeft, outputRight);
        for (const auto sample : outputLeft) {
            finite = finite && std::isfinite(sample);
            largestStep = std::max(largestStep, std::abs(static_cast<double>(sample - previous)));
            previous = sample;
        }
        finite = finite && std::ranges::all_of(outputRight, [](float sample) { return std::isfinite(sample); });
    }
    CAPTURE(largestStep);
    REQUIRE(finite);
    REQUIRE(largestStep < 0.10);
}

TEST_CASE("Modulated delay feedback is finite and deterministic across host block partitions")
{
    auto graph = simpleFeedbackGraph();
    auto& delay = *std::ranges::find(graph.nodes, "delay", &Node::id);
    delay.ports.insert(delay.ports.begin() + 1, controlInputPort("delay-mod"));
    delay.parameters.front().modulation = ParameterModulation {
        "delay-mod", 0.75, ModulationPolarity::bipolar, 1.0, 3.0 };
    graph.nodes.insert(graph.nodes.end() - 1, {
        "lfo", "lfo", { controlOutputPort() }, {
            { "frequency", 100.0, "hertz" }, { "phase", 0.0, "cycles" },
            { "waveform", 1.0, "waveform" }, { "run-mode", 0.0, "run-mode" },
        },
    });
    graph.connections.push_back(cable("lfo-delay", "lfo", "out", "delay", "delay-mod"));
    auto whole = compileFeedbackGraph(graph, 1'000.0, 32);
    auto partitioned = compileFeedbackGraph(graph, 1'000.0, 32);
    REQUIRE(whole.valid()); REQUIRE(partitioned.valid());
    std::array<float, 32> input {}; input.front() = 0.5F;
    std::array<float, 32> silence {}; std::array<float, 32> wholeLeft {}; std::array<float, 32> wholeRight {};
    std::array<float, 32> partitionedLeft {}; std::array<float, 32> partitionedRight {};
    whole.runtime->process(input, silence, wholeLeft, wholeRight);
    partitioned.runtime->process(std::span(input).first(11), std::span(silence).first(11),
        std::span(partitionedLeft).first(11), std::span(partitionedRight).first(11));
    partitioned.runtime->process(std::span(input).subspan(11), std::span(silence).subspan(11),
        std::span(partitionedLeft).subspan(11), std::span(partitionedRight).subspan(11));
    REQUIRE(partitionedLeft == wholeLeft);
    REQUIRE(std::ranges::all_of(wholeLeft, [](const float sample) { return std::isfinite(sample); }));
}

TEST_CASE("Zero-delay algebraic cycles report the exact offending loop")
{
    const auto compiled = compileFeedbackGraph(algebraicLoopGraph(), 48'000.0, 64);
    REQUIRE_FALSE(compiled.valid());
    REQUIRE(compiled.offendingLoops == std::vector<std::vector<std::string>> { { "gain-a", "gain-b", "gain-a" } });
    REQUIRE(compiled.errors == std::vector<std::string> { "zero-delay algebraic loop: gain-a -> gain-b -> gain-a" });
}

TEST_CASE("Algebraic sub-loop is rejected inside a delay-containing component")
{
    GraphDocument graph;
    graph.nodes = { stereoInput(), gainNode("gain-a"), gainNode("gain-b"), sumNode("sum"), delayNode("delay"), stereoOutput() };
    graph.connections = {
        cable("a-b", "gain-a", "out", "gain-b", "in"), cable("b-sum", "gain-b", "out", "sum", "in-a"),
        cable("delay-sum", "delay", "out", "sum", "in-b"), cable("sum-a", "sum", "out", "gain-a", "in"),
        cable("sum-delay", "sum", "out", "delay", "in"), cable("left", "gain-a", "out", "output", "in-l"),
        cable("right", "input", "out-r", "output", "in-r"),
    };
    const auto compiled = compileFeedbackGraph(graph, 48'000.0, 64);
    REQUIRE_FALSE(compiled.valid());
    REQUIRE(compiled.offendingLoops == std::vector<std::vector<std::string>> { { "gain-a", "gain-b", "sum", "gain-a" } });
}

TEST_CASE("Nested and multiple feedback loops compile and render deterministically")
{
    GraphDocument graph;
    graph.nodes = { stereoInput(), sumNode("sum-a"), sumNode("sum-b"), sumNode("sum-c"), delayNode("delay-a"), delayNode("delay-b"), delayNode("delay-c"), stereoOutput() };
    graph.connections = {
        cable("input-a", "input", "out-l", "sum-a", "in-a"), cable("delay-a-sum", "delay-a", "out", "sum-a", "in-b"),
        cable("a-b", "sum-a", "out", "sum-b", "in-a"), cable("delay-b-sum", "delay-b", "out", "sum-b", "in-b"),
        cable("b-delay-a", "sum-b", "out", "delay-a", "in"), cable("b-delay-b", "sum-b", "out", "delay-b", "in"),
        cable("b-left", "sum-b", "out", "output", "in-l"), cable("input-c", "input", "out-r", "sum-c", "in-a"),
        cable("delay-c-sum", "delay-c", "out", "sum-c", "in-b"), cable("c-delay", "sum-c", "out", "delay-c", "in"),
        cable("c-right", "sum-c", "out", "output", "in-r"),
    };
    auto first = compileFeedbackGraph(graph, 1'000.0, 32); auto second = compileFeedbackGraph(graph, 1'000.0, 32);
    REQUIRE(first.valid()); REQUIRE(second.valid());
    REQUIRE(first.feedbackComponents == std::vector<std::vector<std::string>> {
        { "delay-a", "delay-b", "sum-a", "sum-b" }, { "delay-c", "sum-c" },
    });
    std::array<float, 16> left {}; std::array<float, 16> right {}; left[0] = 0.25F; right[0] = -0.5F;
    std::array<float, 16> firstLeft {}; std::array<float, 16> firstRight {}; std::array<float, 16> secondLeft {}; std::array<float, 16> secondRight {};
    first.runtime->process(left, right, firstLeft, firstRight); second.runtime->process(left, right, secondLeft, secondRight);
    REQUIRE(firstLeft == secondLeft); REQUIRE(firstRight == secondRight);
    REQUIRE(std::ranges::all_of(firstLeft, [](float value) { return std::isfinite(value); }));
}

TEST_CASE("Feedback publication rejects invalid edits without silencing the active loop")
{
    AcyclicRuntimeHost host;
    REQUIRE(host.compileFeedbackAndPublish(simpleFeedbackGraph(), 1'000.0, 8).valid());
    const std::array input { 1.0F, 0.0F, 0.0F }; const std::array silence { 0.0F, 0.0F, 0.0F };
    std::array<float, 3> left {}; std::array<float, 3> right {}; host.process(input, silence, left, right);
    REQUIRE_FALSE(host.compileFeedbackAndPublish(algebraicLoopGraph(), 1'000.0, 8).valid());
    std::array<float, 1> continuedLeft {}; std::array<float, 1> continuedRight {}; const std::array<float, 1> zero {};
    host.process(zero, zero, continuedLeft, continuedRight);
    REQUIRE(continuedLeft[0] == 0.125F);
}

TEST_CASE("Legal feedback topology changes remain finite through the bounded crossfade")
{
    AcyclicRuntimeHost host;
    REQUIRE(host.compileFeedbackAndPublish(simpleFeedbackGraph(), 1'000.0, 8).valid());
    std::array<float, 8> input {}; input.fill(0.25F);
    std::array<float, 8> outputLeft {}; std::array<float, 8> outputRight {};
    host.process(input, input, outputLeft, outputRight);

    auto edited = simpleFeedbackGraph();
    std::ranges::find(edited.nodes, "feedback", &Node::id)->parameters[0].value = 0.25;
    REQUIRE(host.compileFeedbackAndPublish(edited, 1'000.0, 8).valid());
    host.process(input, input, outputLeft, outputRight);
    auto snapshot = host.publicationSnapshot();
    REQUIRE(snapshot.crossfadeFromRevision == 1);
    REQUIRE(snapshot.crossfadePositionSamples == 8);
    REQUIRE(snapshot.crossfadeTotalSamples == 10);
    REQUIRE(snapshot.activeDelayLineCount == 1);
    REQUIRE(snapshot.activeDelayMemoryBytes > 0);
    REQUIRE(std::ranges::all_of(outputLeft, [](const float value) { return std::isfinite(value); }));
    REQUIRE(std::ranges::all_of(outputRight, [](const float value) { return std::isfinite(value); }));

    std::array<float, 2> tailInput {}; std::array<float, 2> tailLeft {}; std::array<float, 2> tailRight {};
    host.process(tailInput, tailInput, tailLeft, tailRight);
    snapshot = host.publicationSnapshot();
    REQUIRE(snapshot.crossfadeTotalSamples == 0);
    REQUIRE(std::ranges::all_of(tailLeft, [](const float value) { return std::isfinite(value); }));
}

TEST_CASE("Constructed feedback runtime reset makes repeated measurements deterministic")
{
    AcyclicRuntimeHost host;
    REQUIRE(host.compileFeedbackAndPublish(simpleFeedbackGraph(), 1'000.0, 8).valid());
    const std::array input { 0.1F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F };
    const std::array<float, 8> silence {};
    std::array<float, 8> firstLeft {}; std::array<float, 8> firstRight {};
    std::array<float, 8> secondLeft {}; std::array<float, 8> secondRight {};
    host.process(input, silence, firstLeft, firstRight);
    host.resetActiveRuntimes();
    host.process(input, silence, secondLeft, secondRight);
    REQUIRE(secondLeft == firstLeft);
    REQUIRE(secondRight == firstRight);
}

TEST_CASE("Runaway feedback is muted through a safe edit and explicit state-clearing recovery")
{
    AcyclicRuntimeHost host;
    REQUIRE(host.compileFeedbackAndPublish(simpleFeedbackGraph(), 1'000.0, 8).valid());
    reverb::dsp::NumericalSafetyGuard guard { 16.0F, 4.0F, 50.0 };
    guard.prepare(1'000.0);
    std::array<float, 8> loudInput {}; loudInput.fill(5.0F);
    std::array<float, 8> silence {}; std::array<float, 8> left {}; std::array<float, 8> right {};
    reverb::dsp::SafetyStatus violation;
    for (int block = 0; block < 16 && violation.violation == reverb::dsp::SafetyViolation::none; ++block) {
        host.process(loudInput, silence, left, right);
        violation = guard.inspectAndMute(left);
        if (violation.violation != reverb::dsp::SafetyViolation::none)
            std::ranges::fill(right, 0.0F);
    }
    REQUIRE(violation.violation == reverb::dsp::SafetyViolation::runawayLevel);
    REQUIRE(guard.isMuted());
    REQUIRE(std::ranges::all_of(left, [](const auto sample) { return sample == 0.0F; }));

    auto safeEdit = simpleFeedbackGraph();
    std::ranges::find(safeEdit.nodes, "feedback", &Node::id)->parameters[0].value = 0.25;
    REQUIRE(host.compileFeedbackAndPublish(safeEdit, 1'000.0, 8).valid());
    host.process(silence, silence, left, right);
    REQUIRE(guard.inspectAndMute(left).violation == reverb::dsp::SafetyViolation::none);
    REQUIRE(std::ranges::all_of(left, [](const auto sample) { return sample == 0.0F; }));

    host.resetActiveRuntimes();
    guard.reset();
    host.process(silence, silence, left, right);
    REQUIRE_FALSE(guard.isMuted());
    REQUIRE(std::ranges::all_of(left, [](const auto sample) { return sample == 0.0F; }));
    REQUIRE(std::ranges::all_of(right, [](const auto sample) { return sample == 0.0F; }));
}

TEST_CASE("MVP feedback compilation remains within defined time and memory budgets")
{
    GraphDocument graph; graph.nodes = { stereoInput(), stereoOutput() };
    graph.connections.push_back(cable("right", "input", "out-r", "output", "in-r"));
    for (int index = 0; index < 64; ++index) {
        std::ostringstream suffix; suffix << std::setw(2) << std::setfill('0') << index;
        const auto sum = "sum-" + suffix.str(); const auto delay = "delay-" + suffix.str();
        graph.nodes.push_back(sumNode(sum)); graph.nodes.push_back(delayNode(delay));
        graph.connections.push_back(cable("input-" + suffix.str(), "input", "out-l", sum, "in-a"));
        graph.connections.push_back(cable("return-" + suffix.str(), delay, "out", sum, "in-b"));
        graph.connections.push_back(cable("store-" + suffix.str(), sum, "out", delay, "in"));
        if (index == 0) graph.connections.push_back(cable("left", sum, "out", "output", "in-l"));
    }
    for (int index = 0; graph.nodes.size() < 256; ++index) graph.nodes.push_back(gainNode("unused-gain-" + std::to_string(index)));
    const auto compiled = compileFeedbackGraph(graph, 48'000.0, 1'024);
    REQUIRE(compiled.valid()); REQUIRE(compiled.feedbackComponents.size() == 64);
    REQUIRE(compiled.compileMicroseconds > 0); REQUIRE(compiled.compileMicroseconds < 1'000'000);
    REQUIRE(compiled.runtime->preparedStorageBytes() < 8U * 1'024U * 1'024U);
}
