#include <reverb/dsp/BarrReferenceRuntime.h>

#include <array>
#include <stdexcept>
#include <string>

namespace reverb::dsp {
namespace {

using Port = RuntimePortDefinition;
using Parameter = RuntimeParameterDefinition;

constexpr Port audioInput(std::string_view id) { return { id, "audio", "input" }; }
constexpr Port audioOutput(std::string_view id) { return { id, "audio", "output" }; }

constexpr std::array inputPorts { audioOutput("out-l"), audioOutput("out-r") };
constexpr std::array sumPorts { audioInput("in-l"), audioInput("in-r"), audioOutput("out") };
constexpr std::array monoPorts { audioInput("in"), audioOutput("out") };
constexpr std::array outputPorts { audioInput("in-l"), audioInput("in-r") };
constexpr std::span<const Parameter> noParameters {};
constexpr std::array sumParameters {
    Parameter { BarrParameterId::sumGain, "gain", 0.5, "linear", 0.0, 1.0, 0.001 }
};
constexpr std::array filterParameters {
    Parameter { BarrParameterId::filterCutoff, "cutoff", 7'000.0, "hertz", 100.0, 20'000.0, 1.0 }
};
constexpr std::array diffuserOneParameters {
    Parameter { BarrParameterId::diffuserOneDelay, "delay", 4.31, "milliseconds", 0.1, 100.0, 0.01 },
    Parameter { BarrParameterId::diffuserOneCoefficient, "coefficient", 0.5, "unitless", -0.95, 0.95, 0.001 }
};
constexpr std::array diffuserTwoParameters {
    Parameter { BarrParameterId::diffuserTwoDelay, "delay", 7.13, "milliseconds", 0.1, 100.0, 0.01 },
    Parameter { BarrParameterId::diffuserTwoCoefficient, "coefficient", 0.5, "unitless", -0.95, 0.95, 0.001 }
};
constexpr std::array tankOneParameters {
    Parameter { BarrParameterId::tankOneDelay, "delay", 13.73, "milliseconds", 0.1, 100.0, 0.01 },
    Parameter { BarrParameterId::tankOneCoefficient, "coefficient", 0.5, "unitless", -0.95, 0.95, 0.001 }
};
constexpr std::array tankTwoParameters {
    Parameter { BarrParameterId::tankTwoDelay, "delay", 19.91, "milliseconds", 0.1, 100.0, 0.01 },
    Parameter { BarrParameterId::tankTwoCoefficient, "coefficient", -0.5, "unitless", -0.95, 0.95, 0.001 }
};
constexpr std::array leftTapParameters {
    Parameter { BarrParameterId::leftTapDelay, "delay", 29.71, "milliseconds", 0.1, 100.0, 0.01 },
    Parameter { BarrParameterId::leftTapCoefficient, "coefficient", 0.5, "unitless", -0.95, 0.95, 0.001 }
};
constexpr std::array rightTapParameters {
    Parameter { BarrParameterId::rightTapDelay, "delay", 37.11, "milliseconds", 0.1, 100.0, 0.01 },
    Parameter { BarrParameterId::rightTapCoefficient, "coefficient", 0.5, "unitless", -0.95, 0.95, 0.001 }
};

constexpr std::array nodes {
    RuntimeNodeDefinition { "input", "stereo-input", "Stereo Input", "io", inputPorts, noParameters },
    RuntimeNodeDefinition { "sum", "sum", "Mono Sum", "routing", sumPorts, sumParameters },
    RuntimeNodeDefinition { "input-filter", "lowpass", "Input Low-pass", "filter", monoPorts, filterParameters },
    RuntimeNodeDefinition { "diffuser-1", "allpass", "Diffuser 1", "diffusion", monoPorts, diffuserOneParameters },
    RuntimeNodeDefinition { "diffuser-2", "allpass", "Diffuser 2", "diffusion", monoPorts, diffuserTwoParameters },
    RuntimeNodeDefinition { "tank-1", "allpass", "Tank 1", "tank", monoPorts, tankOneParameters },
    RuntimeNodeDefinition { "tank-2", "allpass", "Tank 2", "tank", monoPorts, tankTwoParameters },
    RuntimeNodeDefinition { "left-tap", "allpass", "Left Tap", "tap", monoPorts, leftTapParameters },
    RuntimeNodeDefinition { "right-tap", "allpass", "Right Tap", "tap", monoPorts, rightTapParameters },
    RuntimeNodeDefinition { "output", "stereo-output", "Stereo Output", "io", outputPorts, noParameters },
};

constexpr std::array connections {
    RuntimeConnectionDefinition { "input-l-to-sum", "input", "out-l", "sum", "in-l" },
    RuntimeConnectionDefinition { "input-r-to-sum", "input", "out-r", "sum", "in-r" },
    RuntimeConnectionDefinition { "sum-to-filter", "sum", "out", "input-filter", "in" },
    RuntimeConnectionDefinition { "filter-to-diffuser-1", "input-filter", "out", "diffuser-1", "in" },
    RuntimeConnectionDefinition { "diffuser-1-to-diffuser-2", "diffuser-1", "out", "diffuser-2", "in" },
    RuntimeConnectionDefinition { "diffuser-2-to-tank-1", "diffuser-2", "out", "tank-1", "in" },
    RuntimeConnectionDefinition { "tank-1-to-tank-2", "tank-1", "out", "tank-2", "in" },
    RuntimeConnectionDefinition { "tank-to-left", "tank-2", "out", "left-tap", "in" },
    RuntimeConnectionDefinition { "tank-to-right", "tank-2", "out", "right-tap", "in" },
    RuntimeConnectionDefinition { "left-to-output", "left-tap", "out", "output", "in-l" },
    RuntimeConnectionDefinition { "right-to-output", "right-tap", "out", "output", "in-r" },
};

} // namespace

std::span<const RuntimeNodeDefinition> barrReferenceRuntimeNodes() noexcept { return nodes; }
std::span<const RuntimeConnectionDefinition> barrReferenceRuntimeConnections() noexcept { return connections; }

double barrReferenceParameter(const std::string_view nodeId, const std::string_view parameterId)
{
    for (const auto& node : nodes) {
        if (node.id != nodeId)
            continue;
        for (const auto& parameter : node.parameters) {
            if (parameter.id == parameterId)
                return parameter.value;
        }
    }
    throw std::invalid_argument(
        "unknown Barr runtime parameter '" + std::string(nodeId) + "." + std::string(parameterId) + "'");
}

const RuntimeParameterDefinition& barrReferenceParameterDefinition(const BarrParameterId id)
{
    for (const auto& node : nodes) {
        for (const auto& parameter : node.parameters) {
            if (parameter.runtimeId == id)
                return parameter;
        }
    }
    throw std::invalid_argument("unknown Barr runtime parameter enum");
}

std::optional<BarrParameterId> findBarrReferenceParameter(
    const std::string_view nodeId, const std::string_view parameterId) noexcept
{
    for (const auto& node : nodes) {
        if (node.id != nodeId)
            continue;
        for (const auto& parameter : node.parameters) {
            if (parameter.id == parameterId)
                return parameter.runtimeId;
        }
    }
    return std::nullopt;
}

} // namespace reverb::dsp
