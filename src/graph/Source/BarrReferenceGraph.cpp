#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/dsp/BarrReferenceRuntime.h>

namespace reverb::graph {
namespace {

Port makePort(const reverb::dsp::RuntimePortDefinition& definition)
{
    return {
        std::string(definition.id),
        definition.signal == "audio" ? SignalType::audio : SignalType::control,
        definition.direction == "input" ? PortDirection::input : PortDirection::output,
    };
}

} // namespace

GraphDocument makeBarrReferenceGraph()
{
    GraphDocument graph;
    for (const auto& definition : reverb::dsp::barrReferenceRuntimeNodes()) {
        Node node { std::string(definition.id), std::string(definition.type), {}, {} };
        for (const auto& port : definition.ports)
            node.ports.push_back(makePort(port));
        for (const auto& parameter : definition.parameters) {
            node.parameters.push_back({
                std::string(parameter.id), parameter.value, std::string(parameter.unit),
                ParameterModulation {
                    std::string(parameter.modulationPort),
                    parameter.modulationAmount,
                    parameter.modulationPolarity == "unipolar"
                        ? ModulationPolarity::unipolar : ModulationPolarity::bipolar,
                    parameter.minimum,
                    parameter.maximum,
                },
            });
        }
        graph.nodes.push_back(std::move(node));
    }
    for (const auto& definition : reverb::dsp::barrReferenceRuntimeConnections()) {
        graph.connections.push_back({
            std::string(definition.id),
            { std::string(definition.sourceNode), std::string(definition.sourcePort) },
            { std::string(definition.targetNode), std::string(definition.targetPort) },
        });
    }
    graph.layout.nodes = {
        { "input", 0.0, 80.0 }, { "sum", 180.0, 80.0 }, { "input-filter", 360.0, 80.0 },
        { "diffuser-1", 540.0, 80.0 }, { "diffuser-2", 720.0, 80.0 },
        { "tank-1", 720.0, 260.0 }, { "tank-2", 540.0, 260.0 },
        { "left-tap", 360.0, 210.0 }, { "right-tap", 360.0, 330.0 }, { "output", 180.0, 260.0 },
    };
    return graph;
}

} // namespace reverb::graph
