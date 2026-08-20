#include <reverb/graph/SplitFeedbackShimmerGraph.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace reverb::graph {
namespace {

Port audioIn(std::string id = "in") { return { std::move(id), SignalType::audio, PortDirection::input }; }
Port audioOut(std::string id = "out") { return { std::move(id), SignalType::audio, PortDirection::output }; }
Port controlIn(std::string id) { return { std::move(id), SignalType::control, PortDirection::input }; }

Node gain(std::string id, const double value)
{
    return { std::move(id), "gain", { audioIn(), controlIn("gain-mod"), audioOut() }, {
        { "gain", value, "linear", ParameterModulation {
            "gain-mod", 0.5, ModulationPolarity::bipolar, -1.0, 1.0 } },
    } };
}

Node boundedGain(std::string id, const double value, const double clampMaximum)
{
    return { std::move(id), "gain", { audioIn(), controlIn("gain-mod"), audioOut() }, {
        { "gain", value, "linear", ParameterModulation {
            "gain-mod", clampMaximum, ModulationPolarity::unipolar, 0.0, clampMaximum } },
    } };
}

Node sum(std::string id)
{
    return { std::move(id), "sum", { audioIn("in-a"), audioIn("in-b"), audioOut() }, {} };
}

Node delay(std::string id, const double milliseconds)
{
    return { std::move(id), "delay", { audioIn(), controlIn("delay-mod"), audioOut() }, {
        { "delay", milliseconds, "milliseconds", ParameterModulation {
            "delay-mod", 10.0, ModulationPolarity::bipolar, 0.1, 10'000.0 } },
    } };
}

Node allpass(std::string id, const double milliseconds, const double coefficient = 0.5)
{
    return { std::move(id), "allpass", {
        audioIn(), controlIn("delay-mod"), controlIn("coefficient-mod"), audioOut() }, {
        { "delay", milliseconds, "milliseconds", ParameterModulation {
            "delay-mod", 2.0, ModulationPolarity::bipolar, 0.1, 100.0 } },
        { "coefficient", coefficient, "unitless", ParameterModulation {
            "coefficient-mod", 0.25, ModulationPolarity::bipolar, -0.95, 0.95 } },
    } };
}

Node lowpass(std::string id, const double cutoffHertz)
{
    return { std::move(id), "lowpass", { audioIn(), controlIn("cutoff-mod"), audioOut() }, {
        { "cutoff", cutoffHertz, "hertz", ParameterModulation {
            "cutoff-mod", 2'000.0, ModulationPolarity::bipolar, 20.0, 20'000.0 } },
    } };
}

Node pitchShift(const double semitones)
{
    return { "shifted-pitch", "pitch-shift", {
        audioIn(), controlIn("semitones-mod"), controlIn("grain-mod"),
        controlIn("overlap-mod"), audioOut(),
    }, {
        { "semitones", semitones, "semitones", ParameterModulation {
            "semitones-mod", 12.0, ModulationPolarity::bipolar, -24.0, 24.0 } },
        { "grain", 60.0, "milliseconds", ParameterModulation {
            "grain-mod", 20.0, ModulationPolarity::bipolar, 20.0, 120.0 } },
        { "overlap", 0.5, "normalized", ParameterModulation {
            "overlap-mod", 0.25, ModulationPolarity::bipolar, 0.1, 1.0 } },
        { "direction", 0.0, "direction" },
    } };
}

Connection cable(std::string id, std::string from, std::string fromPort, std::string to, std::string toPort)
{
    return { std::move(id), { std::move(from), std::move(fromPort) },
        { std::move(to), std::move(toPort) } };
}

} // namespace

GraphDocument makeSplitFeedbackShimmerGraph(const SplitFeedbackShimmerControls& requested)
{
    const SplitFeedbackShimmerControls controls {
        .normalFeedback = std::clamp(requested.normalFeedback, 0.0,
            splitShimmerMaximumNormalFeedback),
        .shiftedFeedback = std::clamp(requested.shiftedFeedback, 0.0,
            splitShimmerMaximumShiftedFeedback),
        .preShiftHighpassHertz = std::clamp(requested.preShiftHighpassHertz, 120.0, 1'200.0),
        .postShiftLowpassHertz = std::clamp(requested.postShiftLowpassHertz, 1'200.0, 9'000.0),
        .wetLevel = std::clamp(requested.wetLevel, 0.0, 0.75),
        .pitchSemitones = std::clamp(requested.pitchSemitones,
            splitShimmerMinimumPitchSemitones, splitShimmerMaximumPitchSemitones),
        .sizeMilliseconds = std::clamp(requested.sizeMilliseconds,
            splitShimmerMinimumSizeMilliseconds, splitShimmerMaximumSizeMilliseconds),
    };

    GraphDocument graph;
    graph.nodes = {
        { "input", "stereo-input", { audioOut("out-l"), audioOut("out-r") }, {} },
        gain("input-left-half", 0.5), gain("input-right-half", 0.5), sum("input-mono-sum"),
        allpass("input-diffusion-a", 4.7), allpass("input-diffusion-b", 8.9),
        sum("tank-entry"), allpass("tank-diffusion-a", 13.7),
        delay("tank-delay", controls.sizeMilliseconds),
        allpass("tank-diffusion-b", 23.9), lowpass("tank-damping", 6'500.0),
        boundedGain("normal-feedback", controls.normalFeedback, splitShimmerMaximumNormalFeedback),
        delay("normal-feedback-delay", 67.0),
        lowpass("shifted-highpass-lowpass", controls.preShiftHighpassHertz),
        gain("shifted-highpass-invert", -1.0), sum("shifted-highpass-sum"),
        pitchShift(controls.pitchSemitones), lowpass("shifted-damping", controls.postShiftLowpassHertz),
        boundedGain("shifted-feedback", controls.shiftedFeedback, splitShimmerMaximumShiftedFeedback),
        delay("shifted-feedback-delay", 83.0), sum("feedback-recombine"),
        boundedGain("wet-level", controls.wetLevel, 0.75),
        allpass("left-extraction", 11.9), allpass("right-extraction", 19.7),
        { "output", "stereo-output", { audioIn("in-l"), audioIn("in-r") }, {} },
    };

    const std::array names {
        std::pair { "input-mono-sum", "Mono input" },
        std::pair { "tank-entry", "Shared tank entry" },
        std::pair { "tank-damping", "Tank damping" },
        std::pair { "normal-feedback", "Normal feedback" },
        std::pair { "normal-feedback-delay", "Normal return delay" },
        std::pair { "shifted-highpass-sum", "Pre-shift high-pass" },
        std::pair { "shifted-pitch", "Octave / +12 st" },
        std::pair { "shifted-damping", "Post-shift low-pass" },
        std::pair { "shifted-feedback", "Shifted feedback" },
        std::pair { "shifted-feedback-delay", "Shifted return delay" },
        std::pair { "feedback-recombine", "Feedback recombine" },
        std::pair { "wet-level", "Wet level" },
        std::pair { "left-extraction", "Left extraction" },
        std::pair { "right-extraction", "Right extraction" },
    };
    for (const auto& [id, name] : names) {
        const auto found = std::ranges::find(graph.nodes, std::string_view(id), &Node::id);
        if (found != graph.nodes.end()) found->name = name;
    }

    graph.connections = {
        cable("input-left-gain", "input", "out-l", "input-left-half", "in"),
        cable("input-right-gain", "input", "out-r", "input-right-half", "in"),
        cable("input-left-sum", "input-left-half", "out", "input-mono-sum", "in-a"),
        cable("input-right-sum", "input-right-half", "out", "input-mono-sum", "in-b"),
        cable("input-diffusion-a", "input-mono-sum", "out", "input-diffusion-a", "in"),
        cable("input-diffusion-b", "input-diffusion-a", "out", "input-diffusion-b", "in"),
        cable("input-tank", "input-diffusion-b", "out", "tank-entry", "in-a"),
        cable("feedback-tank", "feedback-recombine", "out", "tank-entry", "in-b"),
        cable("tank-diffusion-a", "tank-entry", "out", "tank-diffusion-a", "in"),
        cable("tank-delay", "tank-diffusion-a", "out", "tank-delay", "in"),
        cable("tank-diffusion-b", "tank-delay", "out", "tank-diffusion-b", "in"),
        cable("tank-damping", "tank-diffusion-b", "out", "tank-damping", "in"),
        cable("normal-feedback", "tank-damping", "out", "normal-feedback", "in"),
        cable("normal-feedback-delay", "normal-feedback", "out", "normal-feedback-delay", "in"),
        cable("normal-recombine", "normal-feedback-delay", "out", "feedback-recombine", "in-a"),
        cable("highpass-direct", "tank-damping", "out", "shifted-highpass-sum", "in-a"),
        cable("highpass-lowpass", "tank-damping", "out", "shifted-highpass-lowpass", "in"),
        cable("highpass-invert", "shifted-highpass-lowpass", "out", "shifted-highpass-invert", "in"),
        cable("highpass-subtract", "shifted-highpass-invert", "out", "shifted-highpass-sum", "in-b"),
        cable("shifted-pitch", "shifted-highpass-sum", "out", "shifted-pitch", "in"),
        cable("shifted-damping", "shifted-pitch", "out", "shifted-damping", "in"),
        cable("shifted-feedback", "shifted-damping", "out", "shifted-feedback", "in"),
        cable("shifted-feedback-delay", "shifted-feedback", "out", "shifted-feedback-delay", "in"),
        cable("shifted-recombine", "shifted-feedback-delay", "out", "feedback-recombine", "in-b"),
        cable("wet-level", "tank-damping", "out", "wet-level", "in"),
        cable("left-extraction", "wet-level", "out", "left-extraction", "in"),
        cable("right-extraction", "wet-level", "out", "right-extraction", "in"),
        cable("left-output", "left-extraction", "out", "output", "in-l"),
        cable("right-output", "right-extraction", "out", "output", "in-r"),
    };

    graph.layout.nodes = {
        { "input", -1'100.0, -120.0 }, { "input-left-half", -900.0, -200.0 },
        { "input-right-half", -900.0, -40.0 }, { "input-mono-sum", -680.0, -120.0 },
        { "input-diffusion-a", -460.0, -120.0 }, { "input-diffusion-b", -240.0, -120.0 },
        { "tank-entry", 0.0, -120.0 }, { "tank-diffusion-a", 220.0, -120.0 },
        { "tank-delay", 440.0, -120.0 }, { "tank-diffusion-b", 660.0, -120.0 },
        { "tank-damping", 880.0, -120.0 },
        { "normal-feedback", 840.0, 160.0 }, { "normal-feedback-delay", 580.0, 160.0 },
        { "shifted-highpass-lowpass", 1'060.0, 380.0 },
        { "shifted-highpass-invert", 840.0, 380.0 }, { "shifted-highpass-sum", 620.0, 300.0 },
        { "shifted-pitch", 400.0, 300.0 }, { "shifted-damping", 180.0, 300.0 },
        { "shifted-feedback", -40.0, 300.0 }, { "shifted-feedback-delay", -260.0, 300.0 },
        { "feedback-recombine", -260.0, 100.0 },
        { "wet-level", 1'120.0, -120.0 }, { "left-extraction", 1'360.0, -220.0 },
        { "right-extraction", 1'360.0, -20.0 }, { "output", 1'600.0, -120.0 },
    };
    graph.layout.viewport = { 160.0, 80.0, 0.48 };
    return graph;
}

} // namespace reverb::graph
