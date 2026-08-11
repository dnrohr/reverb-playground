#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/ControlRate.h>
#include <reverb/graph/PatchJson.h>

#include <nlohmann/json.hpp>

#include <limits>
#include <ranges>
#include <string>

namespace {

reverb::graph::GraphDocument mappedGraph()
{
    using namespace reverb::graph;
    GraphDocument graph;
    graph.nodes = {
        { "source", "control-source", { { "out", SignalType::control, PortDirection::output } }, {} },
        { "gain", "gain",
            {
                { "in", SignalType::audio, PortDirection::input },
                { "out", SignalType::audio, PortDirection::output },
                { "gain-mod", SignalType::control, PortDirection::input },
            },
            { { "gain", 0.5, "linear", ParameterModulation {
                "gain-mod", 0.25, ModulationPolarity::bipolar, 0.0, 1.0 } } } },
    };
    graph.connections = { { "mod", { "source", "out" }, { "gain", "gain-mod" } } };
    return graph;
}

} // namespace

TEST_CASE("Control-rate plan compiles exact base-plus-modulation mappings with bounded work")
{
    const auto plan = reverb::graph::compileControlRatePlan(mappedGraph(), 48'000.0, 512);
    REQUIRE(plan.valid());
    REQUIRE(plan.quantumSamples == 48);
    REQUIRE(plan.maximumTicksPerBlock == 11);
    REQUIRE(plan.maximumMappingEvaluationsPerBlock == 11);
    REQUIRE(plan.mappings.size() == 1);
    const auto& mapping = plan.mappings.front();
    REQUIRE(mapping.sourceNodeId == "source");
    REQUIRE(mapping.targetNodeId == "gain");
    REQUIRE(mapping.parameterId == "gain");
    REQUIRE(reverb::graph::mappedParameterValue(mapping, -1.0) == 0.25);
    REQUIRE(reverb::graph::mappedParameterValue(mapping, 1.0) == 0.75);
    REQUIRE(reverb::graph::mappedParameterValue(mapping, 100.0) == 0.75);
    REQUIRE(reverb::graph::mappedParameterValue(mapping, std::numeric_limits<double>::infinity()) == 0.5);
}

TEST_CASE("Unipolar mapping clamps before applying amount and parameter bounds")
{
    auto graph = mappedGraph();
    auto& mapping = *graph.nodes[1].parameters[0].modulation;
    mapping.polarity = reverb::graph::ModulationPolarity::unipolar;
    mapping.amount = 2.0;
    const auto plan = reverb::graph::compileControlRatePlan(graph, 44'100.0, 64);
    REQUIRE(plan.valid());
    REQUIRE(plan.quantumSamples == 45);
    REQUIRE(reverb::graph::mappedParameterValue(plan.mappings.front(), -1.0) == 0.5);
    REQUIRE(reverb::graph::mappedParameterValue(plan.mappings.front(), 0.25) == 1.0);
}

TEST_CASE("Control interpolation reaches each tick target without a sample step")
{
    reverb::graph::ControlRamp ramp;
    ramp.reset(0.25);
    ramp.setTarget(0.75, 4);
    REQUIRE(ramp.next() == Catch::Approx(0.375));
    REQUIRE(ramp.next() == Catch::Approx(0.5));
    REQUIRE(ramp.next() == Catch::Approx(0.625));
    REQUIRE(ramp.next() == Catch::Approx(0.75));
    REQUIRE(ramp.next() == Catch::Approx(0.75));
}

TEST_CASE("Patch schema v2 preserves every modulation mapping field exactly")
{
    const auto original = mappedGraph();
    const auto jsonText = reverb::graph::writePatchJson(original);
    const auto json = nlohmann::json::parse(jsonText);
    const auto& mapping = json.at("semantic").at("nodes").at(1)
        .at("parameters").at(0).at("modulation");
    REQUIRE(json.at("schemaVersion") == 2);
    REQUIRE(mapping.at("portId") == "gain-mod");
    REQUIRE(mapping.at("amount") == 0.25);
    REQUIRE(mapping.at("polarity") == "bipolar");
    REQUIRE(mapping.at("clampMinimum") == 0.0);
    REQUIRE(mapping.at("clampMaximum") == 1.0);
    REQUIRE(reverb::graph::parsePatchJson(jsonText) == original);
}

TEST_CASE("Control plan rejects graphs beyond the fixed mapping budget")
{
    using namespace reverb::graph;
    auto graph = mappedGraph();
    graph.connections.clear();
    graph.nodes.resize(1);
    for (std::size_t index = 0; index <= maximumControlMappings; ++index) {
        const auto id = "target-" + std::to_string(index);
        graph.nodes.push_back({
            id,
            "gain",
            { { "mod", SignalType::control, PortDirection::input } },
            { { "gain", 0.5, "linear", ParameterModulation {
                "mod", 0.25, ModulationPolarity::bipolar, 0.0, 1.0 } } },
        });
        graph.connections.push_back({
            "c-" + std::to_string(index), { "source", "out" }, { id, "mod" } });
    }
    const auto plan = compileControlRatePlan(graph, 48'000.0, 512);
    REQUIRE_FALSE(plan.valid());
    REQUIRE(std::ranges::find(plan.errors, "control graph exceeds 128 parameter mappings") != plan.errors.end());
}
