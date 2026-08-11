#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/BarrReferenceRuntime.h>
#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/RuntimeSnapshot.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

TEST_CASE("Barr UI snapshot is generated from the DSP runtime identity")
{
    const auto graph = reverb::graph::makeBarrReferenceGraph();
    REQUIRE(reverb::graph::validateBarrRuntimeIdentity(graph).empty());
    REQUIRE(graph.nodes.size() == reverb::dsp::barrReferenceRuntimeNodes().size());
    REQUIRE(graph.connections.size() == reverb::dsp::barrReferenceRuntimeConnections().size());

    const auto json = nlohmann::json::parse(reverb::graph::writeBarrRuntimeSnapshotJson(48'000.0));
    REQUIRE(json.at("contractVersion") == reverb::graph::barrRuntimeContractVersion);
    REQUIRE(json.at("engineId") == "barr-reference");
    REQUIRE(json.at("sampleRate") == 48'000.0);
    REQUIRE(json.at("nodes").size() == graph.nodes.size());
    REQUIRE(json.at("connections").size() == graph.connections.size());

    const auto tank = std::ranges::find_if(json.at("nodes"), [](const auto& node) {
        return node.at("id") == "tank-2";
    });
    REQUIRE(tank != json.at("nodes").end());
    REQUIRE(tank->at("parameters").at(0).at("value") == 19.91);
    REQUIRE(tank->at("parameters").at(0).at("unit") == "milliseconds");
    REQUIRE(tank->at("parameters").at(0).at("modulation").at("portId") == "delay-mod");
    REQUIRE(tank->at("parameters").at(0).at("modulation").at("amount") == 2.0);
    REQUIRE(tank->at("parameters").at(0).at("modulation").at("polarity") == "bipolar");
    REQUIRE(tank->at("parameters").at(0).at("modulation").at("clampMinimum") == 0.1);
    REQUIRE(tank->at("parameters").at(0).at("modulation").at("clampMaximum") == 100.0);
    REQUIRE(json.at("outsidePatch").size() == 2);
}

TEST_CASE("Barr runtime identity validator reports UI drift")
{
    auto graph = reverb::graph::makeBarrReferenceGraph();
    graph.nodes[3].id = "renamed-diffuser";
    graph.nodes[4].parameters[0].value = 99.0;
    graph.connections.pop_back();

    const auto errors = reverb::graph::validateBarrRuntimeIdentity(graph);
    REQUIRE(errors.size() >= 3);
    REQUIRE(std::ranges::any_of(errors, [](const auto& error) {
        return error.find("diffuser-1") != std::string::npos;
    }));
    REQUIRE(std::ranges::any_of(errors, [](const auto& error) {
        return error.find("diffuser-2.delay") != std::string::npos;
    }));
    REQUIRE(std::ranges::any_of(errors, [](const auto& error) {
        return error.find("connection count") != std::string::npos;
    }));
}
