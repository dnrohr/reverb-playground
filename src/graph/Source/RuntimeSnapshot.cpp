#include <reverb/graph/RuntimeSnapshot.h>

#include <reverb/dsp/BarrReferenceRuntime.h>
#include <reverb/graph/BarrReferenceGraph.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <unordered_map>

namespace reverb::graph {
namespace {

using Json = nlohmann::ordered_json;

template <typename Range, typename GetId>
auto findById(const Range& values, const std::string_view id, GetId getId)
{
    return std::ranges::find_if(values, [id, &getId](const auto& value) { return getId(value) == id; });
}

} // namespace

std::string writeBarrRuntimeSnapshotJson(
    const double sampleRate, const std::span<const double> parameterValues)
{
    const auto graph = makeBarrReferenceGraph();
    std::unordered_map<std::string, NodePosition> positions;
    for (const auto& position : graph.layout.nodes)
        positions.emplace(position.nodeId, position);

    Json nodes = Json::array();
    for (const auto& definition : reverb::dsp::barrReferenceRuntimeNodes()) {
        Json ports = Json::array();
        for (const auto& port : definition.ports) {
            ports.push_back({
                { "id", port.id }, { "signal", port.signal }, { "direction", port.direction },
            });
        }
        Json parameters = Json::array();
        for (const auto& parameter : definition.parameters) {
            const auto index = static_cast<std::size_t>(parameter.runtimeId);
            const auto value = index < parameterValues.size() ? parameterValues[index] : parameter.value;
            parameters.push_back({
                { "id", parameter.id }, { "value", value }, { "unit", parameter.unit },
                { "minimum", parameter.minimum }, { "maximum", parameter.maximum }, { "step", parameter.step },
            });
        }
        const auto& position = positions.at(std::string(definition.id));
        nodes.push_back({
            { "id", definition.id },
            { "type", definition.type },
            { "label", definition.label },
            { "role", definition.role },
            { "ports", std::move(ports) },
            { "parameters", std::move(parameters) },
            { "position", { { "x", position.x }, { "y", position.y } } },
        });
    }

    Json connections = Json::array();
    for (const auto& definition : reverb::dsp::barrReferenceRuntimeConnections()) {
        connections.push_back({
            { "id", definition.id },
            { "source", definition.sourceNode },
            { "sourcePort", definition.sourcePort },
            { "target", definition.targetNode },
            { "targetPort", definition.targetPort },
            { "signal", "audio" },
        });
    }

    return Json {
        { "contractVersion", barrRuntimeContractVersion },
        { "engineId", "barr-reference" },
        { "sampleRate", sampleRate },
        { "nodes", std::move(nodes) },
        { "connections", std::move(connections) },
        { "outsidePatch", Json::array({
            { { "id", "master-audition-gain" }, { "purpose", "audition output level" } },
            { { "id", "numerical-safety-guards" }, { "purpose", "mute non-finite or runaway output" } },
        }) },
    }.dump(2);
}

std::vector<std::string> validateBarrRuntimeIdentity(const GraphDocument& graph)
{
    std::vector<std::string> errors;
    const auto definitions = reverb::dsp::barrReferenceRuntimeNodes();
    const auto connections = reverb::dsp::barrReferenceRuntimeConnections();
    if (graph.nodes.size() != definitions.size())
        errors.push_back("runtime/UI node count differs");
    if (graph.connections.size() != connections.size())
        errors.push_back("runtime/UI connection count differs");

    for (const auto& definition : definitions) {
        const auto node = findById(graph.nodes, definition.id, [](const auto& value) { return std::string_view(value.id); });
        if (node == graph.nodes.end()) {
            errors.push_back("runtime node missing from UI graph: " + std::string(definition.id));
            continue;
        }
        if (node->type != definition.type)
            errors.push_back("runtime/UI type differs for node: " + std::string(definition.id));
        if (node->ports.size() != definition.ports.size())
            errors.push_back("runtime/UI port count differs for node: " + std::string(definition.id));
        if (node->parameters.size() != definition.parameters.size())
            errors.push_back("runtime/UI parameter count differs for node: " + std::string(definition.id));
        for (const auto& port : definition.ports) {
            const auto actual = findById(node->ports, port.id, [](const auto& value) { return std::string_view(value.id); });
            const auto expectedSignal = port.signal == "audio" ? SignalType::audio : SignalType::control;
            const auto expectedDirection = port.direction == "input" ? PortDirection::input : PortDirection::output;
            if (actual == node->ports.end()
                || actual->signal != expectedSignal
                || actual->direction != expectedDirection) {
                errors.push_back("runtime/UI port differs: " + std::string(definition.id)
                    + "." + std::string(port.id));
            }
        }
        for (const auto& parameter : definition.parameters) {
            const auto actual = findById(node->parameters, parameter.id, [](const auto& value) { return std::string_view(value.id); });
            if (actual == node->parameters.end()
                || actual->value != parameter.value
                || actual->unit != parameter.unit) {
                errors.push_back("runtime/UI parameter differs: " + std::string(definition.id)
                    + "." + std::string(parameter.id));
            }
        }
    }

    for (const auto& definition : connections) {
        const auto connection = findById(graph.connections, definition.id, [](const auto& value) { return std::string_view(value.id); });
        if (connection == graph.connections.end()
            || connection->from.nodeId != definition.sourceNode
            || connection->from.portId != definition.sourcePort
            || connection->to.nodeId != definition.targetNode
            || connection->to.portId != definition.targetPort) {
            errors.push_back("runtime/UI connection differs: " + std::string(definition.id));
        }
    }
    return errors;
}

} // namespace reverb::graph
