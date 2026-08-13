#include <reverb/graph/GravityDiffusionGraph.h>

#include <algorithm>
#include <array>
#include <string>

namespace reverb::graph {
namespace {

Port audioIn(std::string id = "in") { return { std::move(id), SignalType::audio, PortDirection::input }; }
Port audioOut(std::string id = "out") { return { std::move(id), SignalType::audio, PortDirection::output }; }
Port controlIn(std::string id = "in") { return { std::move(id), SignalType::control, PortDirection::input }; }
Port controlOut() { return { "out", SignalType::control, PortDirection::output }; }
Node gain(std::string id, double value, bool modulated = false)
{
    auto ports = std::vector { audioIn(), controlIn("gain-mod"), audioOut() };
    std::optional<ParameterModulation> modulation = modulated
        ? ParameterModulation { "gain-mod", 1.0, ModulationPolarity::bipolar, 0.0, 0.25 }
        : ParameterModulation { "gain-mod", 0.5, ModulationPolarity::bipolar, -1.0, 1.0 };
    return { std::move(id), "gain", std::move(ports), { { "gain", value, "linear", modulation } } };
}
Node feedbackGain()
{
    return { "feedback-gain", "gain", { audioIn(), controlIn("gain-mod"), audioOut() }, {
        { "gain", 0.58, "linear", ParameterModulation {
            "gain-mod", 0.23, ModulationPolarity::bipolar, 0.35, 0.81 } },
    } };
}
Node sum(std::string id) { return { std::move(id), "sum", { audioIn("in-a"), audioIn("in-b"), audioOut() }, {} }; }
Node delay(std::string id, double milliseconds, bool sized = false)
{
    auto ports = std::vector { audioIn(), controlIn("delay-mod"), audioOut() };
    std::optional<ParameterModulation> modulation = ParameterModulation {
        "delay-mod", 10.0, ModulationPolarity::bipolar, 0.1, 10'000.0 };
    if (sized) {
        modulation = ParameterModulation { "delay-mod", milliseconds * 0.35, ModulationPolarity::bipolar,
            milliseconds * 0.65, milliseconds * 1.35 };
    }
    return { std::move(id), "delay", std::move(ports), { { "delay", milliseconds, "milliseconds", modulation } } };
}
Node allpass(std::string id, double milliseconds, bool sized = false, bool moving = false, bool diffusionControlled = false)
{
    auto ports = std::vector { audioIn(), controlIn("delay-mod"), controlIn("coefficient-mod"), audioOut() };
    std::optional<ParameterModulation> modulation = ParameterModulation {
        "delay-mod", 2.0, ModulationPolarity::bipolar, 0.1, 100.0 };
    if (sized || moving) {
        const auto amount = sized ? milliseconds * 0.35 : 1.25;
        modulation = ParameterModulation { "delay-mod", amount, ModulationPolarity::bipolar,
            sized ? milliseconds * 0.65 : milliseconds - 1.25,
            sized ? milliseconds * 1.35 : milliseconds + 1.25 };
    }
    std::optional<ParameterModulation> coefficientModulation = ParameterModulation {
        "coefficient-mod", 0.25, ModulationPolarity::bipolar, -0.95, 0.95 };
    if (diffusionControlled) {
        coefficientModulation = ParameterModulation {
            "coefficient-mod", 0.18, ModulationPolarity::bipolar, 0.32, 0.68 };
    }
    return { std::move(id), "allpass", std::move(ports), {
        { "delay", milliseconds, "milliseconds", modulation },
        { "coefficient", 0.5, "unitless", coefficientModulation } } };
}
Node macro(std::string id, std::string name, double value, std::string presentation = {})
{
    value = std::clamp(value, -1.0, 1.0);
    return { std::move(id), "macro", { controlOut() }, {
        { "value", value, "normalized" }, { "default-value", value, "normalized" },
        { "center-detent", 1.0, "boolean" },
    }, std::move(name), std::move(presentation) };
}
Connection cable(std::string id, std::string from, std::string fromPort, std::string to, std::string toPort)
{ return { std::move(id), { std::move(from), std::move(fromPort) }, { std::move(to), std::move(toPort) } }; }

} // namespace

std::array<double, 8> gravityTapWeights(const double gravity) noexcept
{
    std::array<double, 8> weights {};
    const auto bounded = std::clamp(gravity, -1.0, 1.0);
    for (std::size_t index = 0; index < weights.size(); ++index)
        weights[index] = gravityTapBases[index] + gravityTapSlopes[index] * bounded;
    return weights;
}

GraphDocument makeGravityDiffusionGraph(const double gravity)
{
    auto graph = makeGravityDiffusionGraph(GravityDiffusionControls { .gravity = gravity });
    const auto complementary = [](const std::string_view id) {
        return id == "size" || id == "feedback" || id == "damping" || id == "modulation"
            || id == "motion-a" || id == "motion-b";
    };
    std::erase_if(graph.nodes, [&](const Node& node) { return complementary(node.id); });
    std::erase_if(graph.connections, [&](const Connection& connection) {
        return complementary(connection.from.nodeId) || complementary(connection.to.nodeId);
    });
    std::erase_if(graph.layout.nodes, [&](const NodePosition& position) { return complementary(position.nodeId); });
    for (auto& node : graph.nodes) {
        const auto complementaryTarget = node.type == "delay" || node.type == "allpass"
            || node.type == "lowpass" || node.id == "feedback-gain";
        if (!complementaryTarget) continue;
        std::erase_if(node.ports, [](const Port& port) { return port.signal == SignalType::control; });
        for (auto& parameter : node.parameters) parameter.modulation.reset();
    }
    return graph;
}

GraphDocument makeGravityDiffusionGraph(const GravityDiffusionControls& controls)
{
    GraphDocument graph;
    graph.nodes = {
        { "input", "stereo-input", { audioOut("out-l"), audioOut("out-r") }, {} },
        gain("input-l-half", 0.5), gain("input-r-half", 0.5), sum("input-sum"),
        allpass("input-ap-1", 3.1, true, false, true), allpass("input-ap-2", 4.7, true, false, true),
        allpass("input-ap-3", 7.9, true, false, true), allpass("input-ap-4", 11.3, true, false, true), sum("tank-entry"),
        macro("gravity", "Gravity", controls.gravity, "gravity"),
        macro("size", "Size", controls.size), macro("feedback", "Feedback", controls.feedback),
        macro("damping", "Damping", controls.damping), macro("modulation", "Modulation", controls.modulation),
        { "motion-a", "lfo", { controlIn("frequency-mod"), controlIn("phase-mod"), controlIn("waveform-mod"), controlIn("run-mode-mod"), controlOut() }, {
            { "frequency", 0.11, "hertz", ParameterModulation { "frequency-mod", 1.0, ModulationPolarity::bipolar, 0.01, 100.0 } },
            { "phase", 0.0, "cycles", ParameterModulation { "phase-mod", 0.25, ModulationPolarity::bipolar, 0.0, 0.999 } },
            { "waveform", 0.0, "waveform", ParameterModulation { "waveform-mod", 1.0, ModulationPolarity::bipolar, 0.0, 1.0 } },
            { "run-mode", 0.0, "run-mode", ParameterModulation { "run-mode-mod", 1.0, ModulationPolarity::bipolar, 0.0, 1.0 } },
        } },
        { "motion-b", "lfo", { controlIn("frequency-mod"), controlIn("phase-mod"), controlIn("waveform-mod"), controlIn("run-mode-mod"), controlOut() }, {
            { "frequency", 0.073, "hertz", ParameterModulation { "frequency-mod", 1.0, ModulationPolarity::bipolar, 0.01, 100.0 } },
            { "phase", 0.25, "cycles", ParameterModulation { "phase-mod", 0.25, ModulationPolarity::bipolar, 0.0, 0.999 } },
            { "waveform", 1.0, "waveform", ParameterModulation { "waveform-mod", 1.0, ModulationPolarity::bipolar, 0.0, 1.0 } },
            { "run-mode", 0.0, "run-mode", ParameterModulation { "run-mode-mod", 1.0, ModulationPolarity::bipolar, 0.0, 1.0 } },
        } },
    };
    const std::array stageDelays { 23.0, 29.0, 37.0, 43.0, 53.0, 61.0, 71.0, 83.0 };
    const std::array stageAllpasses { 13.7, 17.9, 19.3, 23.1, 29.7, 31.1, 37.1, 41.3 };
    for (std::size_t index = 0; index < stageDelays.size(); ++index) {
        const auto number = std::to_string(index + 1);
        graph.nodes.push_back(delay("stage-delay-" + number, stageDelays[index], true));
        graph.nodes.push_back(allpass("stage-ap-" + number, stageAllpasses[index], false, true));
        graph.nodes.push_back(gain("tap-gain-" + number, gravityTapBases[index], true));
        graph.nodes.push_back({ "gravity-map-" + number, "control-map", {
            controlIn(), controlIn("scale-mod"), controlIn("offset-mod"), controlIn("polarity-mod"), controlOut() }, {
            { "scale", gravityTapSlopes[index], "linear", ParameterModulation { "scale-mod", 1.0, ModulationPolarity::bipolar, -4.0, 4.0 } },
            { "offset", 0.0, "unitless", ParameterModulation { "offset-mod", 0.5, ModulationPolarity::bipolar, -1.0, 1.0 } },
            { "polarity", 1.0, "polarity", ParameterModulation { "polarity-mod", 1.0, ModulationPolarity::bipolar, 0.0, 1.0 } },
            { "curve-family", 0.0, "curve-family" },
            { "curve-amount", 0.0, "unitless" }, { "exponent", 1.0, "unitless" },
            { "clamp-min", -0.25, "unitless" }, { "clamp-max", 0.25, "unitless" },
        } });
    }
    graph.nodes.insert(graph.nodes.end(), {
        { "feedback-damping", "lowpass", { audioIn(), controlIn("cutoff-mod"), audioOut() }, {
            { "cutoff", 5'800.0, "hertz", ParameterModulation { "cutoff-mod", -4'200.0,
                ModulationPolarity::bipolar, 1'600.0, 10'000.0 } } } },
        feedbackGain(), delay("feedback-delay", 97.0, true),
        sum("left-sum-a"), sum("left-sum-b"), sum("left-sum"),
        sum("right-sum-a"), sum("right-sum-b"), sum("right-sum"),
        { "output", "stereo-output", { audioIn("in-l"), audioIn("in-r") }, {} },
    });
    graph.connections = {
        cable("input-l-gain", "input", "out-l", "input-l-half", "in"), cable("input-r-gain", "input", "out-r", "input-r-half", "in"),
        cable("input-l-sum", "input-l-half", "out", "input-sum", "in-a"), cable("input-r-sum", "input-r-half", "out", "input-sum", "in-b"),
        cable("input-ap-1", "input-sum", "out", "input-ap-1", "in"), cable("input-ap-2", "input-ap-1", "out", "input-ap-2", "in"),
        cable("input-ap-3", "input-ap-2", "out", "input-ap-3", "in"), cable("input-ap-4", "input-ap-3", "out", "input-ap-4", "in"),
        cable("input-tank", "input-ap-4", "out", "tank-entry", "in-a"), cable("feedback-tank", "feedback-delay", "out", "tank-entry", "in-b"),
        cable("feedback-control", "feedback", "out", "feedback-gain", "gain-mod"),
        cable("damping-control", "damping", "out", "feedback-damping", "cutoff-mod"),
    };
    for (const auto number : { "1", "2", "3", "4" }) {
        graph.connections.push_back(cable("size-input-ap-" + std::string(number), "size", "out", "input-ap-" + std::string(number), "delay-mod"));
        graph.connections.push_back(cable("modulation-input-ap-" + std::string(number), "modulation", "out", "input-ap-" + std::string(number), "coefficient-mod"));
    }
    auto previous = std::string("tank-entry");
    for (std::size_t index = 0; index < stageDelays.size(); ++index) {
        const auto number = std::to_string(index + 1);
        graph.connections.push_back(cable("stage-in-" + number, previous, "out", "stage-delay-" + number, "in"));
        graph.connections.push_back(cable("stage-diffuse-" + number, "stage-delay-" + number, "out", "stage-ap-" + number, "in"));
        graph.connections.push_back(cable("tap-" + number, "stage-ap-" + number, "out", "tap-gain-" + number, "in"));
        graph.connections.push_back(cable("gravity-map-in-" + number, "gravity", "out", "gravity-map-" + number, "in"));
        graph.connections.push_back(cable("gravity-map-out-" + number, "gravity-map-" + number, "out", "tap-gain-" + number, "gain-mod"));
        graph.connections.push_back(cable("size-stage-" + number, "size", "out", "stage-delay-" + number, "delay-mod"));
        graph.connections.push_back(cable("motion-stage-" + number, index % 2 == 0 ? "motion-a" : "motion-b", "out", "stage-ap-" + number, "delay-mod"));
        previous = "stage-ap-" + number;
    }
    graph.connections.insert(graph.connections.end(), {
        cable("stage-8-damping", "stage-ap-8", "out", "feedback-damping", "in"), cable("damping-feedback", "feedback-damping", "out", "feedback-gain", "in"),
        cable("feedback-return-delay", "feedback-gain", "out", "feedback-delay", "in"),
        cable("size-feedback-delay", "size", "out", "feedback-delay", "delay-mod"),
        cable("tap-1-left-a", "tap-gain-1", "out", "left-sum-a", "in-a"), cable("tap-3-left-a", "tap-gain-3", "out", "left-sum-a", "in-b"),
        cable("tap-5-left-b", "tap-gain-5", "out", "left-sum-b", "in-a"), cable("tap-7-left-b", "tap-gain-7", "out", "left-sum-b", "in-b"),
        cable("left-a-final", "left-sum-a", "out", "left-sum", "in-a"), cable("left-b-final", "left-sum-b", "out", "left-sum", "in-b"),
        cable("tap-2-right-a", "tap-gain-2", "out", "right-sum-a", "in-a"), cable("tap-4-right-a", "tap-gain-4", "out", "right-sum-a", "in-b"),
        cable("tap-6-right-b", "tap-gain-6", "out", "right-sum-b", "in-a"), cable("tap-8-right-b", "tap-gain-8", "out", "right-sum-b", "in-b"),
        cable("right-a-final", "right-sum-a", "out", "right-sum", "in-a"), cable("right-b-final", "right-sum-b", "out", "right-sum", "in-b"),
        cable("left-output", "left-sum", "out", "output", "in-l"), cable("right-output", "right-sum", "out", "output", "in-r"),
    });
    graph.layout.nodes = {
        { "input", -760.0, -180.0 }, { "input-l-half", -580.0, -240.0 },
        { "input-r-half", -580.0, -100.0 }, { "input-sum", -380.0, -180.0 },
        { "input-ap-1", -180.0, -180.0 }, { "input-ap-2", 20.0, -180.0 },
        { "input-ap-3", 220.0, -180.0 }, { "input-ap-4", 420.0, -180.0 },
        { "tank-entry", 640.0, -180.0 },
        { "gravity", -560.0, 500.0 }, { "size", -800.0, 240.0 },
        { "feedback", -800.0, 390.0 }, { "damping", -800.0, 540.0 },
        { "modulation", -800.0, 690.0 }, { "motion-a", -560.0, 1'360.0 },
        { "motion-b", -320.0, 1'360.0 },
        { "left-sum-a", 800.0, 330.0 }, { "left-sum-b", 800.0, 610.0 },
        { "left-sum", 1'020.0, 470.0 }, { "right-sum-a", 800.0, 750.0 },
        { "right-sum-b", 800.0, 1'030.0 }, { "right-sum", 1'020.0, 890.0 },
        { "output", 1'250.0, 680.0 }, { "feedback-damping", 700.0, 1'360.0 },
        { "feedback-gain", 470.0, 1'360.0 }, { "feedback-delay", 230.0, 1'360.0 },
    };
    for (std::size_t index = 0; index < 8; ++index) {
        const auto number = std::to_string(index + 1);
        const auto y = 100.0 + 150.0 * static_cast<double>(index);
        graph.layout.nodes.push_back({ "gravity-map-" + number, -320.0, y });
        graph.layout.nodes.push_back({ "stage-delay-" + number, -80.0, y });
        graph.layout.nodes.push_back({ "stage-ap-" + number, 160.0, y });
        graph.layout.nodes.push_back({ "tap-gain-" + number, 430.0, y });
    }
    graph.layout.viewport = { 640.0, 210.0, 0.52 };
    return graph;
}

} // namespace reverb::graph
