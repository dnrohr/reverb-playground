#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/NumericalSafetyGuard.h>
#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/PatchJson.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace {
using namespace reverb::graph;
Port audioIn(std::string id = "in") { return { std::move(id), SignalType::audio, PortDirection::input }; }
Port audioOut(std::string id = "out") { return { std::move(id), SignalType::audio, PortDirection::output }; }
Port controlIn(std::string id) { return { std::move(id), SignalType::control, PortDirection::input }; }
Port controlOut(std::string id = "out") { return { std::move(id), SignalType::control, PortDirection::output }; }
Connection cable(std::string id, std::string fromNode, std::string fromPort, std::string toNode, std::string toPort)
{ return { std::move(id), { std::move(fromNode), std::move(fromPort) }, { std::move(toNode), std::move(toPort) } }; }
Node stereoInput() { return { "input", "stereo-input", { audioOut("out-l"), audioOut("out-r") }, {} }; }
Node stereoOutput() { return { "output", "stereo-output", { audioIn("in-l"), audioIn("in-r") }, {} }; }
Node follower() { return { "follower", "envelope-follower", { audioIn(), controlOut() }, {
    { "attack", 0.1, "milliseconds" }, { "release", 1.0, "milliseconds" },
} }; }
Node gate() { return { "gate", "hold-gate", { audioIn(), controlIn("gate"), audioOut() }, {
    { "threshold", 0.5, "unitless" }, { "attack", 1.0, "milliseconds" },
    { "hold", 3.0, "milliseconds" }, { "release", 2.0, "milliseconds" },
} }; }

GraphDocument feedForwardGateGraph()
{
    GraphDocument graph; graph.nodes = { stereoInput(), follower(), gate(), stereoOutput() };
    graph.connections = {
        cable("trigger", "input", "out-l", "follower", "in"),
        cable("audio", "input", "out-r", "gate", "in"),
        cable("control", "follower", "out", "gate", "gate"),
        cable("left", "gate", "out", "output", "in-l"),
        cable("right", "input", "out-r", "output", "in-r"),
    };
    return graph;
}
}

TEST_CASE("Compiled follower and hold gate preserve exact visible signal semantics")
{
    auto compiled = compileFeedbackGraph(feedForwardGateGraph(), 1'000.0, 8);
    REQUIRE(compiled.valid());
    const std::array trigger { 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F };
    std::array<float, 8> audio {}; audio.fill(1.0F);
    std::array<float, 8> left {}; std::array<float, 8> right {};
    compiled.runtime->process(trigger, audio, left, right);
    REQUIRE(left == std::array { 1.0F, 1.0F, 1.0F, 1.0F, 0.5F, 0.0F, 0.0F, 0.0F });
    REQUIRE(right == audio);

    compiled.runtime->reset();
    std::array<float, 8> repeatedLeft {}; std::array<float, 8> repeatedRight {};
    compiled.runtime->process(trigger, audio, repeatedLeft, repeatedRight);
    REQUIRE(repeatedLeft == left);
    REQUIRE(repeatedRight == right);
}

TEST_CASE("Follower and hold gate persist without hidden modulation mappings")
{
    const auto graph = feedForwardGateGraph();
    const auto json = writePatchJson(graph);
    REQUIRE(json.find("envelope-follower") != std::string::npos);
    REQUIRE(json.find("hold-gate") != std::string::npos);
    REQUIRE(json.find("modulation") == std::string::npos);
    REQUIRE(parsePatchJson(json) == graph);
}

TEST_CASE("Hold gate accepts one explicit mapper after the envelope follower")
{
    auto graph = feedForwardGateGraph();
    graph.nodes.insert(graph.nodes.end() - 1, { "map", "control-map", {
        controlIn("in"), controlOut(),
    }, { { "scale", 1.0, "linear" }, { "offset", 0.0, "unitless" }, { "polarity", 0.0, "polarity" } } });
    std::ranges::find(graph.connections, "control", &Connection::id)->to = { "map", "in" };
    graph.connections.push_back(cable("mapped-control", "map", "out", "gate", "gate"));
    auto compiled = compileFeedbackGraph(graph, 1'000.0, 8);
    REQUIRE(compiled.valid());
    const std::array trigger { 1.0F, 0.0F, 0.0F, 0.0F };
    std::array<float, 4> audio {}; audio.fill(0.25F);
    std::array<float, 4> left {}; std::array<float, 4> right {};
    compiled.runtime->process(trigger, audio, left, right);
    REQUIRE(left == audio);
}

TEST_CASE("Unsupported hold-gate control sources fail before publication")
{
    auto graph = feedForwardGateGraph();
    graph.nodes.push_back({ "lfo", "lfo", { controlOut() }, {
        { "frequency", 1.0, "hertz" }, { "phase", 0.0, "cycles" },
        { "waveform", 0.0, "waveform" }, { "run-mode", 0.0, "run-mode" },
    } });
    auto& control = *std::ranges::find(graph.connections, "control", &Connection::id);
    control.from = { "lfo", "out" };
    const auto compiled = compileFeedbackGraph(graph, 48'000.0, 64);
    REQUIRE_FALSE(compiled.valid());
    REQUIRE(std::ranges::any_of(compiled.errors, [](const auto& error) {
        return error.find("control must come from envelope-follower") != std::string::npos;
    }));
}

TEST_CASE("Envelope gate mapper rejects nested parameter modulation instead of ignoring it")
{
    auto graph = feedForwardGateGraph();
    graph.nodes.insert(graph.nodes.end() - 1, { "map", "control-map", {
        controlIn("in"), controlIn("scale-mod"), controlIn("offset-mod"),
        controlIn("polarity-mod"), controlOut(),
    }, {
        { "scale", 1.0, "linear", ParameterModulation { "scale-mod", 1.0,
            ModulationPolarity::bipolar, -4.0, 4.0 } },
        { "offset", 0.0, "unitless" }, { "polarity", 0.0, "polarity" },
    } });
    graph.nodes.push_back({ "lfo", "lfo", { controlOut() }, {
        { "frequency", 1.0, "hertz" }, { "phase", 0.0, "cycles" },
        { "waveform", 0.0, "waveform" }, { "run-mode", 0.0, "run-mode" },
    } });
    std::ranges::find(graph.connections, "control", &Connection::id)->to = { "map", "in" };
    graph.connections.push_back(cable("mapped-control", "map", "out", "gate", "gate"));
    graph.connections.push_back(cable("nested-modulation", "lfo", "out", "map", "scale-mod"));
    const auto compiled = compileFeedbackGraph(graph, 48'000.0, 64);
    REQUIRE_FALSE(compiled.valid());
    REQUIRE(std::ranges::any_of(compiled.errors, [](const auto& error) {
        return error.find("uses base scale/offset/polarity only") != std::string::npos;
    }));
}

TEST_CASE("Hold gate inside delayed feedback remains finite and cannot create energy")
{
    auto graph = feedForwardGateGraph();
    graph.nodes.insert(graph.nodes.end() - 1, {
        "sum", "sum", { audioIn("in-a"), audioIn("in-b"), audioOut() }, {},
    });
    graph.nodes.insert(graph.nodes.end() - 1, {
        "delay", "delay", { audioIn(), audioOut() }, { { "delay", 1.0, "milliseconds" } },
    });
    std::ranges::find(graph.connections, "audio", &Connection::id)->to = { "sum", "in-a" };
    graph.connections.push_back(cable("feedback", "delay", "out", "sum", "in-b"));
    graph.connections.push_back(cable("sum-gate", "sum", "out", "gate", "in"));
    graph.connections.push_back(cable("gate-delay", "gate", "out", "delay", "in"));
    auto compiled = compileFeedbackGraph(graph, 1'000.0, 64);
    REQUIRE(compiled.valid());
    std::array<float, 64> trigger {}; trigger.fill(1.0F);
    std::array<float, 64> input {}; input.front() = 0.25F;
    std::array<float, 64> left {}; std::array<float, 64> right {};
    compiled.runtime->process(trigger, input, left, right);
    REQUIRE(std::ranges::all_of(left, [](const auto sample) { return std::isfinite(sample); }));
    REQUIRE(std::ranges::all_of(left, [](const auto sample) { return std::abs(sample) <= 0.25F; }));
    reverb::dsp::NumericalSafetyGuard safety;
    safety.prepare(1'000.0);
    REQUIRE(safety.inspectAndMute(left).violation == reverb::dsp::SafetyViolation::none);
}
