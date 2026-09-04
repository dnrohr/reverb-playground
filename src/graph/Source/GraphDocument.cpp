#include <reverb/graph/GraphDocument.h>

#include <algorithm>
#include <cmath>
#include <ranges>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace reverb::graph {
namespace {

constexpr std::size_t maximumHierarchyDepth = 1;

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

bool containsDelayFreeCycle(
    const std::string& nodeId,
    const std::unordered_map<std::string, std::vector<std::string>>& adjacency,
    std::unordered_set<std::string>& visiting,
    std::unordered_set<std::string>& visited)
{
    if (visiting.contains(nodeId))
        return true;
    if (visited.contains(nodeId))
        return false;

    visiting.insert(nodeId);
    if (const auto edges = adjacency.find(nodeId); edges != adjacency.end()) {
        for (const auto& target : edges->second) {
            if (containsDelayFreeCycle(target, adjacency, visiting, visited))
                return true;
        }
    }
    visiting.erase(nodeId);
    visited.insert(nodeId);
    return false;
}

} // namespace

ValidationResult validate(const GraphDocument& document)
{
    ValidationResult result;
    std::unordered_set<std::string> nodeIds;
    std::unordered_set<std::string> delayNodeIds;
    std::unordered_map<std::string, LocatedPort> ports;

    requireNonEmpty(document.engineVersion, "engineVersion", result);

    for (const auto& node : document.nodes) {
        requireNonEmpty(node.id, "node id", result);
        requireNonEmpty(node.type, "node '" + node.id + "' type", result);

        if (!nodeIds.insert(node.id).second)
            result.errors.push_back("duplicate node id '" + node.id + "'");
        if (node.type == "delay")
            delayNodeIds.insert(node.id);

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
            if (!std::isfinite(parameter.value))
                result.errors.push_back("parameter '" + node.id + "." + parameter.id + "' must be finite");
            if (parameter.modulation) {
                const auto& modulation = *parameter.modulation;
                const auto socket = std::ranges::find(node.ports, modulation.portId, &Port::id);
                if (socket == node.ports.end()
                    || socket->signal != SignalType::control
                    || socket->direction != PortDirection::input) {
                    result.errors.push_back("parameter '" + node.id + "." + parameter.id
                        + "' must reference a control input socket");
                }
                if (!std::isfinite(modulation.amount))
                    result.errors.push_back("parameter '" + node.id + "." + parameter.id
                        + "' modulation amount must be finite");
                if (!std::isfinite(modulation.clampMinimum)
                    || !std::isfinite(modulation.clampMaximum)
                    || modulation.clampMinimum >= modulation.clampMaximum) {
                    result.errors.push_back("parameter '" + node.id + "." + parameter.id
                        + "' modulation clamp must be finite and increasing");
                }
            }
        }
    }

    std::unordered_set<std::string> connectionIds;
    std::unordered_map<std::string, std::vector<std::string>> delayFreeAdjacency;
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
        if (!delayNodeIds.contains(connection.from.nodeId) && !delayNodeIds.contains(connection.to.nodeId))
            delayFreeAdjacency[connection.from.nodeId].push_back(connection.to.nodeId);
    }

    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visited;
    for (const auto& node : document.nodes) {
        if (!delayNodeIds.contains(node.id)
            && containsDelayFreeCycle(node.id, delayFreeAdjacency, visiting, visited)) {
            result.errors.push_back("directed cycles must contain an explicit delay node");
            break;
        }
    }

    std::unordered_set<std::string> positionedNodeIds;
    for (const auto& position : document.layout.nodes) {
        if (!nodeIds.contains(position.nodeId))
            result.errors.push_back("layout references unknown node '" + position.nodeId + "'");
        if (!positionedNodeIds.insert(position.nodeId).second)
            result.errors.push_back("duplicate layout position for node '" + position.nodeId + "'");
    }

    std::unordered_set<std::string> groupIds;
    std::unordered_set<std::string> groupedNodeIds;
    for (const auto& group : document.layout.groups) {
        if (group.id.empty() || !groupIds.insert(group.id).second)
            result.errors.push_back("layout group IDs must be non-empty and unique");
        if (group.name.empty() || group.name.size() > 64)
            result.errors.push_back("layout group '" + group.id + "' name must contain 1 through 64 characters");
        if (group.nodeIds.size() < 2)
            result.errors.push_back("layout group '" + group.id + "' must contain at least two nodes");
        for (const auto& nodeId : group.nodeIds) {
            if (!nodeIds.contains(nodeId))
                result.errors.push_back("layout group '" + group.id + "' references unknown node '" + nodeId + "'");
            if (!groupedNodeIds.insert(nodeId).second)
                result.errors.push_back("node '" + nodeId + "' belongs to multiple layout groups");
            const auto node = std::ranges::find(document.nodes, nodeId, &Node::id);
            if (node != document.nodes.end() && (node->type == "stereo-input" || node->type == "stereo-output"))
                result.errors.push_back("layout group '" + group.id + "' cannot contain I/O node '" + nodeId + "'");
        }
    }

    std::unordered_set<std::string> routedCableIds;
    for (const auto& cable : document.layout.cables) {
        if (!routedCableIds.insert(cable.edgeId).second)
            result.errors.push_back("layout cable IDs must be unique");
        if (std::ranges::find(document.connections, cable.edgeId, &Connection::id) == document.connections.end())
            result.errors.push_back("layout cable references unknown connection '" + cable.edgeId + "'");
        if (cable.waypoints.size() > 32)
            result.errors.push_back("layout cable '" + cable.edgeId + "' exceeds 32 waypoints");
        for (const auto& point : cable.waypoints)
            if (!std::isfinite(point.x) || !std::isfinite(point.y))
                result.errors.push_back("layout cable '" + cable.edgeId + "' has a non-finite waypoint");
        if (cable.portalName && (cable.portalName->find_first_not_of(" \t\r\n") == std::string::npos || cable.portalName->size() > 32))
            result.errors.push_back("layout cable '" + cable.edgeId + "' portal name must contain 1 through 32 characters");
        if (cable.waypoints.empty() && !cable.portalName)
            result.errors.push_back("layout cable '" + cable.edgeId + "' has empty routing metadata");
    }

    std::unordered_set<std::string> subpatchIds;
    std::unordered_set<std::string> subpatchMemberIds;
    for (const auto& instance : document.layout.subpatches) {
        if (instance.id.empty() || !subpatchIds.insert(instance.id).second)
            result.errors.push_back("subpatch instance IDs must be non-empty and unique");
        if (instance.definitionId.empty() || instance.definitionVersion < 1 || instance.definitionName.empty() || instance.definitionName.size() > 64)
            result.errors.push_back("subpatch instance '" + instance.id + "' has invalid definition identity");
        if (instance.memberNodeIds.empty() || instance.ports.empty())
            result.errors.push_back("subpatch instance '" + instance.id + "' requires members and explicit ports");
        std::unordered_set<std::string> localMembers;
        for (const auto& nodeId : instance.memberNodeIds) {
            if (!nodeIds.contains(nodeId) || !localMembers.insert(nodeId).second || !subpatchMemberIds.insert(nodeId).second)
                result.errors.push_back("subpatch instance '" + instance.id + "' has an invalid, duplicate, or shared member '" + nodeId + "'");
            const auto member = std::ranges::find(document.nodes, nodeId, &Node::id);
            if (member != document.nodes.end() && (member->type == "stereo-input" || member->type == "stereo-output"))
                result.errors.push_back("subpatch instance '" + instance.id + "' cannot contain I/O node '" + nodeId + "'");
        }
        std::unordered_set<std::string> localPorts;
        for (const auto& binding : instance.ports) {
            if (binding.id.empty() || !localPorts.insert(binding.id).second || !localMembers.contains(binding.nodeId)) {
                result.errors.push_back("subpatch instance '" + instance.id + "' has an invalid explicit port '" + binding.id + "'");
                continue;
            }
            const auto actual = ports.find(portKey(binding.nodeId, binding.portId));
            if (actual == ports.end() || actual->second.port->signal != binding.signal || actual->second.port->direction != binding.direction)
                result.errors.push_back("subpatch instance '" + instance.id + "' port '" + binding.id + "' does not match its primitive endpoint");
        }
    }

    std::unordered_set<std::string> hierarchyIds;
    std::unordered_set<std::string> hierarchyMemberIds;
    std::unordered_map<std::string, std::string> hierarchyParents;
    for (const auto& hierarchy : document.layout.hierarchies) {
        if (hierarchy.id.empty() || !hierarchyIds.insert(hierarchy.id).second)
            result.errors.push_back("hierarchy IDs must be non-empty and unique");
        if (hierarchy.kind != "compound" && hierarchy.kind != "subpatch")
            result.errors.push_back("hierarchy '" + hierarchy.id + "' has unsupported kind '" + hierarchy.kind + "'");
        if (hierarchy.name.empty() || hierarchy.name.size() > 64)
            result.errors.push_back("hierarchy '" + hierarchy.id + "' name must contain 1 through 64 characters");
        if (hierarchy.memberNodeIds.empty() || hierarchy.ports.empty())
            result.errors.push_back("hierarchy '" + hierarchy.id + "' requires members and explicit boundary ports");
        if (!std::isfinite(hierarchy.x) || !std::isfinite(hierarchy.y)
            || !std::isfinite(hierarchy.nestedViewport.x) || !std::isfinite(hierarchy.nestedViewport.y)
            || !std::isfinite(hierarchy.nestedViewport.zoom) || hierarchy.nestedViewport.zoom <= 0.0)
            result.errors.push_back("hierarchy '" + hierarchy.id + "' contains an invalid position or nested viewport");
        std::unordered_set<std::string> localMembers;
        for (const auto& nodeId : hierarchy.memberNodeIds) {
            if (!nodeIds.contains(nodeId) || !localMembers.insert(nodeId).second || !hierarchyMemberIds.insert(nodeId).second)
                result.errors.push_back("hierarchy '" + hierarchy.id + "' has an invalid, duplicate, or shared member '" + nodeId + "'");
            const auto member = std::ranges::find(document.nodes, nodeId, &Node::id);
            if (member != document.nodes.end() && (member->type == "stereo-input" || member->type == "stereo-output"))
                result.errors.push_back("hierarchy '" + hierarchy.id + "' cannot contain I/O node '" + nodeId + "'");
        }
        std::unordered_set<std::string> localPorts;
        for (const auto& binding : hierarchy.ports) {
            if (binding.id.empty() || !localPorts.insert(binding.id).second || binding.name.empty()
                || binding.name.size() > 32 || binding.targets.empty()) {
                result.errors.push_back("hierarchy '" + hierarchy.id + "' has invalid boundary port '" + binding.id + "'");
                continue;
            }
            for (const auto& target : binding.targets) {
                if (!localMembers.contains(target.nodeId)) {
                    result.errors.push_back("hierarchy '" + hierarchy.id + "' port '" + binding.id
                        + "' targets non-member '" + target.nodeId + "." + target.portId + "'");
                    continue;
                }
                const auto actual = ports.find(portKey(target.nodeId, target.portId));
                if (actual == ports.end() || actual->second.port->signal != binding.signal
                    || actual->second.port->direction != binding.direction) {
                    result.errors.push_back("hierarchy '" + hierarchy.id + "' port '" + binding.id
                        + "' does not match internal port '" + target.nodeId + "." + target.portId + "'");
                }
            }
            if (binding.direction == PortDirection::input) {
                std::unordered_set<std::string> externalSources;
                for (const auto& connection : document.connections) {
                    if (localMembers.contains(connection.from.nodeId)) continue;
                    if (std::ranges::any_of(binding.targets, [&](const auto& target) {
                        return target.nodeId == connection.to.nodeId && target.portId == connection.to.portId;
                    })) externalSources.insert(portKey(connection.from.nodeId, connection.from.portId));
                }
                if (externalSources.size() > 1)
                    result.errors.push_back("hierarchy '" + hierarchy.id + "' port '" + binding.id
                        + "' has divergent external sources; reconnect the parent proxy as one boundary operation");
            }
        }
        for (const auto& connection : document.connections) {
            const auto sourceInside = localMembers.contains(connection.from.nodeId);
            const auto targetInside = localMembers.contains(connection.to.nodeId);
            if (sourceInside == targetInside) continue;
            const auto direction = targetInside ? PortDirection::input : PortDirection::output;
            const auto& endpoint = targetInside ? connection.to : connection.from;
            const auto matched = std::ranges::any_of(hierarchy.ports, [&](const auto& binding) {
                return binding.direction == direction && std::ranges::any_of(binding.targets, [&](const auto& target) {
                    return target.nodeId == endpoint.nodeId && target.portId == endpoint.portId;
                });
            });
            if (!matched)
                result.errors.push_back("hierarchy '" + hierarchy.id + "' has unmapped crossing connection '"
                    + connection.id + "' at '" + endpoint.nodeId + "." + endpoint.portId + "'");
        }
        if (hierarchy.parentId) hierarchyParents[hierarchy.id] = *hierarchy.parentId;
    }
    for (const auto& [id, parent] : hierarchyParents) {
        if (!hierarchyIds.contains(parent)) {
            result.errors.push_back("hierarchy '" + id + "' references missing parent '" + parent + "'");
            continue;
        }
        std::unordered_set<std::string> seen;
        auto cursor = id;
        std::size_t depth = 0;
        while (true) {
            const auto next = hierarchyParents.find(cursor);
            if (next == hierarchyParents.end()) break;
            if (!seen.insert(cursor).second) {
                result.errors.push_back("hierarchy '" + id + "' is recursive");
                break;
            }
            if (seen.contains(next->second)) {
                result.errors.push_back("hierarchy '" + id + "' is recursive");
                break;
            }
            cursor = next->second;
            if (++depth >= maximumHierarchyDepth) {
                result.errors.push_back("hierarchy '" + id + "' exceeds nesting depth "
                    + std::to_string(maximumHierarchyDepth));
                break;
            }
        }
    }

    if (document.layout.viewport.zoom <= 0.0)
        result.errors.push_back("viewport zoom must be greater than zero");

    return result;
}

} // namespace reverb::graph
