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
        if (version == 2)
            REQUIRE(schema.at("properties").at("qualityPolicy").at("enum")
                == nlohmann::json::array({ "draft", "normal", "high" }));
    }
}

TEST_CASE("Patch quality policy defaults safely and round trips explicitly")
{
    using namespace reverb::graph;
    const auto legacy = parsePatchJson(readFixture("patches/valid/barr-minimal.json"));
    REQUIRE(legacy.qualityPolicy == QualityPolicy::normal);

    auto high = legacy;
    high.qualityPolicy = QualityPolicy::high;
    const auto written = writePatchJson(high);
    auto json = nlohmann::json::parse(written);
    REQUIRE(json.at("qualityPolicy") == "high");
    REQUIRE(parsePatchJson(written) == high);

    json["qualityPolicy"] = "unbounded";
    REQUIRE_THROWS(parsePatchJson(json.dump()));
}

TEST_CASE("Every released schema version migrates to deterministic schema v2")
{
    REQUIRE(reverb::graph::GraphDocument::oldestReadableSchemaVersion == 1);
    REQUIRE(reverb::graph::GraphDocument::schemaVersion == 2);
    const auto versionOne = reverb::graph::parsePatchJson(
        readFixture("patches/valid/schema-v1-migration.json"));
    REQUIRE(reverb::graph::validate(versionOne).valid());
    const auto& gain = findNode(versionOne, "legacy-gain");
    REQUIRE(gain.ports.size() == 3);
    const reverb::graph::Port expectedPort {
        .id = "gain-mod",
        .signal = reverb::graph::SignalType::control,
        .direction = reverb::graph::PortDirection::input,
    };
    REQUIRE(gain.ports[2] == expectedPort);
    REQUIRE(gain.parameters.front().value == 0.375);
    const reverb::graph::ParameterModulation expectedMapping {
        .portId = "gain-mod",
        .amount = 0.5,
        .polarity = reverb::graph::ModulationPolarity::bipolar,
        .clampMinimum = -1.0,
        .clampMaximum = 1.0,
    };
    REQUIRE(gain.parameters.front().modulation == expectedMapping);

    const auto migrated = reverb::graph::writePatchJson(versionOne);
    const auto json = nlohmann::json::parse(migrated);
    REQUIRE(json.at("schemaVersion") == 2);
    const auto versionTwo = reverb::graph::parsePatchJson(migrated);
    REQUIRE(versionTwo == versionOne);
    REQUIRE(reverb::graph::writePatchJson(versionTwo) == migrated);
}

TEST_CASE("Legacy schema-v2 Pitch Shift values migrate visibly into one octave")
{
    auto json = nlohmann::json::parse(readFixture("../../factory-patches/safe-parallel-shimmer.rvp.json"));
    auto& nodes = json.at("semantic").at("nodes");
    const auto pitch = std::ranges::find_if(nodes, [](const auto& node) { return node.at("type") == "pitch-shift"; });
    REQUIRE(pitch != nodes.end());
    auto& semitones = pitch->at("parameters").at(0);
    semitones["value"] = 24.0;
    semitones["modulation"]["clampMinimum"] = -24.0;
    semitones["modulation"]["clampMaximum"] = 24.0;

    const auto migrated = reverb::graph::parsePatchJson(json.dump());
    const auto& migratedPitch = findNode(migrated, pitch->at("id").get<std::string>());
    REQUIRE(migratedPitch.parameters.front().value == 12.0);
    REQUIRE(migratedPitch.parameters.front().modulation->clampMinimum == -12.0);
    REQUIRE(migratedPitch.parameters.front().modulation->clampMaximum == 12.0);
    REQUIRE(migrated.migrationWarnings.size() == 1);
    REQUIRE(migrated.migrationWarnings.front().find("one-octave range") != std::string::npos);
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
