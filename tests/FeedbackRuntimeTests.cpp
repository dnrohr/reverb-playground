#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/dsp/NumericalSafetyGuard.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
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
