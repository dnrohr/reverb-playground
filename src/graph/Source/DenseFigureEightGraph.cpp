#include <reverb/graph/DenseFigureEightGraph.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace reverb::graph {
namespace {

Port audioIn(std::string id = "in") { return { std::move(id), SignalType::audio, PortDirection::input }; }
Port audioOut(std::string id = "out") { return { std::move(id), SignalType::audio, PortDirection::output }; }
Port controlIn(std::string id) { return { std::move(id), SignalType::control, PortDirection::input }; }
Port controlOut() { return { "out", SignalType::control, PortDirection::output }; }

Node gain(std::string id, const double value, const double maximum = 1.0)
{
    return { std::move(id), "gain", { audioIn(), controlIn("gain-mod"), audioOut() }, {
        { "gain", value, "linear", ParameterModulation {
            "gain-mod", maximum, ModulationPolarity::unipolar, 0.0, maximum } },
    } };
}

Node returnGain(std::string id, const double value)
{
    const auto sign = value < 0.0 ? -1.0 : 1.0;
    return { std::move(id), "gain", { audioIn(), controlIn("gain-mod"), audioOut() }, {
        { "gain", value, "linear", ParameterModulation {
            "gain-mod", sign * std::abs(value) * 0.14, ModulationPolarity::bipolar,
            sign < 0.0 ? -0.98 : 0.0, sign < 0.0 ? 0.0 : 0.98 } },
    } };
}

Node sum(std::string id) { return { std::move(id), "sum", { audioIn("in-a"), audioIn("in-b"), audioOut() }, {} }; }

Node delay(std::string id, const double milliseconds)
{
    return { std::move(id), "delay", { audioIn(), controlIn("delay-mod"), audioOut() }, {
        { "delay", milliseconds, "milliseconds", ParameterModulation {
            "delay-mod", 2.0, ModulationPolarity::bipolar, milliseconds - 2.0, milliseconds + 2.0 } },
    } };
}

Node allpass(std::string id, const double milliseconds, const double modulationDepth)
{
    const auto clampDepth = std::max(modulationDepth, 0.001);
    return { std::move(id), "allpass", {
        audioIn(), controlIn("delay-mod"), controlIn("coefficient-mod"), audioOut() }, {
        { "delay", milliseconds, "milliseconds", ParameterModulation {
            "delay-mod", modulationDepth, ModulationPolarity::bipolar,
            milliseconds - clampDepth, milliseconds + clampDepth } },
        { "coefficient", 0.57, "unitless", ParameterModulation {
            "coefficient-mod", 0.15, ModulationPolarity::bipolar, 0.35, 0.72 } },
    } };
}

Node lowpass(std::string id, const double cutoff)
{
    return { std::move(id), "lowpass", { audioIn(), controlIn("cutoff-mod"), audioOut() }, {
        { "cutoff", cutoff, "hertz", ParameterModulation {
            "cutoff-mod", 4'000.0, ModulationPolarity::bipolar, 1'000.0, 12'000.0 } },
    } };
}

Node lfo(std::string id, const double frequency, const double phase)
{
    return { std::move(id), "lfo", { controlIn("frequency-mod"), controlIn("phase-mod"),
        controlIn("waveform-mod"), controlIn("run-mode-mod"), controlOut() }, {
        { "frequency", frequency, "hertz", ParameterModulation { "frequency-mod", 0.15,
            ModulationPolarity::bipolar, 0.01, 0.35 } },
        { "phase", phase, "cycles", ParameterModulation { "phase-mod", 0.25,
            ModulationPolarity::bipolar, 0.0, 0.999 } },
        { "waveform", 0.0, "waveform", ParameterModulation { "waveform-mod", 1.0,
            ModulationPolarity::bipolar, 0.0, 1.0 } },
        { "run-mode", 0.0, "run-mode", ParameterModulation { "run-mode-mod", 1.0,
            ModulationPolarity::bipolar, 0.0, 1.0 } },
    } };
}

Node macro(std::string id, std::string name)
{
    return { std::move(id), "macro", { controlOut() }, {
        { "value", 0.0, "normalized" }, { "default-value", 0.0, "normalized" },
        { "center-detent", 1.0, "boolean" },
    }, std::move(name) };
}

Connection cable(std::string id, std::string from, std::string fromPort, std::string to, std::string toPort)
{
    return { std::move(id), { std::move(from), std::move(fromPort) },
        { std::move(to), std::move(toPort) } };
}

} // namespace

double feedbackGainForRt60(const double traversalMilliseconds, const double rt60Seconds) noexcept
{
    if (!std::isfinite(traversalMilliseconds) || !std::isfinite(rt60Seconds)
        || traversalMilliseconds <= 0.0 || rt60Seconds <= 0.0) return 0.0;
    return std::clamp(std::pow(10.0, -3.0 * traversalMilliseconds / (1'000.0 * rt60Seconds)), 0.0, 0.98);
}

GraphDocument makeDenseFigureEightGraph(const DenseFigureEightControls& requested)
{
    const auto rt60 = std::clamp(requested.rt60Seconds, 0.4, 8.0);
    const auto damping = std::clamp(requested.dampingHertz, 1'200.0, 12'000.0);
    const auto modulation = std::clamp(requested.modulationDepthMilliseconds, 0.0, 1.5);
    const auto wet = std::clamp(requested.wetLevel, 0.0, 0.75);
    constexpr double branchATraversal = 7.3 + 13.7 + 19.9 + 71.1 + 97.3;
    constexpr double branchBTraversal = 8.9 + 17.3 + 23.9 + 83.7 + 109.1;

    GraphDocument graph;
    graph.nodes = {
        { "input", "stereo-input", { audioOut("out-l"), audioOut("out-r") }, {} },
        gain("input-left-half", 0.5), gain("input-right-half", 0.5), sum("input-mono"),
        allpass("input-ap-1", 3.1, 0.0), allpass("input-ap-2", 4.7, 0.0),
        allpass("input-ap-3", 7.9, 0.0), allpass("input-ap-4", 11.3, 0.0),
        gain("inject-a", 0.42), gain("inject-b", -0.42), sum("entry-a"), sum("entry-b"),
        allpass("a-ap-1", 7.3, modulation), delay("a-delay-1", 71.1),
        allpass("a-ap-2", 13.7, modulation), delay("a-delay-2", 97.3),
        allpass("a-ap-3", 19.9, modulation), lowpass("a-damping", damping),
        returnGain("a-cross-gain", feedbackGainForRt60(branchATraversal, rt60)),
        allpass("b-ap-1", 8.9, modulation), delay("b-delay-1", 83.7),
        allpass("b-ap-2", 17.3, modulation), delay("b-delay-2", 109.1),
        allpass("b-ap-3", 23.9, modulation), lowpass("b-damping", damping * 0.93),
        returnGain("b-cross-gain", -feedbackGainForRt60(branchBTraversal, rt60)),
        lfo("motion-a", 0.113, 0.0), lfo("motion-b", 0.071, 0.31),
        macro("decay-macro", "Decay"), macro("tone-macro", "Tone"), macro("motion-macro", "Motion"),
        gain("left-a", wet * 0.72), gain("left-b", wet * 0.28), sum("left-sum"),
        gain("right-a", -wet * 0.24), gain("right-b", wet * 0.76), sum("right-sum"),
        allpass("left-extraction", 11.9, modulation * 0.5),
        allpass("right-extraction", 17.9, modulation * 0.5),
        { "output", "stereo-output", { audioIn("in-l"), audioIn("in-r") }, {} },
    };

    graph.connections = {
        cable("input-left", "input", "out-l", "input-left-half", "in"),
        cable("input-right", "input", "out-r", "input-right-half", "in"),
        cable("input-left-sum", "input-left-half", "out", "input-mono", "in-a"),
        cable("input-right-sum", "input-right-half", "out", "input-mono", "in-b"),
        cable("input-ap-1", "input-mono", "out", "input-ap-1", "in"),
        cable("input-ap-2", "input-ap-1", "out", "input-ap-2", "in"),
        cable("input-ap-3", "input-ap-2", "out", "input-ap-3", "in"),
        cable("input-ap-4", "input-ap-3", "out", "input-ap-4", "in"),
        cable("inject-a", "input-ap-4", "out", "inject-a", "in"),
        cable("inject-b", "input-ap-4", "out", "inject-b", "in"),
        cable("inject-entry-a", "inject-a", "out", "entry-a", "in-a"),
        cable("inject-entry-b", "inject-b", "out", "entry-b", "in-a"),
        cable("b-cross-entry-a", "b-cross-gain", "out", "entry-a", "in-b"),
        cable("a-cross-entry-b", "a-cross-gain", "out", "entry-b", "in-b"),
        cable("a-ap-1", "entry-a", "out", "a-ap-1", "in"),
        cable("a-delay-1", "a-ap-1", "out", "a-delay-1", "in"),
        cable("a-ap-2", "a-delay-1", "out", "a-ap-2", "in"),
        cable("a-delay-2", "a-ap-2", "out", "a-delay-2", "in"),
        cable("a-ap-3", "a-delay-2", "out", "a-ap-3", "in"),
        cable("a-damping", "a-ap-3", "out", "a-damping", "in"),
        cable("a-cross", "a-damping", "out", "a-cross-gain", "in"),
        cable("b-ap-1", "entry-b", "out", "b-ap-1", "in"),
        cable("b-delay-1", "b-ap-1", "out", "b-delay-1", "in"),
        cable("b-ap-2", "b-delay-1", "out", "b-ap-2", "in"),
        cable("b-delay-2", "b-ap-2", "out", "b-delay-2", "in"),
        cable("b-ap-3", "b-delay-2", "out", "b-ap-3", "in"),
        cable("b-damping", "b-ap-3", "out", "b-damping", "in"),
        cable("b-cross", "b-damping", "out", "b-cross-gain", "in"),
        cable("motion-a-a1", "motion-a", "out", "a-ap-1", "delay-mod"),
        cable("motion-a-a3", "motion-a", "out", "a-ap-3", "delay-mod"),
        cable("motion-b-b1", "motion-b", "out", "b-ap-1", "delay-mod"),
        cable("motion-b-b3", "motion-b", "out", "b-ap-3", "delay-mod"),
        cable("decay-a", "decay-macro", "out", "a-cross-gain", "gain-mod"),
        cable("decay-b", "decay-macro", "out", "b-cross-gain", "gain-mod"),
        cable("tone-a", "tone-macro", "out", "a-damping", "cutoff-mod"),
        cable("tone-b", "tone-macro", "out", "b-damping", "cutoff-mod"),
        cable("motion-rate-a", "motion-macro", "out", "motion-a", "frequency-mod"),
        cable("motion-rate-b", "motion-macro", "out", "motion-b", "frequency-mod"),
        cable("left-a", "a-damping", "out", "left-a", "in"),
        cable("left-b", "b-delay-2", "out", "left-b", "in"),
        cable("left-a-sum", "left-a", "out", "left-sum", "in-a"),
        cable("left-b-sum", "left-b", "out", "left-sum", "in-b"),
        cable("right-a", "a-delay-2", "out", "right-a", "in"),
        cable("right-b", "b-damping", "out", "right-b", "in"),
        cable("right-a-sum", "right-a", "out", "right-sum", "in-a"),
        cable("right-b-sum", "right-b", "out", "right-sum", "in-b"),
        cable("left-extraction", "left-sum", "out", "left-extraction", "in"),
        cable("right-extraction", "right-sum", "out", "right-extraction", "in"),
        cable("left-output", "left-extraction", "out", "output", "in-l"),
        cable("right-output", "right-extraction", "out", "output", "in-r"),
    };

    const std::array names {
        std::pair { "entry-a", "Branch A entry" }, std::pair { "entry-b", "Branch B entry" },
        std::pair { "a-cross-gain", "A to B RT60 return" }, std::pair { "b-cross-gain", "B to A RT60 return" },
        std::pair { "a-damping", "Branch A damping" }, std::pair { "b-damping", "Branch B damping" },
        std::pair { "left-extraction", "Left unequal tap mix" }, std::pair { "right-extraction", "Right unequal tap mix" },
    };
    for (const auto& [id, name] : names) {
        const auto found = std::ranges::find(graph.nodes, std::string_view(id), &Node::id);
        if (found != graph.nodes.end()) found->name = name;
    }

    graph.layout.nodes = {
        { "input", -1'260.0, -120.0 }, { "input-left-half", -1'080.0, -210.0 },
        { "input-right-half", -1'080.0, -30.0 }, { "input-mono", -870.0, -120.0 },
        { "input-ap-1", -650.0, -120.0 }, { "input-ap-2", -430.0, -120.0 },
        { "input-ap-3", -210.0, -120.0 }, { "input-ap-4", 10.0, -120.0 },
        { "inject-a", 230.0, -210.0 }, { "inject-b", 230.0, 390.0 },
        { "entry-a", 450.0, -210.0 }, { "a-ap-1", 670.0, -210.0 }, { "a-delay-1", 890.0, -210.0 },
        { "a-ap-2", 1'110.0, -210.0 }, { "a-delay-2", 1'330.0, -210.0 },
        { "a-ap-3", 1'550.0, -210.0 }, { "a-damping", 1'770.0, -210.0 }, { "a-cross-gain", 1'770.0, 90.0 },
        { "entry-b", 450.0, 390.0 }, { "b-ap-1", 670.0, 390.0 }, { "b-delay-1", 890.0, 390.0 },
        { "b-ap-2", 1'110.0, 390.0 }, { "b-delay-2", 1'330.0, 390.0 },
        { "b-ap-3", 1'550.0, 390.0 }, { "b-damping", 1'770.0, 390.0 }, { "b-cross-gain", 450.0, 90.0 },
        { "motion-a", 690.0, 690.0 }, { "motion-b", 930.0, 690.0 },
        { "decay-macro", 1'230.0, 690.0 }, { "tone-macro", 1'450.0, 690.0 },
        { "motion-macro", 1'670.0, 690.0 },
        { "left-a", 2'030.0, -310.0 }, { "left-b", 2'030.0, -110.0 }, { "left-sum", 2'250.0, -210.0 },
        { "right-a", 2'030.0, 290.0 }, { "right-b", 2'030.0, 490.0 }, { "right-sum", 2'250.0, 390.0 },
        { "left-extraction", 2'470.0, -210.0 }, { "right-extraction", 2'470.0, 390.0 },
        { "output", 2'700.0, 90.0 },
    };
    graph.layout.viewport = { 260.0, 120.0, 0.42 };
    return graph;
}

} // namespace reverb::graph
