#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/dsp/BarrReference.h>
#include <reverb/dsp/PitchShift.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <numbers>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace reverb::graph;

Port inputPort(std::string id = "in") { return { std::move(id), SignalType::audio, PortDirection::input }; }
Port outputPort(std::string id = "out") { return { std::move(id), SignalType::audio, PortDirection::output }; }
Port controlInputPort(std::string id) { return { std::move(id), SignalType::control, PortDirection::input }; }
Port controlOutputPort(std::string id = "out") { return { std::move(id), SignalType::control, PortDirection::output }; }
Node stereoInput() { return { "input", "stereo-input", { outputPort("out-l"), outputPort("out-r") }, {} }; }
Node stereoOutput() { return { "output", "stereo-output", { inputPort("in-l"), inputPort("in-r") }, {} }; }
Node pitchShiftNode()
{
    return { "pitch", "pitch-shift", {
        inputPort(), controlInputPort("semitones-mod"), controlInputPort("grain-mod"),
        controlInputPort("overlap-mod"), outputPort(),
    }, {
        { "semitones", 12.0, "semitones" }, { "grain", 60.0, "milliseconds" },
        { "overlap", 0.5, "normalized" }, { "direction", 0.0, "direction" },
    } };
}
Connection cable(std::string id, std::string fromNode, std::string fromPort, std::string toNode, std::string toPort)
{
    return { std::move(id), { std::move(fromNode), std::move(fromPort) }, { std::move(toNode), std::move(toPort) } };
}

GraphDocument gainSumGraph()
{
    GraphDocument graph;
    graph.nodes = {
        stereoInput(),
        { "gain", "gain", { inputPort(), outputPort() }, { { "gain", 0.25, "linear" } } },
        { "sum", "sum", { inputPort("in-a"), inputPort("in-b"), outputPort() }, {} },
        stereoOutput(),
    };
    graph.connections = {
        cable("left-gain", "input", "out-l", "gain", "in"),
        cable("gain-sum", "gain", "out", "sum", "in-a"),
        cable("right-sum", "input", "out-r", "sum", "in-b"),
        cable("sum-left", "sum", "out", "output", "in-l"),
        cable("right-right", "input", "out-r", "output", "in-r"),
    };
    return graph;
}

template <typename Predicate>
bool waitUntil(Predicate&& predicate, const std::chrono::milliseconds timeout = std::chrono::seconds(5))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return predicate();
}
}

TEST_CASE("Acyclic scheduling is deterministic across document ordering")
{
    auto first = gainSumGraph();
    auto second = first;
    std::ranges::reverse(second.nodes);
    std::ranges::reverse(second.connections);
    const auto firstResult = compileAcyclicGraph(first, 48'000.0, 64);
    const auto secondResult = compileAcyclicGraph(second, 48'000.0, 64);
    REQUIRE(firstResult.valid());
    REQUIRE(secondResult.valid());
    REQUIRE(firstResult.schedule == std::vector<std::string> { "input", "gain", "sum", "output" });
    REQUIRE(secondResult.schedule == firstResult.schedule);
}

TEST_CASE("Constructed gain and sum graph matches direct reference calculation")
{
    auto compiled = compileAcyclicGraph(gainSumGraph(), 48'000.0, 8);
    REQUIRE(compiled.valid());
    const std::array left { 1.0F, -0.5F, 0.25F, 0.0F };
    const std::array right { 0.5F, 0.25F, -0.5F, 1.0F };
    std::array<float, 4> outputLeft {}; std::array<float, 4> outputRight {};
    compiled.runtime->process(left, right, outputLeft, outputRight);
    for (std::size_t index = 0; index < left.size(); ++index) {
        REQUIRE(outputLeft[index] == left[index] * 0.25F + right[index]);
        REQUIRE(outputRight[index] == right[index]);
    }
}

TEST_CASE("Constructed delay graph matches a direct one-sample shift")
{
    GraphDocument graph;
    graph.nodes = { stereoInput(), { "delay", "delay", { inputPort(), outputPort() }, { { "delay", 1.0, "milliseconds" } } }, stereoOutput() };
    graph.connections = {
        cable("into-delay", "input", "out-l", "delay", "in"),
        cable("delay-left", "delay", "out", "output", "in-l"),
        cable("right", "input", "out-r", "output", "in-r"),
    };
    auto compiled = compileAcyclicGraph(graph, 1'000.0, 8);
    REQUIRE(compiled.valid());
    const std::array left { 1.0F, 2.0F, 3.0F, 4.0F }; const std::array right { 4.0F, 3.0F, 2.0F, 1.0F };
    std::array<float, 4> outputLeft {}; std::array<float, 4> outputRight {};
    compiled.runtime->process(left, right, outputLeft, outputRight);
    REQUIRE(outputLeft == std::array { 0.0F, 1.0F, 2.0F, 3.0F });
    REQUIRE(outputRight == right);
}

TEST_CASE("Visible pitch shift graph binds exactly to the prepared mono processor")
{
    constexpr auto sampleRate = 48'000.0;
    GraphDocument graph;
    graph.nodes = { stereoInput(), pitchShiftNode(), stereoOutput() };
    graph.connections = {
        cable("into-pitch", "input", "out-l", "pitch", "in"),
        cable("pitch-left", "pitch", "out", "output", "in-l"),
        cable("right", "input", "out-r", "output", "in-r"),
    };
    const auto frames = reverb::dsp::pitch_shift::reportedLatencySamples(sampleRate) + 24'000;
    auto compiled = compileAcyclicGraph(graph, sampleRate, frames);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.delayMemory.lineCount == 1);
    REQUIRE(compiled.delayMemory.requestedSamples
        == reverb::dsp::pitch_shift::reportedLatencySamples(sampleRate));
    REQUIRE(compiled.delayMemory.allocatedSamples
        == reverb::dsp::pitch_shift::preparedStorageSamples(sampleRate));

    std::vector<float> input(frames);
    for (std::size_t frame = 0; frame < frames; ++frame)
        input[frame] = static_cast<float>(std::sin(2.0 * std::numbers::pi * 400.0
            * static_cast<double>(frame) / sampleRate));
    std::vector<float> silence(frames);
    std::vector<float> graphLeft(frames); std::vector<float> graphRight(frames);
    compiled.runtime->process(input, silence, graphLeft, graphRight);

    reverb::dsp::PitchShift direct;
    direct.prepare(sampleRate);
    auto expected = input;
    direct.process(expected);
    REQUIRE(graphLeft == expected);
    REQUIRE(graphRight == silence);
    REQUIRE(std::ranges::any_of(
        std::span<const float>(graphLeft).subspan(compiled.runtime->delayMemoryPlan().requestedSamples),
        [](const float sample) { return sample != 0.0F; }));
}

TEST_CASE("Pitch shift phase is optional for released graphs and rejects invalid new values")
{
    auto graph = GraphDocument {};
    graph.nodes = { stereoInput(), pitchShiftNode(), stereoOutput() };
    graph.connections = {
        cable("into-pitch", "input", "out-l", "pitch", "in"),
        cable("pitch-left", "pitch", "out", "output", "in-l"),
        cable("right", "input", "out-r", "output", "in-r"),
    };
    REQUIRE(compileAcyclicGraph(graph, 48'000.0, 64).valid());

    graph.nodes[1].parameters.push_back({ "phase", 0.373, "cycles" });
    REQUIRE(compileAcyclicGraph(graph, 48'000.0, 64).valid());
    graph.nodes[1].parameters.back().value = 1.0;
    const auto invalid = compileAcyclicGraph(graph, 48'000.0, 64);
    REQUIRE_FALSE(invalid.valid());
    REQUIRE(invalid.errors.front().find("optional phase cycles") != std::string::npos);
}

TEST_CASE("Compiled constant control matches the equivalent static delay")
{
    GraphDocument modulated;
    modulated.nodes = {
        stereoInput(),
        { "lfo", "lfo", { controlOutputPort() }, {
            { "frequency", 1.0, "hertz" }, { "phase", 0.0, "cycles" },
            { "waveform", 0.0, "waveform" }, { "run-mode", 0.0, "run-mode" },
        } },
        { "map", "control-map", { controlInputPort("in"), controlOutputPort() }, {
            { "scale", 0.0, "linear" }, { "offset", 1.0, "unitless" },
            { "polarity", 1.0, "polarity" },
        } },
        { "delay", "delay", { inputPort(), controlInputPort("delay-mod"), outputPort() }, {
            { "delay", 2.0, "milliseconds", ParameterModulation {
                "delay-mod", 1.0, ModulationPolarity::bipolar, 1.0, 3.0 } },
        } },
        stereoOutput(),
    };
    modulated.connections = {
        cable("audio-in", "input", "out-l", "delay", "in"),
        cable("audio-out", "delay", "out", "output", "in-l"),
        cable("right", "input", "out-r", "output", "in-r"),
        cable("lfo-map", "lfo", "out", "map", "in"),
        cable("map-delay", "map", "out", "delay", "delay-mod"),
    };
    auto staticGraph = modulated;
    staticGraph.nodes.erase(staticGraph.nodes.begin() + 1, staticGraph.nodes.begin() + 3);
    staticGraph.nodes[1].ports.erase(staticGraph.nodes[1].ports.begin() + 1);
    staticGraph.nodes[1].parameters[0] = { "delay", 3.0, "milliseconds" };
    staticGraph.connections.erase(staticGraph.connections.begin() + 3, staticGraph.connections.end());

    auto moving = compileAcyclicGraph(modulated, 1'000.0, 16);
    auto fixed = compileAcyclicGraph(staticGraph, 1'000.0, 16);
    REQUIRE(moving.valid());
    REQUIRE(fixed.valid());
    REQUIRE(moving.delayMemory.allocatedSamples == 4);
    const std::array<float, 8> input { 1, 2, 3, 4, 5, 6, 7, 8 };
    std::array<float, 8> silence {};
    std::array<float, 8> movingLeft {}; std::array<float, 8> movingRight {};
    std::array<float, 8> fixedLeft {}; std::array<float, 8> fixedRight {};
    moving.runtime->process(input, silence, movingLeft, movingRight);
    fixed.runtime->process(input, silence, fixedLeft, fixedRight);
    REQUIRE(movingLeft == fixedLeft);
}

TEST_CASE("Macro automation is smoothed without topology compilation and reset is deterministic")
{
    GraphDocument graph;
    graph.nodes = {
        stereoInput(),
        { "macro-1", "macro", { controlOutputPort() }, {
            { "value", -1.0, "normalized" }, { "default-value", -1.0, "normalized" },
            { "center-detent", 1.0, "boolean" },
        }, "Gravity" },
        { "map", "control-map", { controlInputPort("in"), controlOutputPort() }, {
            { "scale", 1.0, "linear" }, { "offset", 0.0, "unitless" },
            { "polarity", 1.0, "polarity" },
        } },
        { "delay", "delay", { inputPort(), controlInputPort("delay-mod"), outputPort() }, {
            { "delay", 10.0, "milliseconds", ParameterModulation {
                "delay-mod", 4.0, ModulationPolarity::bipolar, 1.0, 30.0 } },
        } },
        stereoOutput(),
    };
    graph.connections = {
        cable("audio-in", "input", "out-l", "delay", "in"),
        cable("audio-out", "delay", "out", "output", "in-l"),
        cable("right", "input", "out-r", "output", "in-r"),
        cable("macro-map", "macro-1", "out", "map", "in"),
        cable("map-delay", "map", "out", "delay", "delay-mod"),
    };
    auto compiled = compileAcyclicGraph(graph, 1'000.0, 64);
    REQUIRE(compiled.valid());

    const auto renderOnset = [&](PreparedAcyclicRuntime& runtime) {
        std::array<float, 30> settle {}; std::array<float, 30> settleOut {}; std::array<float, 30> settleRight {};
        runtime.process(settle, settle, settleOut, settleRight);
        std::array<float, 40> impulse {}; impulse.front() = 1.0F;
        std::array<float, 40> silence {}; std::array<float, 40> left {}; std::array<float, 40> right {};
        runtime.process(impulse, silence, left, right);
        return static_cast<std::size_t>(std::ranges::find_if(left, [](const float value) { return value != 0.0F; }) - left.begin());
    };

    REQUIRE(renderOnset(*compiled.runtime) == 6);
    compiled.runtime->reset();
    REQUIRE(compiled.runtime->setMacroValue("macro-1", 1.0));
    REQUIRE(renderOnset(*compiled.runtime) == 14);
    compiled.runtime->reset();
    REQUIRE(renderOnset(*compiled.runtime) == 6);

    AcyclicRuntimeHost host;
    const auto revision = host.requestCompilation(graph, 1'000.0, 64, false);
    REQUIRE(waitUntil([&] { return host.publicationSnapshot().pendingRevision == revision; }));
    std::array<float, 64> silence {}; std::array<float, 64> left {}; std::array<float, 64> right {};
    host.process(silence, silence, left, right);
    const auto requestedBefore = host.publicationSnapshot().requestedRevision;
    for (int index = 0; index < 1'000; ++index)
        REQUIRE(host.setMacroValue("macro-1", index % 2 == 0 ? -1.0 : 1.0));
    host.process(silence, silence, left, right);
    REQUIRE(host.publicationSnapshot().requestedRevision == requestedBefore);
    REQUIRE(std::ranges::all_of(left, [](const float value) { return std::isfinite(value); }));
}

TEST_CASE("Visible LFO mapping drives bounded Barr-style moving diffusion")
{
    GraphDocument graph;
    graph.nodes = {
        stereoInput(),
        { "lfo", "lfo", { controlOutputPort() }, {
            { "frequency", 2.0, "hertz" }, { "phase", 0.0, "cycles" },
            { "waveform", 1.0, "waveform" }, { "run-mode", 0.0, "run-mode" },
        } },
        { "map", "control-map", { controlInputPort("in"), controlOutputPort() }, {
            { "scale", 1.0, "linear" }, { "offset", 0.0, "unitless" },
            { "polarity", 1.0, "polarity" },
        } },
        { "diffuser", "allpass", {
            inputPort(), controlInputPort("delay-mod"), controlInputPort("coefficient-mod"), outputPort(),
        }, {
            { "delay", 7.0, "milliseconds", ParameterModulation {
                "delay-mod", 3.0, ModulationPolarity::bipolar, 1.0, 12.0 } },
            { "coefficient", 0.5, "unitless", ParameterModulation {
                "coefficient-mod", 0.2, ModulationPolarity::bipolar, -0.95, 0.95 } },
        } },
        stereoOutput(),
    };
    graph.connections = {
        cable("audio-in", "input", "out-l", "diffuser", "in"),
        cable("audio-out", "diffuser", "out", "output", "in-l"),
        cable("right", "input", "out-r", "output", "in-r"),
        cable("lfo-map", "lfo", "out", "map", "in"),
        cable("map-delay", "map", "out", "diffuser", "delay-mod"),
        cable("map-coefficient", "map", "out", "diffuser", "coefficient-mod"),
    };
    auto compiled = compileAcyclicGraph(graph, 48'000.0, 4'800);
    REQUIRE(compiled.valid());
    std::vector<float> input(4'800, 0.0F); input.front() = 1.0F;
    std::vector<float> silence(input.size());
    std::vector<float> outputLeft(input.size()); std::vector<float> outputRight(input.size());
    compiled.runtime->process(input, silence, outputLeft, outputRight);
    REQUIRE(std::ranges::all_of(outputLeft, [](const float sample) { return std::isfinite(sample); }));
    REQUIRE(std::ranges::any_of(outputLeft, [](const float sample) { return sample != 0.0F; }));
}

TEST_CASE("Compiled Barr primitive graph matches its direct DSP reference")
{
    constexpr std::size_t count = 8'192;
    auto compiled = reverb::graph::compileAcyclicGraph(reverb::graph::makeBarrReferenceGraph(), 48'000.0, count);
    REQUIRE(compiled.valid());
    reverb::dsp::BarrReference direct; direct.prepare(48'000.0);
    std::vector<float> inputLeft(count, 0.0F); std::vector<float> inputRight(count, 0.0F);
    std::vector<float> compiledLeft(count); std::vector<float> compiledRight(count);
    std::vector<float> directLeft(count); std::vector<float> directRight(count);
    inputLeft.front() = 1.0F;
    compiled.runtime->process(inputLeft, inputRight, compiledLeft, compiledRight);
    direct.process(inputLeft, inputRight, directLeft, directRight);
    for (std::size_t index = 0; index < count; ++index) {
        REQUIRE(std::abs(compiledLeft[index] - directLeft[index]) < 1.0e-6F);
        REQUIRE(std::abs(compiledRight[index] - directRight[index]) < 1.0e-6F);
    }
}

TEST_CASE("Disconnected and unreachable nodes produce deterministic warnings")
{
    auto graph = gainSumGraph();
    graph.nodes.push_back({ "orphan", "gain", { inputPort(), outputPort() }, { { "gain", 1.0, "linear" } } });
    graph.nodes.push_back({ "dead-end", "gain", { inputPort(), outputPort() }, { { "gain", 1.0, "linear" } } });
    graph.connections.push_back(cable("dead-branch", "input", "out-l", "dead-end", "in"));
    const auto compiled = compileAcyclicGraph(graph, 48'000.0, 64);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.warnings == std::vector<std::string> {
        "node 'dead-end' cannot reach stereo output and is discarded",
        "disconnected node 'orphan' processes silence and its output is discarded",
    });
}

TEST_CASE("Runtime storage is prepared before bounded noexcept processing")
{
    auto compiled = compileAcyclicGraph(gainSumGraph(), 48'000.0, 4);
    REQUIRE(compiled.valid());
    const auto storage = compiled.runtime->preparedStorageBytes();
    std::array<float, 4> input {}; std::array<float, 4> outputLeft {}; std::array<float, 4> outputRight {};
    compiled.runtime->process(input, input, outputLeft, outputRight);
    REQUIRE(compiled.runtime->preparedStorageBytes() == storage);
    std::array<float, 5> oversizedInput { 1, 1, 1, 1, 1 };
    std::array<float, 5> oversizedLeft { 1, 1, 1, 1, 1 }; std::array<float, 5> oversizedRight { 1, 1, 1, 1, 1 };
    compiled.runtime->process(oversizedInput, oversizedInput, oversizedLeft, oversizedRight);
    REQUIRE(std::ranges::all_of(oversizedLeft, [](const float sample) { return sample == 0.0F; }));
    REQUIRE(std::ranges::all_of(oversizedRight, [](const float sample) { return sample == 0.0F; }));
}

TEST_CASE("Invalid compilation leaves the last valid runtime audible")
{
    AcyclicRuntimeHost host;
    const auto published = host.compileAndPublish(gainSumGraph(), 48'000.0, 8);
    REQUIRE(published.valid());
    REQUIRE(host.hasRuntime());
    auto invalid = gainSumGraph(); invalid.nodes[1].type = "unknown";
    const auto rejected = host.compileAndPublish(invalid, 48'000.0, 8);
    REQUIRE_FALSE(rejected.valid());
    const std::array left { 1.0F }; const std::array right { 0.5F };
    std::array<float, 1> outputLeft {}; std::array<float, 1> outputRight {};
    host.process(left, right, outputLeft, outputRight);
    REQUIRE(outputLeft[0] == 0.75F);
    REQUIRE(outputRight[0] == 0.5F);
}

TEST_CASE("Topology publication exposes pending active and failed revisions without replacing valid audio")
{
    AcyclicRuntimeHost host;
    const auto firstRevision = host.requestCompilation(gainSumGraph(), 48'000.0, 8, false);
    REQUIRE(waitUntil([&] { return host.publicationSnapshot().pendingRevision == firstRevision; }));
    auto snapshot = host.publicationSnapshot();
    REQUIRE(snapshot.requestedRevision == firstRevision);
    REQUIRE(snapshot.activeRevision == 0);

    const std::array left { 1.0F }; const std::array right { 0.5F };
    std::array<float, 1> outputLeft {}; std::array<float, 1> outputRight {};
    host.process(left, right, outputLeft, outputRight);
    REQUIRE(host.publicationSnapshot().activeRevision == firstRevision);
    snapshot = host.publicationSnapshot();
    REQUIRE(snapshot.activePlanDiagnostics.nodeCount == 4);
    REQUIRE(snapshot.activePlanDiagnostics.connectionCount == 5);
    REQUIRE(snapshot.activePlanDiagnostics.executionDomain == "block-wise");
    REQUIRE(snapshot.activePlanDiagnostics.estimatedScalarOperationsPerSample == 4);
    REQUIRE(snapshot.activeRequestToActiveMicroseconds > 0);
    REQUIRE(outputLeft[0] == 0.75F);

    auto invalid = gainSumGraph(); invalid.nodes[1].type = "unknown";
    const auto failedRevision = host.requestCompilation(std::move(invalid), 48'000.0, 8, false);
    REQUIRE(waitUntil([&] { return host.publicationSnapshot().failedRevision == failedRevision; }));
    snapshot = host.publicationSnapshot();
    REQUIRE(snapshot.activeRevision == firstRevision);
    REQUIRE(snapshot.failure.find("unsupported node type") != std::string::npos);
    outputLeft.fill(0.0F); outputRight.fill(0.0F);
    host.process(left, right, outputLeft, outputRight);
    REQUIRE(outputLeft[0] == 0.75F);
}

TEST_CASE("Topology changes crossfade for a fixed duration and coalesce while a transition is active")
{
    AcyclicRuntimeHost host;
    auto first = gainSumGraph();
    first.nodes[1].parameters[0].value = 0.25;
    REQUIRE(host.compileAndPublish(first, 1'000.0, 5).valid());
    const std::array<float, 5> input { 1, 1, 1, 1, 1 };
    const std::array<float, 5> silence {};
    std::array<float, 5> outputLeft {}; std::array<float, 5> outputRight {};
    host.process(input, silence, outputLeft, outputRight);
    REQUIRE(outputLeft == std::array<float, 5> { 0.25F, 0.25F, 0.25F, 0.25F, 0.25F });

    auto second = gainSumGraph(); second.nodes[1].parameters[0].value = 0.75;
    REQUIRE(host.compileAndPublish(second, 1'000.0, 5).valid());
    host.process(input, silence, outputLeft, outputRight);
    const auto transitioning = host.publicationSnapshot();
    REQUIRE(transitioning.crossfadeFromRevision == 1);
    REQUIRE(transitioning.crossfadePositionSamples == 5);
    REQUIRE(transitioning.crossfadeTotalSamples == 10);
    REQUIRE(outputLeft == std::array<float, 5> { 0.30F, 0.35F, 0.40F, 0.45F, 0.50F });

    auto third = gainSumGraph(); third.nodes[1].parameters[0].value = 0.1;
    REQUIRE(host.compileAndPublish(third, 1'000.0, 5).valid());
    auto fourth = gainSumGraph(); fourth.nodes[1].parameters[0].value = 0.9;
    REQUIRE(host.compileAndPublish(fourth, 1'000.0, 5).valid());
    auto queued = host.publicationSnapshot();
    REQUIRE(queued.activeRevision == 2);
    REQUIRE(queued.pendingRevision == 4);
    REQUIRE(queued.supersededRequests > 0);
    REQUIRE(queued.supersededCompilations > 0);

    host.process(input, silence, outputLeft, outputRight);
    REQUIRE(outputLeft == std::array<float, 5> { 0.55F, 0.60F, 0.65F, 0.70F, 0.75F });
    const auto completed = host.publicationSnapshot();
    REQUIRE(completed.crossfadeTotalSamples == 0);
    REQUIRE(completed.completedCrossfades == 1);
    REQUIRE(completed.lastCrossfadeFromRevision == 1);
    REQUIRE(completed.lastCrossfadeToRevision == 2);
    host.process(input, silence, outputLeft, outputRight);
    queued = host.publicationSnapshot();
    REQUIRE(queued.activeRevision == 4);
    REQUIRE(queued.crossfadeFromRevision == 2);
    REQUIRE(std::ranges::all_of(outputLeft, [](const float value) { return std::isfinite(value); }));
}

TEST_CASE("Rapid topology edits coalesce with bounded swaps and off-thread reclamation")
{
    AcyclicRuntimeHost host;
    REQUIRE(host.compileAndPublish(gainSumGraph(), 48'000.0, 64).valid());
    std::atomic_bool stop {};
    std::atomic_bool finite { true };
    std::atomic<std::uint64_t> blocks {};
    std::jthread audio([&](const std::stop_token token) {
        std::array<float, 64> left {}; std::array<float, 64> right {};
        std::array<float, 64> outputLeft {}; std::array<float, 64> outputRight {};
        left.front() = 0.25F; right.front() = -0.125F;
        while (!token.stop_requested() && !stop.load(std::memory_order_acquire)) {
            host.process(left, right, outputLeft, outputRight);
            if (!std::ranges::all_of(outputLeft, [](const float value) { return std::isfinite(value); })
                || !std::ranges::all_of(outputRight, [](const float value) { return std::isfinite(value); }))
                finite.store(false, std::memory_order_release);
            blocks.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::uint64_t finalRevision = 0;
    for (int index = 0; index < 1'000; ++index) {
        auto graph = gainSumGraph();
        graph.nodes[1].parameters[0].value = 0.1 + 0.0008 * static_cast<double>(index);
        finalRevision = host.requestCompilation(std::move(graph), 48'000.0, 64, false);
    }
    REQUIRE(waitUntil([&] {
        const auto state = host.publicationSnapshot();
        return state.activeRevision == finalRevision
            && state.pendingRevision == 0
            && state.crossfadeTotalSamples == 0;
    }));
    REQUIRE(waitUntil([&] { return host.publicationSnapshot().reclaimedRuntimes > 0; }));
    stop.store(true, std::memory_order_release);
    audio.request_stop();
    audio.join();
    const auto snapshot = host.publicationSnapshot();
    REQUIRE(finite.load(std::memory_order_acquire));
    REQUIRE(blocks.load(std::memory_order_acquire) > 0);
    REQUIRE(snapshot.activeRevision == finalRevision);
    REQUIRE(snapshot.supersededRequests > 0);
    REQUIRE(snapshot.completedCompilations < snapshot.requestedRevision);
    REQUIRE(snapshot.completedCrossfades < snapshot.requestedRevision);
    REQUIRE(snapshot.reclaimedRuntimes > 0);
}

TEST_CASE("Acyclic compiler reserves delay-containing cycles for feedback compilation")
{
    GraphDocument graph;
    graph.nodes = { stereoInput(), { "delay", "delay", { inputPort(), outputPort() }, { { "delay", 10.0, "milliseconds" } } }, stereoOutput() };
    graph.connections = {
        cable("self", "delay", "out", "delay", "in"),
        cable("left", "delay", "out", "output", "in-l"),
        cable("right", "input", "out-r", "output", "in-r"),
    };
    REQUIRE(validate(graph).valid());
    const auto compiled = compileAcyclicGraph(graph, 48'000.0, 64);
    REQUIRE_FALSE(compiled.valid());
    REQUIRE(compiled.errors == std::vector<std::string> {
        "acyclic compiler rejected a directed cycle; feedback compilation is provided by M3.4",
    });
}

TEST_CASE("Compiled graph latency adds serial processors and exposes output paths")
{
    GraphDocument graph;
    graph.nodes = {
        stereoInput(),
        { "delay", "delay", { inputPort(), outputPort() }, { { "delay", 10.0, "milliseconds" } } },
        pitchShiftNode(),
        stereoOutput(),
    };
    graph.connections = {
        cable("input-delay", "input", "out-l", "delay", "in"),
        cable("delay-pitch", "delay", "out", "pitch", "in"),
        cable("pitch-left", "pitch", "out", "output", "in-l"),
        cable("pitch-right", "pitch", "out", "output", "in-r"),
    };
    const auto compiled = compileAcyclicGraph(graph, 48'000.0, 64);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.latency.totalSamples == 17'762);
    REQUIRE(compiled.latency.outputPaths.size() == 2);
    REQUIRE(compiled.latency.outputPaths[0].samples == 17'762);
    REQUIRE(compiled.latency.outputPaths[0].nodeIds
        == std::vector<std::string> { "input", "delay", "pitch", "output" });
}

TEST_CASE("Prepared plan profiles actual families and execution domain off the audio thread")
{
    const auto blockPlan = compileAcyclicGraph(gainSumGraph(), 48'000.0, 64);
    REQUIRE(blockPlan.valid());
    REQUIRE(blockPlan.planDiagnostics.nodeCount == 4);
    REQUIRE(blockPlan.planDiagnostics.connectionCount == 5);
    REQUIRE(blockPlan.planDiagnostics.feedbackRegionCount == 0);
    REQUIRE(blockPlan.planDiagnostics.executionDomain == "block-wise");
    REQUIRE(blockPlan.planDiagnostics.blockWiseRegionCount == 1);
    REQUIRE(blockPlan.planDiagnostics.sampleWiseRegionCount == 0);
    REQUIRE(blockPlan.planDiagnostics.estimatedScalarOperationsPerSample == 4);
    REQUIRE(blockPlan.planDiagnostics.preparedStorageBytes == blockPlan.runtime->preparedStorageBytes());
    REQUIRE(blockPlan.planDiagnostics.compileTiming.totalMicroseconds >=
        blockPlan.planDiagnostics.compileTiming.validationMicroseconds
        + blockPlan.planDiagnostics.compileTiming.schedulingMicroseconds
        + blockPlan.planDiagnostics.compileTiming.preparationMicroseconds);
    REQUIRE(std::ranges::find(blockPlan.planDiagnostics.workloadFamilies, "gain", &WorkloadFamily::family)
        != blockPlan.planDiagnostics.workloadFamilies.end());

    GraphDocument feedback;
    feedback.nodes = {
        stereoInput(),
        { "join", "sum", { inputPort("in-a"), inputPort("in-b"), outputPort() }, {} },
        { "delay", "delay", { inputPort(), outputPort() }, { { "delay", 10.0, "milliseconds" } } },
        stereoOutput(),
    };
    feedback.connections = {
        cable("input-join", "input", "out-l", "join", "in-a"),
        cable("delay-return", "delay", "out", "join", "in-b"),
        cable("join-delay", "join", "out", "delay", "in"),
        cable("left", "join", "out", "output", "in-l"),
        cable("right", "join", "out", "output", "in-r"),
    };
    const auto samplePlan = compileFeedbackGraph(feedback, 48'000.0, 64);
    REQUIRE(samplePlan.valid());
    REQUIRE(samplePlan.planDiagnostics.feedbackRegionCount == 1);
    REQUIRE(samplePlan.planDiagnostics.executionDomain == "hybrid");
    REQUIRE(samplePlan.planDiagnostics.blockWiseRegionCount == 2);
    REQUIRE(samplePlan.planDiagnostics.sampleWiseRegionCount == 1);
    const auto dispatch = std::ranges::find(
        samplePlan.planDiagnostics.workloadFamilies, "sample-wise-dispatch", &WorkloadFamily::family);
    REQUIRE(dispatch != samplePlan.planDiagnostics.workloadFamilies.end());
    REQUIRE(dispatch->nodeCount == 2);
}

TEST_CASE("Compiled graph latency reports parallel maxima and uncompensated differences")
{
    GraphDocument graph;
    graph.nodes = {
        stereoInput(),
        { "short", "delay", { inputPort(), outputPort() }, { { "delay", 10.0, "milliseconds" } } },
        { "long", "delay", { inputPort(), outputPort() }, { { "delay", 25.0, "milliseconds" } } },
        { "join", "sum", { inputPort("in-a"), inputPort("in-b"), outputPort() }, {} },
        stereoOutput(),
    };
    graph.connections = {
        cable("input-short", "input", "out-l", "short", "in"),
        cable("input-long", "input", "out-r", "long", "in"),
        cable("short-join", "short", "out", "join", "in-a"),
        cable("long-join", "long", "out", "join", "in-b"),
        cable("join-left", "join", "out", "output", "in-l"),
        cable("join-right", "join", "out", "output", "in-r"),
    };
    const auto compiled = compileAcyclicGraph(graph, 1'000.0, 64);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.latency.totalSamples == 25);
    const auto join = std::ranges::find(compiled.latency.parallelJoins, "join", &LatencyJoin::nodeId);
    REQUIRE(join != compiled.latency.parallelJoins.end());
    REQUIRE(join->minimumInputSamples == 10);
    REQUIRE(join->maximumInputSamples == 25);
    REQUIRE(join->uncompensatedSamples() == 15);
}

TEST_CASE("Compiled graph latency crosses a feedback Delay only once")
{
    GraphDocument graph;
    graph.nodes = {
        stereoInput(),
        { "join", "sum", { inputPort("in-a"), inputPort("in-b"), outputPort() }, {} },
        { "delay", "delay", { inputPort(), outputPort() }, { { "delay", 10.0, "milliseconds" } } },
        stereoOutput(),
    };
    graph.connections = {
        cable("input-join", "input", "out-l", "join", "in-a"),
        cable("delay-return", "delay", "out", "join", "in-b"),
        cable("join-delay", "join", "out", "delay", "in"),
        cable("join-left", "join", "out", "output", "in-l"),
        cable("join-right", "join", "out", "output", "in-r"),
    };
    const auto compiled = compileFeedbackGraph(graph, 1'000.0, 64);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.latency.totalSamples == 10);
    REQUIRE(compiled.latency.parallelJoins.front().uncompensatedSamples() == 10);
}
