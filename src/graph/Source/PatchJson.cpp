#include <reverb/graph/PatchJson.h>
#include <reverb/dsp/PitchShiftContract.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
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

ModulationPolarity parseModulationPolarity(const std::string& value)
{
    if (value == "unipolar")
        return ModulationPolarity::unipolar;
    if (value == "bipolar")
        return ModulationPolarity::bipolar;
    throw std::invalid_argument("unknown modulation polarity '" + value + "'");
}

const char* toString(const ModulationPolarity value)
{
    return value == ModulationPolarity::unipolar ? "unipolar" : "bipolar";
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
    Parameter parameter {
        .id = json.at("id").get<std::string>(),
        .value = json.at("value").get<double>(),
        .unit = json.at("unit").get<std::string>(),
    };
    if (const auto mapping = json.find("modulation"); mapping != json.end()) {
        parameter.modulation = ParameterModulation {
            .portId = mapping->at("portId").get<std::string>(),
            .amount = mapping->at("amount").get<double>(),
            .polarity = parseModulationPolarity(mapping->at("polarity").get<std::string>()),
            .clampMinimum = mapping->at("clampMinimum").get<double>(),
            .clampMaximum = mapping->at("clampMaximum").get<double>(),
        };
    }
    return parameter;
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
    Json json {
        { "id", parameter.id },
        { "value", parameter.value },
        { "unit", parameter.unit },
    };
    if (parameter.modulation) {
        json["modulation"] = {
            { "portId", parameter.modulation->portId },
            { "amount", parameter.modulation->amount },
            { "polarity", toString(parameter.modulation->polarity) },
            { "clampMinimum", parameter.modulation->clampMinimum },
            { "clampMaximum", parameter.modulation->clampMaximum },
        };
    }
    return json;
}

Json writeReference(const PortReference& reference)
{
    return Json {
        { "nodeId", reference.nodeId },
        { "portId", reference.portId },
    };
}

void migrateVersionOneModulation(Node& node)
{
    for (auto& parameter : node.parameters) {
        const auto portId = parameter.id + "-mod";
        if (std::ranges::none_of(node.ports, [&](const Port& port) { return port.id == portId; }))
            node.ports.push_back({ portId, SignalType::control, PortDirection::input });

        double minimum = 0.0;
        double maximum = 1.0;
        double amount = 0.25;
        if (parameter.id == "gain") {
            minimum = node.type == "sum" ? 0.0 : -1.0;
            maximum = 1.0;
            amount = 0.5;
        } else if (parameter.id == "delay") {
            minimum = 0.1;
            maximum = node.type == "delay" ? 10'000.0 : 100.0;
            amount = node.type == "delay" ? 10.0 : 2.0;
        } else if (parameter.id == "coefficient") {
            minimum = -0.95;
            maximum = 0.95;
            amount = 0.25;
        } else if (parameter.id == "cutoff") {
            minimum = node.id == "input-filter" ? 100.0 : 20.0;
            maximum = 20'000.0;
            amount = 5'000.0;
        } else {
            const auto extent = std::max(1.0, std::abs(parameter.value));
            minimum = parameter.value - extent;
            maximum = parameter.value + extent;
            amount = extent * 0.5;
        }
        parameter.modulation = ParameterModulation {
            portId, amount, ModulationPolarity::bipolar, minimum, maximum,
        };
    }
}

void migrateLegacyPitchRange(Node& node, std::vector<std::string>& warnings)
{
    if (node.type != "pitch-shift") return;
    const auto semitones = std::ranges::find(node.parameters, "semitones", &Parameter::id);
    if (semitones == node.parameters.end()) return;
    const auto bounded = std::clamp(semitones->value,
        reverb::dsp::pitch_shift::minimumSemitones,
        reverb::dsp::pitch_shift::maximumSemitones);
    const auto original = semitones->value;
    semitones->value = bounded;
    auto changed = bounded != original;
    if (semitones->modulation) {
        const auto minimum = std::max(semitones->modulation->clampMinimum,
            reverb::dsp::pitch_shift::minimumSemitones);
        const auto maximum = std::min(semitones->modulation->clampMaximum,
            reverb::dsp::pitch_shift::maximumSemitones);
        changed = changed || minimum != semitones->modulation->clampMinimum
            || maximum != semitones->modulation->clampMaximum;
        semitones->modulation->clampMinimum = minimum;
        semitones->modulation->clampMaximum = maximum;
    }
    if (changed)
        warnings.push_back("migrated Pitch Shift '" + node.id + "' semitone value/mapping to "
            + std::to_string(bounded) + " for the one-octave range");
}

} // namespace

GraphDocument parsePatchJson(const std::string_view jsonText)
{
    const auto root = Json::parse(jsonText);
    const auto version = root.at("schemaVersion").get<std::uint32_t>();
    if (version < GraphDocument::oldestReadableSchemaVersion || version > GraphDocument::schemaVersion)
        throw std::invalid_argument("unsupported schemaVersion " + std::to_string(version));

    GraphDocument document;
    document.engineVersion = root.at("engineVersion").get<std::string>();
    if (const auto quality = root.find("qualityPolicy"); quality != root.end()) {
        const auto value = quality->get<std::string>();
        if (value == "draft") document.qualityPolicy = QualityPolicy::draft;
        else if (value == "normal") document.qualityPolicy = QualityPolicy::normal;
        else if (value == "high") document.qualityPolicy = QualityPolicy::high;
        else throw std::invalid_argument("unsupported qualityPolicy '" + value + "'");
    }

    const auto& semantic = root.at("semantic");
    for (const auto& nodeJson : semantic.at("nodes")) {
        Node node;
        node.id = nodeJson.at("id").get<std::string>();
        node.type = nodeJson.at("type").get<std::string>();
        if (const auto name = nodeJson.find("name"); name != nodeJson.end())
            node.name = name->get<std::string>();
        if (const auto presentation = nodeJson.find("presentation"); presentation != nodeJson.end())
            node.presentation = presentation->get<std::string>();
        for (const auto& portJson : nodeJson.at("ports"))
            node.ports.push_back(parsePort(portJson));
        for (const auto& parameterJson : nodeJson.at("parameters"))
            node.parameters.push_back(parseParameter(parameterJson));
        if (version == 1)
            migrateVersionOneModulation(node);
        migrateLegacyPitchRange(node, document.migrationWarnings);
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
    if (const auto groups = layout.find("groups"); groups != layout.end()) {
        for (const auto& groupJson : *groups) {
            LayoutGroup group { .id = groupJson.at("id").get<std::string>(), .name = groupJson.at("name").get<std::string>(),
                .collapsed = groupJson.at("collapsed").get<bool>() };
            group.nodeIds = groupJson.at("nodeIds").get<std::vector<std::string>>();
            document.layout.groups.push_back(std::move(group));
        }
    }
    if (const auto cables = layout.find("cables"); cables != layout.end()) {
        for (const auto& cableJson : *cables) {
            LayoutCable cable { .edgeId = cableJson.at("edgeId").get<std::string>() };
            if (const auto waypoints = cableJson.find("waypoints"); waypoints != cableJson.end())
                for (const auto& point : *waypoints) cable.waypoints.push_back({ point.at("x").get<double>(), point.at("y").get<double>() });
            if (const auto portal = cableJson.find("portal"); portal != cableJson.end()) cable.portalName = portal->at("name").get<std::string>();
            document.layout.cables.push_back(std::move(cable));
        }
    }
    if (const auto subpatches = layout.find("subpatches"); subpatches != layout.end()) {
        for (const auto& instanceJson : *subpatches) {
            LayoutSubpatchInstance instance {
                .id = instanceJson.at("id").get<std::string>(),
                .definitionId = instanceJson.at("definitionId").get<std::string>(),
                .definitionVersion = instanceJson.at("definitionVersion").get<std::uint32_t>(),
                .definitionName = instanceJson.at("definitionName").get<std::string>(),
                .memberNodeIds = instanceJson.at("memberNodeIds").get<std::vector<std::string>>(),
            };
            for (const auto& portJson : instanceJson.at("ports")) instance.ports.push_back({
                .id = portJson.at("id").get<std::string>(), .signal = parseSignalType(portJson.at("signal").get<std::string>()),
                .direction = parseDirection(portJson.at("direction").get<std::string>()),
                .nodeId = portJson.at("nodeId").get<std::string>(), .portId = portJson.at("portId").get<std::string>(),
            });
            document.layout.subpatches.push_back(std::move(instance));
        }
    }
    if (const auto hierarchies = layout.find("hierarchies"); hierarchies != layout.end()) {
        for (const auto& hierarchyJson : *hierarchies) {
            const auto& position = hierarchyJson.at("position");
            const auto& nestedViewport = hierarchyJson.at("nestedViewport");
            LayoutHierarchyPresentation hierarchy {
                .id = hierarchyJson.at("id").get<std::string>(),
                .kind = hierarchyJson.at("kind").get<std::string>(),
                .name = hierarchyJson.at("name").get<std::string>(),
                .collapsed = hierarchyJson.at("collapsed").get<bool>(),
                .memberNodeIds = hierarchyJson.at("memberNodeIds").get<std::vector<std::string>>(),
                .x = position.at("x").get<double>(),
                .y = position.at("y").get<double>(),
                .nestedViewport = { nestedViewport.at("x").get<double>(), nestedViewport.at("y").get<double>(), nestedViewport.at("zoom").get<double>() },
            };
            for (const auto& portJson : hierarchyJson.at("ports")) {
                HierarchyPortBinding port {
                    .id = portJson.at("id").get<std::string>(),
                    .name = portJson.at("name").get<std::string>(),
                    .signal = parseSignalType(portJson.at("signal").get<std::string>()),
                    .direction = parseDirection(portJson.at("direction").get<std::string>()),
                };
                for (const auto& targetJson : portJson.at("targets")) port.targets.push_back(parseReference(targetJson));
                hierarchy.ports.push_back(std::move(port));
            }
            if (const auto parentId = hierarchyJson.find("parentId"); parentId != hierarchyJson.end())
                hierarchy.parentId = parentId->get<std::string>();
            document.layout.hierarchies.push_back(std::move(hierarchy));
        }
    }

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

        Json writtenNode {
            { "id", node.id },
            { "type", node.type },
            { "ports", std::move(ports) },
            { "parameters", std::move(parameters) },
        };
        if (!node.name.empty())
            writtenNode["name"] = node.name;
        if (!node.presentation.empty())
            writtenNode["presentation"] = node.presentation;
        nodeArray.push_back(std::move(writtenNode));
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
    Json groupArray = Json::array();
    for (const auto& group : document.layout.groups)
        groupArray.push_back({ { "id", group.id }, { "name", group.name }, { "collapsed", group.collapsed }, { "nodeIds", group.nodeIds } });

    Json layoutJson {
        { "nodes", std::move(positionArray) },
        { "viewport",
            {
                { "x", document.layout.viewport.x },
                { "y", document.layout.viewport.y },
                { "zoom", document.layout.viewport.zoom },
            } },
    };
    if (!groupArray.empty()) layoutJson["groups"] = std::move(groupArray);
    Json cableArray = Json::array();
    for (const auto& cable : document.layout.cables) {
        Json entry { { "edgeId", cable.edgeId } };
        if (!cable.waypoints.empty()) {
            Json points = Json::array();
            for (const auto& point : cable.waypoints) points.push_back({ { "x", point.x }, { "y", point.y } });
            entry["waypoints"] = std::move(points);
        }
        if (cable.portalName) entry["portal"] = { { "name", *cable.portalName } };
        cableArray.push_back(std::move(entry));
    }
    if (!cableArray.empty()) layoutJson["cables"] = std::move(cableArray);
    Json subpatchArray = Json::array();
    for (const auto& instance : document.layout.subpatches) {
        Json ports = Json::array();
        for (const auto& port : instance.ports) ports.push_back({ { "id", port.id }, { "signal", toString(port.signal) },
            { "direction", toString(port.direction) }, { "nodeId", port.nodeId }, { "portId", port.portId } });
        subpatchArray.push_back({ { "id", instance.id }, { "definitionId", instance.definitionId },
            { "definitionVersion", instance.definitionVersion }, { "definitionName", instance.definitionName },
            { "memberNodeIds", instance.memberNodeIds }, { "ports", std::move(ports) } });
    }
    if (!subpatchArray.empty()) layoutJson["subpatches"] = std::move(subpatchArray);
    Json hierarchyArray = Json::array();
    for (const auto& hierarchy : document.layout.hierarchies) {
        Json ports = Json::array();
        for (const auto& port : hierarchy.ports) {
            Json targets = Json::array();
            for (const auto& target : port.targets) targets.push_back(writeReference(target));
            ports.push_back({ { "id", port.id }, { "name", port.name }, { "signal", toString(port.signal) },
                { "direction", toString(port.direction) }, { "targets", std::move(targets) } });
        }
        Json entry {
            { "id", hierarchy.id }, { "kind", hierarchy.kind }, { "name", hierarchy.name },
            { "collapsed", hierarchy.collapsed }, { "memberNodeIds", hierarchy.memberNodeIds },
            { "position", { { "x", hierarchy.x }, { "y", hierarchy.y } } },
            { "nestedViewport", { { "x", hierarchy.nestedViewport.x }, { "y", hierarchy.nestedViewport.y },
                { "zoom", hierarchy.nestedViewport.zoom } } },
            { "ports", std::move(ports) },
        };
        if (hierarchy.parentId) entry["parentId"] = *hierarchy.parentId;
        hierarchyArray.push_back(std::move(entry));
    }
    if (!hierarchyArray.empty()) layoutJson["hierarchies"] = std::move(hierarchyArray);

    const Json root {
        { "schemaVersion", GraphDocument::schemaVersion },
        { "engineVersion", document.engineVersion },
        { "qualityPolicy", document.qualityPolicy == QualityPolicy::draft ? "draft"
            : document.qualityPolicy == QualityPolicy::high ? "high" : "normal" },
        { "semantic",
            {
                { "nodes", std::move(nodeArray) },
                { "connections", std::move(connectionArray) },
            } },
        { "layout", std::move(layoutJson) },
    };

    return root.dump(2) + "\n";
}

} // namespace reverb::graph
