#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/GraphDocument.h>
#include <reverb/graph/PatchJson.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string readFixture(const std::filesystem::path& relativePath)
{
    const auto path = std::filesystem::path { REVERB_TEST_FIXTURES_DIR } / relativePath;
    std::ifstream stream(path, std::ios::binary);
    REQUIRE(stream.good());
    return { std::istreambuf_iterator<char> { stream }, std::istreambuf_iterator<char> {} };
}

const reverb::graph::Node& findNode(const reverb::graph::GraphDocument& document, const std::string& id)
{
    for (const auto& node : document.nodes) {
        if (node.id == id)
            return node;
    }
    throw std::runtime_error("Node not found: " + id);
}

} // namespace

TEST_CASE("Valid patch preserves semantic graph, layout, stable IDs, and milliseconds")
{
    const auto original = reverb::graph::parsePatchJson(readFixture("patches/valid/barr-minimal.json"));

    REQUIRE(reverb::graph::validate(original).valid());
    REQUIRE(original.nodes.size() == 4);
    REQUIRE(original.connections.size() == 5);
    REQUIRE(original.layout.nodes.size() == 4);

    const auto& input = findNode(original, "input");
    REQUIRE((input.ports == std::vector<reverb::graph::Port> {
        { .id = "out-l", .signal = reverb::graph::SignalType::audio, .direction = reverb::graph::PortDirection::output },
        { .id = "out-r", .signal = reverb::graph::SignalType::audio, .direction = reverb::graph::PortDirection::output },
    }));

    const auto& output = findNode(original, "output");
    REQUIRE((output.ports == std::vector<reverb::graph::Port> {
        { .id = "in-l", .signal = reverb::graph::SignalType::audio, .direction = reverb::graph::PortDirection::input },
        { .id = "in-r", .signal = reverb::graph::SignalType::audio, .direction = reverb::graph::PortDirection::input },
    }));

    const auto& allpass = findNode(original, "allpass-1");
    REQUIRE(allpass.parameters.front().id == "delay");
    REQUIRE(allpass.parameters.front().value == 13.725);
    REQUIRE(allpass.parameters.front().unit == "milliseconds");
    REQUIRE(allpass.ports.back().signal == reverb::graph::SignalType::control);
    REQUIRE(allpass.parameters.front().modulation.has_value());
    REQUIRE(allpass.parameters.front().modulation->portId == "delay-mod");
    REQUIRE(allpass.parameters.front().modulation->amount == 2.0);
    REQUIRE(allpass.parameters.front().modulation->polarity == reverb::graph::ModulationPolarity::bipolar);
    REQUIRE(allpass.parameters.front().modulation->clampMinimum == 0.1);
    REQUIRE(allpass.parameters.front().modulation->clampMaximum == 100.0);

    const auto written = reverb::graph::writePatchJson(original);
    const auto roundTripped = reverb::graph::parsePatchJson(written);
    REQUIRE(roundTripped == original);
    REQUIRE(reverb::graph::writePatchJson(roundTripped) == written);
}

TEST_CASE("Validator rejects an audio-control type mismatch")
{
    const auto document = reverb::graph::parsePatchJson(
        readFixture("patches/invalid/audio-control-connection.json"));
    const auto result = reverb::graph::validate(document);

    REQUIRE_FALSE(result.valid());
    REQUIRE(result.errors.size() == 1);
    REQUIRE(result.errors.front() == "connection 'invalid-control-to-audio' mixes audio and control signals");
}

TEST_CASE("Patch schema is valid JSON Schema metadata")
{
    for (const auto version : { 1, 2 }) {
        const auto schemaPath = std::filesystem::path { REVERB_TEST_FIXTURES_DIR }
            / ".." / ".." / "schemas" / ("patch-v" + std::to_string(version) + ".schema.json");
        std::ifstream stream(schemaPath, std::ios::binary);
        REQUIRE(stream.good());

        const auto schema = nlohmann::json::parse(stream);
        REQUIRE(schema.at("$schema") == "https://json-schema.org/draft/2020-12/schema");
        REQUIRE(schema.at("properties").at("schemaVersion").at("const") == version);
        REQUIRE(schema.at("required").size() == 4);
    }
}

TEST_CASE("Validator rejects zero-delay cycles and accepts delayed feedback")
{
    using namespace reverb::graph;
    const Port input { "in", SignalType::audio, PortDirection::input };
    const Port output { "out", SignalType::audio, PortDirection::output };

    GraphDocument graph;
    graph.nodes = {
        Node { "sum", "sum", { input, output }, {} },
        Node { "gain", "gain", { input, output }, {} },
    };
    graph.connections = {
        Connection { "a", { "sum", "out" }, { "gain", "in" } },
        Connection { "b", { "gain", "out" }, { "sum", "in" } },
    };

    const auto invalid = validate(graph);
    REQUIRE_FALSE(invalid.valid());
    REQUIRE(std::ranges::any_of(invalid.errors, [](const auto& error) {
        return error.find("explicit delay") != std::string::npos;
    }));

    graph.nodes[1].type = "delay";
    REQUIRE(validate(graph).valid());
}
