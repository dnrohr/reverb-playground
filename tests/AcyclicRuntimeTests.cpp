#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/dsp/BarrReference.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
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
    REQUIRE(waitUntil([&] { return host.publicationSnapshot().activeRevision == finalRevision; }));
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
