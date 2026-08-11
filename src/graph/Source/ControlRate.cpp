#include <reverb/graph/ControlRate.h>

#include <algorithm>
#include <cmath>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

namespace reverb::graph {
namespace {

std::string portKey(const std::string& nodeId, const std::string& portId)
{
    return nodeId + "\n" + portId;
}

} // namespace

ControlRatePlan compileControlRatePlan(
    const GraphDocument& document, const double sampleRate, const std::size_t maximumBlockSize)
{
    ControlRatePlan plan;
    plan.sampleRate = sampleRate;
    plan.maximumBlockSize = maximumBlockSize;
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
        plan.errors.push_back("control-rate sample rate must be finite and positive");
    if (maximumBlockSize == 0)
        plan.errors.push_back("control-rate maximum block size must be positive");
    if (!plan.errors.empty())
        return plan;

    plan.quantumSamples = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(sampleRate / controlRateHz)));
    plan.maximumTicksPerBlock = (maximumBlockSize + plan.quantumSamples - 1) / plan.quantumSamples;

    std::unordered_map<std::string, const Connection*> incomingControl;
    std::unordered_set<std::string> controlNodes;
    for (const auto& node : document.nodes) {
        if (std::ranges::any_of(node.ports, [](const Port& port) {
                return port.signal == SignalType::control;
            }))
            controlNodes.insert(node.id);
    }
    if (controlNodes.size() > maximumControlNodes)
        plan.errors.push_back("control graph exceeds 64 participating nodes");

    for (const auto& connection : document.connections) {
        const auto targetNode = std::ranges::find(document.nodes, connection.to.nodeId, &Node::id);
        if (targetNode == document.nodes.end())
            continue;
        const auto targetPort = std::ranges::find(targetNode->ports, connection.to.portId, &Port::id);
        if (targetPort == targetNode->ports.end() || targetPort->signal != SignalType::control)
            continue;
        const auto [_, inserted] = incomingControl.emplace(
            portKey(connection.to.nodeId, connection.to.portId), &connection);
        if (!inserted)
            plan.errors.push_back("control input '" + connection.to.nodeId + "." + connection.to.portId
                + "' has more than one cable");
    }

    for (const auto& node : document.nodes) {
        for (const auto& parameter : node.parameters) {
            if (!parameter.modulation)
                continue;
            const auto& modulation = *parameter.modulation;
            const auto cable = incomingControl.find(portKey(node.id, modulation.portId));
            if (cable == incomingControl.end())
                continue; // An exposed but disconnected socket leaves the base value unchanged.
            plan.mappings.push_back({
                cable->second->from.nodeId,
                cable->second->from.portId,
                node.id,
                parameter.id,
                parameter.value,
                modulation.amount,
                modulation.polarity,
                modulation.clampMinimum,
                modulation.clampMaximum,
            });
        }
    }
    if (plan.mappings.size() > maximumControlMappings)
        plan.errors.push_back("control graph exceeds 128 parameter mappings");
    plan.maximumMappingEvaluationsPerBlock = plan.maximumTicksPerBlock * plan.mappings.size();
    return plan;
}

double mappedParameterValue(const CompiledControlMapping& mapping, const double controlValue) noexcept
{
    const auto finiteControl = std::isfinite(controlValue) ? controlValue : 0.0;
    const auto normalized = mapping.polarity == ModulationPolarity::bipolar
        ? std::clamp(finiteControl, -1.0, 1.0)
        : std::clamp(finiteControl, 0.0, 1.0);
    return std::clamp(
        mapping.baseValue + mapping.amount * normalized,
        mapping.clampMinimum,
        mapping.clampMaximum);
}

void ControlRamp::reset(const double value) noexcept
{
    current_ = target_ = std::isfinite(value) ? value : 0.0;
    increment_ = 0.0;
    remaining_ = 0;
}

void ControlRamp::setTarget(const double value, const std::size_t samples) noexcept
{
    target_ = std::isfinite(value) ? value : current_;
    remaining_ = samples;
    increment_ = samples == 0 ? 0.0 : (target_ - current_) / static_cast<double>(samples);
    if (samples == 0)
        current_ = target_;
}

double ControlRamp::next() noexcept
{
    if (remaining_ > 0) {
        current_ += increment_;
        if (--remaining_ == 0)
            current_ = target_;
    }
    return current_;
}

} // namespace reverb::graph
