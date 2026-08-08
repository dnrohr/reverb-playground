#include <reverb/graph/GraphDocument.h>

#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace reverb::graph {
namespace {

struct LocatedPort final {
    const Port* port {};
};

std::string portKey(const std::string_view nodeId, const std::string_view portId)
{
    return std::string(nodeId) + "\n" + std::string(portId);
}

void requireNonEmpty(const std::string& value, const std::string& label, ValidationResult& result)
{
    if (value.empty())
        result.errors.push_back(label + " must not be empty");
}

} // namespace

ValidationResult validate(const GraphDocument& document)
{
    ValidationResult result;
    std::unordered_set<std::string> nodeIds;
    std::unordered_map<std::string, LocatedPort> ports;

    requireNonEmpty(document.engineVersion, "engineVersion", result);

    for (const auto& node : document.nodes) {
        requireNonEmpty(node.id, "node id", result);
        requireNonEmpty(node.type, "node '" + node.id + "' type", result);

        if (!nodeIds.insert(node.id).second)
            result.errors.push_back("duplicate node id '" + node.id + "'");

        std::unordered_set<std::string> localPortIds;
        for (const auto& port : node.ports) {
            requireNonEmpty(port.id, "port id on node '" + node.id + "'", result);
            if (!localPortIds.insert(port.id).second)
                result.errors.push_back("duplicate port id '" + port.id + "' on node '" + node.id + "'");
            ports.emplace(portKey(node.id, port.id), LocatedPort { &port });
        }

        std::unordered_set<std::string> parameterIds;
        for (const auto& parameter : node.parameters) {
            requireNonEmpty(parameter.id, "parameter id on node '" + node.id + "'", result);
            requireNonEmpty(parameter.unit, "parameter '" + parameter.id + "' unit", result);
            if (!parameterIds.insert(parameter.id).second)
                result.errors.push_back("duplicate parameter id '" + parameter.id + "' on node '" + node.id + "'");
        }
    }

    std::unordered_set<std::string> connectionIds;
    for (const auto& connection : document.connections) {
        requireNonEmpty(connection.id, "connection id", result);
        if (!connectionIds.insert(connection.id).second)
            result.errors.push_back("duplicate connection id '" + connection.id + "'");

        const auto source = ports.find(portKey(connection.from.nodeId, connection.from.portId));
        const auto target = ports.find(portKey(connection.to.nodeId, connection.to.portId));

        if (source == ports.end()) {
            result.errors.push_back("connection '" + connection.id + "' has unknown source port");
            continue;
        }
        if (target == ports.end()) {
            result.errors.push_back("connection '" + connection.id + "' has unknown target port");
            continue;
        }
        if (source->second.port->direction != PortDirection::output)
            result.errors.push_back("connection '" + connection.id + "' source is not an output");
        if (target->second.port->direction != PortDirection::input)
            result.errors.push_back("connection '" + connection.id + "' target is not an input");
        if (source->second.port->signal != target->second.port->signal)
            result.errors.push_back("connection '" + connection.id + "' mixes audio and control signals");
    }

    std::unordered_set<std::string> positionedNodeIds;
    for (const auto& position : document.layout.nodes) {
        if (!nodeIds.contains(position.nodeId))
            result.errors.push_back("layout references unknown node '" + position.nodeId + "'");
        if (!positionedNodeIds.insert(position.nodeId).second)
            result.errors.push_back("duplicate layout position for node '" + position.nodeId + "'");
    }

    if (document.layout.viewport.zoom <= 0.0)
        result.errors.push_back("viewport zoom must be greater than zero");

    return result;
}

} // namespace reverb::graph
