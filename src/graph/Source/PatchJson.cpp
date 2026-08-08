#include <reverb/graph/PatchJson.h>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <utility>

namespace reverb::graph {
namespace {

using Json = nlohmann::ordered_json;

SignalType parseSignalType(const std::string& value)
{
    if (value == "audio")
        return SignalType::audio;
    if (value == "control")
        return SignalType::control;
    throw std::invalid_argument("unknown signal type '" + value + "'");
}

PortDirection parseDirection(const std::string& value)
{
    if (value == "input")
        return PortDirection::input;
    if (value == "output")
        return PortDirection::output;
    throw std::invalid_argument("unknown port direction '" + value + "'");
}

const char* toString(const SignalType value)
{
    return value == SignalType::audio ? "audio" : "control";
}

const char* toString(const PortDirection value)
{
    return value == PortDirection::input ? "input" : "output";
}

Port parsePort(const Json& json)
{
    return {
        .id = json.at("id").get<std::string>(),
        .signal = parseSignalType(json.at("signal").get<std::string>()),
        .direction = parseDirection(json.at("direction").get<std::string>()),
    };
}

Parameter parseParameter(const Json& json)
{
    return {
        .id = json.at("id").get<std::string>(),
        .value = json.at("value").get<double>(),
        .unit = json.at("unit").get<std::string>(),
    };
}

PortReference parseReference(const Json& json)
{
    return {
        .nodeId = json.at("nodeId").get<std::string>(),
        .portId = json.at("portId").get<std::string>(),
    };
}

Json writePort(const Port& port)
{
    return Json {
        { "id", port.id },
        { "signal", toString(port.signal) },
        { "direction", toString(port.direction) },
    };
}

Json writeParameter(const Parameter& parameter)
{
    return Json {
        { "id", parameter.id },
        { "value", parameter.value },
        { "unit", parameter.unit },
    };
}

Json writeReference(const PortReference& reference)
{
    return Json {
        { "nodeId", reference.nodeId },
        { "portId", reference.portId },
    };
}

} // namespace

GraphDocument parsePatchJson(const std::string_view jsonText)
{
    const auto root = Json::parse(jsonText);
    const auto version = root.at("schemaVersion").get<std::uint32_t>();
    if (version != GraphDocument::schemaVersion)
        throw std::invalid_argument("unsupported schemaVersion " + std::to_string(version));

    GraphDocument document;
    document.engineVersion = root.at("engineVersion").get<std::string>();

    const auto& semantic = root.at("semantic");
    for (const auto& nodeJson : semantic.at("nodes")) {
        Node node;
        node.id = nodeJson.at("id").get<std::string>();
        node.type = nodeJson.at("type").get<std::string>();
        for (const auto& portJson : nodeJson.at("ports"))
            node.ports.push_back(parsePort(portJson));
        for (const auto& parameterJson : nodeJson.at("parameters"))
            node.parameters.push_back(parseParameter(parameterJson));
        document.nodes.push_back(std::move(node));
    }

    for (const auto& connectionJson : semantic.at("connections")) {
        document.connections.push_back({
            .id = connectionJson.at("id").get<std::string>(),
            .from = parseReference(connectionJson.at("from")),
            .to = parseReference(connectionJson.at("to")),
        });
    }

    const auto& layout = root.at("layout");
    for (const auto& positionJson : layout.at("nodes")) {
        document.layout.nodes.push_back({
            .nodeId = positionJson.at("nodeId").get<std::string>(),
            .x = positionJson.at("x").get<double>(),
            .y = positionJson.at("y").get<double>(),
        });
    }
    const auto& viewport = layout.at("viewport");
    document.layout.viewport = {
        .x = viewport.at("x").get<double>(),
        .y = viewport.at("y").get<double>(),
        .zoom = viewport.at("zoom").get<double>(),
    };

    return document;
}

std::string writePatchJson(const GraphDocument& document)
{
    Json nodeArray = Json::array();
    for (const auto& node : document.nodes) {
        Json ports = Json::array();
        for (const auto& port : node.ports)
            ports.push_back(writePort(port));

        Json parameters = Json::array();
        for (const auto& parameter : node.parameters)
            parameters.push_back(writeParameter(parameter));

        nodeArray.push_back({
            { "id", node.id },
            { "type", node.type },
            { "ports", std::move(ports) },
            { "parameters", std::move(parameters) },
        });
    }

    Json connectionArray = Json::array();
    for (const auto& connection : document.connections) {
        connectionArray.push_back({
            { "id", connection.id },
            { "from", writeReference(connection.from) },
            { "to", writeReference(connection.to) },
        });
    }

    Json positionArray = Json::array();
    for (const auto& position : document.layout.nodes) {
        positionArray.push_back({
            { "nodeId", position.nodeId },
            { "x", position.x },
            { "y", position.y },
        });
    }

    const Json root {
        { "schemaVersion", GraphDocument::schemaVersion },
        { "engineVersion", document.engineVersion },
        { "semantic",
            {
                { "nodes", std::move(nodeArray) },
                { "connections", std::move(connectionArray) },
            } },
        { "layout",
            {
                { "nodes", std::move(positionArray) },
                { "viewport",
                    {
                        { "x", document.layout.viewport.x },
                        { "y", document.layout.viewport.y },
                        { "zoom", document.layout.viewport.zoom },
                    } },
            } },
    };

    return root.dump(2) + "\n";
}

} // namespace reverb::graph
