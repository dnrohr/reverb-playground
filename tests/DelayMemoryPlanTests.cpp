#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/AcyclicRuntime.h>

#include <array>
#include <string>

namespace {
using namespace reverb::graph;

Port inputPort(std::string id = "in") { return { std::move(id), SignalType::audio, PortDirection::input }; }
Port outputPort(std::string id = "out") { return { std::move(id), SignalType::audio, PortDirection::output }; }
Node stereoInput() { return { "input", "stereo-input", { outputPort("out-l"), outputPort("out-r") }, {} }; }
Node stereoOutput() { return { "output", "stereo-output", { inputPort("in-l"), inputPort("in-r") }, {} }; }
Connection cable(std::string id, std::string fromNode, std::string fromPort, std::string toNode, std::string toPort)
{
    return { std::move(id), { std::move(fromNode), std::move(fromPort) },
        { std::move(toNode), std::move(toPort) } };
}

GraphDocument graphWithDelay(const double milliseconds, const std::string& type = "delay")
{
    GraphDocument graph;
    auto parameters = std::vector<Parameter> { { "delay", milliseconds, "milliseconds" } };
    if (type == "allpass") parameters.push_back({ "coefficient", 0.5, "unitless" });
    graph.nodes = {
        stereoInput(), { "time", type, { inputPort(), outputPort() }, std::move(parameters) }, stereoOutput(),
    };
    graph.connections = {
        cable("in-time", "input", "out-l", "time", "in"),
        cable("time-left", "time", "out", "output", "in-l"),
        cable("right", "input", "out-r", "output", "in-r"),
    };
    return graph;
}

GraphDocument nineMaximumDelays()
{
    GraphDocument graph;
    graph.nodes = { stereoInput(), stereoOutput() };
    graph.connections = {
        cable("left", "input", "out-l", "output", "in-l"),
        cable("right", "input", "out-r", "output", "in-r"),
    };
    for (int index = 0; index < 9; ++index) {
        graph.nodes.push_back({ "delay-" + std::to_string(index), "delay",
            { inputPort(), outputPort() }, { { "delay", 10'000.0, "milliseconds" } } });
    }
    return graph;
}
}

TEST_CASE("Delay memory planning reports zero, minimum, maximum, and allpass allocation")
{
    auto noDelay = graphWithDelay(1.0);
    noDelay.nodes.erase(noDelay.nodes.begin() + 1);
    noDelay.connections = {
        cable("left", "input", "out-l", "output", "in-l"),
        cable("right", "input", "out-r", "output", "in-r"),
    };
    const auto zero = compileAcyclicGraph(noDelay, 48'000.0, 64);
    REQUIRE(zero.valid());
    REQUIRE(zero.delayMemory.lineCount == 0);
    REQUIRE(zero.delayMemory.requestedBytes == 0);
    REQUIRE(zero.delayMemory.allocatedBytes == 0);

    const auto minimum = compileAcyclicGraph(graphWithDelay(0.001), 48'000.0, 64);
    REQUIRE(minimum.valid());
    REQUIRE(minimum.delayMemory.requestedSamples == 1);
    REQUIRE(minimum.delayMemory.allocatedSamples == 1);
    REQUIRE(minimum.runtime->delayMemoryPlan().allocatedBytes == sizeof(float));

    const auto maximum = compileAcyclicGraph(graphWithDelay(10'000.0), 1'000.0, 64);
    REQUIRE(maximum.valid());
    REQUIRE(maximum.delayMemory.requestedSamples == 10'000);
    REQUIRE(maximum.delayMemory.allocatedSamples == 10'000);

    const auto allpass = compileAcyclicGraph(graphWithDelay(10.0, "allpass"), 48'000.0, 64);
    REQUIRE(allpass.valid());
    REQUIRE(allpass.delayMemory.requestedSamples == 480);
    REQUIRE(allpass.delayMemory.allocatedSamples == 4'801);
    REQUIRE(allpass.delayMemory.allocatedBytes > allpass.delayMemory.requestedBytes);
}

TEST_CASE("Zero and over-limit delay boundaries fail before runtime preparation")
{
    const auto zero = compileAcyclicGraph(graphWithDelay(0.0), 48'000.0, 64);
    REQUIRE_FALSE(zero.valid());
    REQUIRE(zero.errors == std::vector<std::string> {
        "node 'time' delay must be greater than zero milliseconds",
    });

    const auto overLimit = compileAcyclicGraph(graphWithDelay(10'000.001), 48'000.0, 64);
    REQUIRE_FALSE(overLimit.valid());
    REQUIRE(overLimit.errors == std::vector<std::string> {
        "node 'time' exceeds the 10-second delay limit",
    });
}

TEST_CASE("Over-budget plans report exact totals and fail before publication")
{
    const auto graph = nineMaximumDelays();
    const auto compiled = compileAcyclicGraph(graph, 192'000.0, 64);
    REQUIRE_FALSE(compiled.valid());
    REQUIRE(compiled.runtime == nullptr);
    REQUIRE(compiled.delayMemory.lineCount == 9);
    REQUIRE(compiled.delayMemory.requestedBytes == 69'120'000);
    REQUIRE(compiled.delayMemory.allocatedBytes == 69'120'000);
    REQUIRE(compiled.delayMemory.budgetBytes == delayMemoryBudgetBytes);
    REQUIRE_FALSE(compiled.delayMemory.withinBudget());
    REQUIRE(compiled.errors == std::vector<std::string> {
        "patch requires 69120000 bytes of delay memory; project budget is 67108864 bytes",
    });
}

TEST_CASE("Sample-rate memory recalculation cannot replace the active runtime when over budget")
{
    const auto graph = nineMaximumDelays();
    AcyclicRuntimeHost host;
    const auto at44k = host.compileAndPublish(graph, 44'100.0, 8);
    REQUIRE(at44k.valid());
    REQUIRE(at44k.delayMemory.allocatedBytes == 15'876'000);
    REQUIRE(host.hasRuntime());

    const auto at192k = host.compileAndPublish(graph, 192'000.0, 8);
    REQUIRE_FALSE(at192k.valid());
    REQUIRE(at192k.delayMemory.allocatedBytes == 69'120'000);
    REQUIRE(host.hasRuntime());

    const std::array left { 0.25F, -0.5F }; const std::array right { -0.75F, 1.0F };
    std::array<float, 2> outputLeft {}; std::array<float, 2> outputRight {};
    host.process(left, right, outputLeft, outputRight);
    REQUIRE(outputLeft == left);
    REQUIRE(outputRight == right);
}

TEST_CASE("Prepared delay arena remains fixed throughout processing")
{
    auto compiled = compileAcyclicGraph(graphWithDelay(5.0), 48'000.0, 16);
    REQUIRE(compiled.valid());
    const auto plan = compiled.runtime->delayMemoryPlan();
    const auto storage = compiled.runtime->preparedStorageBytes();
    std::array<float, 16> input {}; std::array<float, 16> left {}; std::array<float, 16> right {};
    for (int block = 0; block < 1'000; ++block) compiled.runtime->process(input, input, left, right);
    REQUIRE(compiled.runtime->delayMemoryPlan().allocatedBytes == plan.allocatedBytes);
    REQUIRE(compiled.runtime->preparedStorageBytes() == storage);
}
