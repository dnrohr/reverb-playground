#include <reverb/graph/ReverseCosmicShimmerGraph.h>

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

Node gain(std::string id, const double value, const double minimum = -1.0, const double maximum = 1.0)
{
    return { std::move(id), "gain", { audioIn(), controlIn("gain-mod"), audioOut() }, {
        { "gain", value, "linear", ParameterModulation {
            "gain-mod", std::max(std::abs(minimum), std::abs(maximum)),
            ModulationPolarity::bipolar, minimum, maximum } },
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

Node allpass(std::string id, const double milliseconds, const double motionDepth)
{
    const auto clampDepth = std::max(motionDepth, 0.001);
    return { std::move(id), "allpass", {
        audioIn(), controlIn("delay-mod"), controlIn("coefficient-mod"), audioOut() }, {
        { "delay", milliseconds, "milliseconds", ParameterModulation {
            "delay-mod", motionDepth, ModulationPolarity::bipolar,
            milliseconds - clampDepth, milliseconds + clampDepth } },
        { "coefficient", 0.5, "unitless", ParameterModulation {
            "coefficient-mod", 0.25, ModulationPolarity::bipolar, -0.95, 0.95 } },
    } };
}

Node lowpass(std::string id, const double cutoff)
{
    return { std::move(id), "lowpass", { audioIn(), controlIn("cutoff-mod"), audioOut() }, {
        { "cutoff", cutoff, "hertz", ParameterModulation {
            "cutoff-mod", 2'000.0, ModulationPolarity::bipolar, 20.0, 20'000.0 } },
    } };
}

Node pitchShift(std::string id, const double phase)
{
    return { std::move(id), "pitch-shift", {
        audioIn(), controlIn("semitones-mod"), controlIn("grain-mod"),
        controlIn("overlap-mod"), audioOut(),
    }, {
        { "semitones", 12.0, "semitones", ParameterModulation {
            "semitones-mod", 12.0, ModulationPolarity::bipolar, -24.0, 24.0 } },
        { "grain", 72.0, "milliseconds", ParameterModulation {
            "grain-mod", 20.0, ModulationPolarity::bipolar, 20.0, 120.0 } },
        { "overlap", 0.68, "normalized", ParameterModulation {
            "overlap-mod", 0.25, ModulationPolarity::bipolar, 0.1, 1.0 } },
        { "direction", 1.0, "direction" }, { "phase", phase, "cycles" },
    } };
}

Node lfo(std::string id, const double frequency, const double phase, const double waveform)
{
    return { std::move(id), "lfo", {
        controlIn("frequency-mod"), controlIn("phase-mod"), controlIn("waveform-mod"),
        controlIn("run-mode-mod"), controlOut(),
    }, {
        { "frequency", frequency, "hertz", ParameterModulation {
            "frequency-mod", 1.0, ModulationPolarity::bipolar, 0.01, 100.0 } },
        { "phase", phase, "cycles", ParameterModulation {
            "phase-mod", 0.25, ModulationPolarity::bipolar, 0.0, 0.999 } },
        { "waveform", waveform, "waveform", ParameterModulation {
            "waveform-mod", 1.0, ModulationPolarity::bipolar, 0.0, 1.0 } },
        { "run-mode", 0.0, "run-mode", ParameterModulation {
            "run-mode-mod", 1.0, ModulationPolarity::bipolar, 0.0, 1.0 } },
    } };
}

Connection cable(std::string id, std::string from, std::string fromPort, std::string to, std::string toPort)
{
    return { std::move(id), { std::move(from), std::move(fromPort) },
        { std::move(to), std::move(toPort) } };
}

} // namespace

GraphDocument makeReverseCosmicShimmerGraph(const ReverseCosmicShimmerControls& requested)
{
    const auto riseScale = std::clamp(requested.riseScale, 0.65, 1.35);
    const auto size = std::clamp(requested.sizeMilliseconds, 120.0, 260.0);
    const auto normalFeedback = std::clamp(requested.normalFeedback, 0.0,
        reverseCosmicMaximumNormalFeedback);
    const auto shimmerFeedback = std::clamp(requested.shimmerFeedback, 0.0,
        reverseCosmicMaximumShimmerFeedback);
    const auto damping = std::clamp(requested.dampingHertz, 1'800.0, 7'500.0);
    const auto motion = std::clamp(requested.modulationDepthMilliseconds, 0.0, 1.8);
    const auto wet = std::clamp(requested.wetLevel, 0.0, 0.70);
    const auto shimmerVoiceGain = shimmerFeedback * 0.5;

    GraphDocument graph;
    graph.nodes = {
        { "input", "stereo-input", { audioOut("out-l"), audioOut("out-r") }, {} },
        gain("input-left-half", 0.5), gain("input-right-half", 0.5), sum("input-mono"),
        delay("rise-delay-early", 80.0 * riseScale), gain("rise-gain-early", 0.12, 0.0, 0.30),
        delay("rise-delay-middle", 240.0 * riseScale), gain("rise-gain-middle", 0.25, 0.0, 0.40),
        delay("rise-delay-late", 520.0 * riseScale), gain("rise-gain-late", 0.50, 0.0, 0.60),
        sum("rise-sum-early"), sum("rise-sum-complete"),
        allpass("rise-diffusion-a", 7.3, 0.0), allpass("rise-diffusion-b", 13.1, 0.0),
        sum("tank-entry"), allpass("tank-diffusion-a", 19.7, motion),
        delay("tank-delay", size), allpass("tank-diffusion-b", 31.9, motion),
        lowpass("tank-damping", damping),
        gain("normal-feedback", normalFeedback, 0.0, reverseCosmicMaximumNormalFeedback),
        delay("normal-return-delay", 71.0),
        lowpass("shift-highpass-lowpass", 360.0), gain("shift-highpass-invert", -1.0),
        sum("shift-highpass"),
        pitchShift("reverse-pitch-left", 0.0), pitchShift("reverse-pitch-right", 0.373),
        lowpass("shift-damping-left", damping * 0.82), lowpass("shift-damping-right", damping * 0.76),
        gain("shimmer-feedback-left", shimmerVoiceGain, 0.0, 0.06),
        gain("shimmer-feedback-right", shimmerVoiceGain, 0.0, 0.06),
        delay("shimmer-return-left", 89.0), delay("shimmer-return-right", 103.0),
        sum("shimmer-return-sum"), sum("feedback-recombine"),
        gain("wet-level", wet, 0.0, 0.70),
        allpass("left-extraction", 17.3, motion), allpass("right-extraction", 26.9, motion),
        allpass("right-extraction-b", 43.1, motion * 0.73),
        gain("shimmer-output-left", 0.14, 0.0, 0.20), gain("shimmer-output-right", 0.14, 0.0, 0.20),
        sum("left-output-sum"), sum("right-output-sum"),
        lfo("motion-left", 0.083, 0.0, 0.0), lfo("motion-right", 0.061, 0.317, 1.0),
        { "output", "stereo-output", { audioIn("in-l"), audioIn("in-r") }, {} },
    };

    const std::array names {
        std::pair { "input-mono", "Mono input" }, std::pair { "rise-delay-early", "Rise / 80 ms" },
        std::pair { "rise-delay-middle", "Rise / 240 ms" }, std::pair { "rise-delay-late", "Rise / 520 ms" },
        std::pair { "rise-sum-complete", "Causal rise complete" }, std::pair { "tank-entry", "Tank entry" },
        std::pair { "tank-delay", "Size" }, std::pair { "tank-damping", "Damping" },
        std::pair { "normal-feedback", "Normal feedback" }, std::pair { "shift-highpass", "Pre-shift high-pass" },
        std::pair { "reverse-pitch-left", "Reverse octave / phase 0.000" },
        std::pair { "reverse-pitch-right", "Reverse octave / phase 0.373" },
        std::pair { "shift-damping-left", "Shimmer damping L" },
        std::pair { "shift-damping-right", "Shimmer damping R" },
        std::pair { "shimmer-feedback-left", "Shimmer feedback L" },
        std::pair { "shimmer-feedback-right", "Shimmer feedback R" },
        std::pair { "feedback-recombine", "Normal + shimmer returns" },
        std::pair { "wet-level", "Wet level" }, std::pair { "motion-left", "Modulation L" },
        std::pair { "motion-right", "Modulation R" }, std::pair { "left-extraction", "Left diffusion" },
        std::pair { "right-extraction", "Right diffusion" },
        std::pair { "right-extraction-b", "Right diffusion / unequal stage" },
    };
    for (const auto& [id, name] : names) {
        const auto found = std::ranges::find(graph.nodes, std::string_view(id), &Node::id);
        if (found != graph.nodes.end()) found->name = name;
    }

    graph.connections = {
        cable("input-left", "input", "out-l", "input-left-half", "in"),
        cable("input-right", "input", "out-r", "input-right-half", "in"),
        cable("input-left-sum", "input-left-half", "out", "input-mono", "in-a"),
        cable("input-right-sum", "input-right-half", "out", "input-mono", "in-b"),
        cable("rise-early-delay", "input-mono", "out", "rise-delay-early", "in"),
        cable("rise-early-gain", "rise-delay-early", "out", "rise-gain-early", "in"),
        cable("rise-middle-delay", "input-mono", "out", "rise-delay-middle", "in"),
        cable("rise-middle-gain", "rise-delay-middle", "out", "rise-gain-middle", "in"),
        cable("rise-late-delay", "input-mono", "out", "rise-delay-late", "in"),
        cable("rise-late-gain", "rise-delay-late", "out", "rise-gain-late", "in"),
        cable("rise-early-a", "rise-gain-early", "out", "rise-sum-early", "in-a"),
        cable("rise-early-b", "rise-gain-middle", "out", "rise-sum-early", "in-b"),
        cable("rise-complete-a", "rise-sum-early", "out", "rise-sum-complete", "in-a"),
        cable("rise-complete-b", "rise-gain-late", "out", "rise-sum-complete", "in-b"),
        cable("rise-diffuse-a", "rise-sum-complete", "out", "rise-diffusion-a", "in"),
        cable("rise-diffuse-b", "rise-diffusion-a", "out", "rise-diffusion-b", "in"),
        cable("rise-tank", "rise-diffusion-b", "out", "tank-entry", "in-a"),
        cable("feedback-tank", "feedback-recombine", "out", "tank-entry", "in-b"),
        cable("tank-ap-a", "tank-entry", "out", "tank-diffusion-a", "in"),
        cable("tank-delay", "tank-diffusion-a", "out", "tank-delay", "in"),
        cable("tank-ap-b", "tank-delay", "out", "tank-diffusion-b", "in"),
        cable("tank-damping", "tank-diffusion-b", "out", "tank-damping", "in"),
        cable("normal-feedback", "tank-damping", "out", "normal-feedback", "in"),
        cable("normal-delay", "normal-feedback", "out", "normal-return-delay", "in"),
        cable("normal-recombine", "normal-return-delay", "out", "feedback-recombine", "in-a"),
        cable("highpass-direct", "tank-damping", "out", "shift-highpass", "in-a"),
        cable("highpass-lowpass", "tank-damping", "out", "shift-highpass-lowpass", "in"),
        cable("highpass-invert", "shift-highpass-lowpass", "out", "shift-highpass-invert", "in"),
        cable("highpass-subtract", "shift-highpass-invert", "out", "shift-highpass", "in-b"),
        cable("pitch-left", "shift-highpass", "out", "reverse-pitch-left", "in"),
        cable("pitch-right", "shift-highpass", "out", "reverse-pitch-right", "in"),
        cable("pitch-damp-left", "reverse-pitch-left", "out", "shift-damping-left", "in"),
        cable("pitch-damp-right", "reverse-pitch-right", "out", "shift-damping-right", "in"),
        cable("shimmer-gain-left", "shift-damping-left", "out", "shimmer-feedback-left", "in"),
        cable("shimmer-gain-right", "shift-damping-right", "out", "shimmer-feedback-right", "in"),
        cable("shimmer-delay-left", "shimmer-feedback-left", "out", "shimmer-return-left", "in"),
        cable("shimmer-delay-right", "shimmer-feedback-right", "out", "shimmer-return-right", "in"),
        cable("shimmer-return-a", "shimmer-return-left", "out", "shimmer-return-sum", "in-a"),
        cable("shimmer-return-b", "shimmer-return-right", "out", "shimmer-return-sum", "in-b"),
        cable("shimmer-recombine", "shimmer-return-sum", "out", "feedback-recombine", "in-b"),
        cable("wet", "tank-damping", "out", "wet-level", "in"),
        cable("wet-left", "wet-level", "out", "left-extraction", "in"),
        cable("wet-right", "wet-level", "out", "right-extraction", "in"),
        cable("wet-right-b", "right-extraction", "out", "right-extraction-b", "in"),
        cable("shimmer-output-left", "shift-damping-left", "out", "shimmer-output-left", "in"),
        cable("shimmer-output-right", "shift-damping-right", "out", "shimmer-output-right", "in"),
        cable("left-output-normal", "left-extraction", "out", "left-output-sum", "in-a"),
        cable("left-output-shimmer", "shimmer-output-left", "out", "left-output-sum", "in-b"),
        cable("right-output-normal", "right-extraction-b", "out", "right-output-sum", "in-a"),
        cable("right-output-shimmer", "shimmer-output-right", "out", "right-output-sum", "in-b"),
        cable("left-output", "left-output-sum", "out", "output", "in-l"),
        cable("right-output", "right-output-sum", "out", "output", "in-r"),
        cable("motion-tank-a", "motion-left", "out", "tank-diffusion-a", "delay-mod"),
        cable("motion-tank-b", "motion-right", "out", "tank-diffusion-b", "delay-mod"),
        cable("motion-output-left", "motion-left", "out", "left-extraction", "delay-mod"),
        cable("motion-output-right", "motion-right", "out", "right-extraction", "delay-mod"),
        cable("motion-output-right-b", "motion-right", "out", "right-extraction-b", "delay-mod"),
    };

    const std::array<NodePosition, 45> positions {{
        NodePosition { "input", -1'420.0, -120.0 }, { "input-left-half", -1'220.0, -200.0 },
        { "input-right-half", -1'220.0, -40.0 }, { "input-mono", -1'000.0, -120.0 },
        { "rise-delay-early", -760.0, -300.0 }, { "rise-gain-early", -520.0, -300.0 },
        { "rise-delay-middle", -760.0, -100.0 }, { "rise-gain-middle", -520.0, -100.0 },
        { "rise-delay-late", -760.0, 100.0 }, { "rise-gain-late", -520.0, 100.0 },
        { "rise-sum-early", -280.0, -200.0 }, { "rise-sum-complete", -40.0, -100.0 },
        { "rise-diffusion-a", 200.0, -100.0 }, { "rise-diffusion-b", 440.0, -100.0 },
        { "tank-entry", 680.0, -100.0 }, { "tank-diffusion-a", 920.0, -100.0 },
        { "tank-delay", 1'160.0, -100.0 }, { "tank-diffusion-b", 1'400.0, -100.0 },
        { "tank-damping", 1'640.0, -100.0 }, { "wet-level", 1'880.0, -100.0 },
        { "left-extraction", 2'120.0, -220.0 }, { "right-extraction", 2'000.0, 20.0 },
        { "right-extraction-b", 2'240.0, 20.0 },
        { "left-output-sum", 2'600.0, -220.0 }, { "right-output-sum", 2'600.0, 20.0 },
        { "output", 2'840.0, -100.0 }, { "normal-feedback", 1'580.0, 260.0 },
        { "normal-return-delay", 1'340.0, 260.0 }, { "shift-highpass-lowpass", 1'620.0, 520.0 },
        { "shift-highpass-invert", 1'380.0, 520.0 }, { "shift-highpass", 1'140.0, 440.0 },
        { "reverse-pitch-left", 900.0, 360.0 }, { "reverse-pitch-right", 900.0, 620.0 },
        { "shift-damping-left", 660.0, 360.0 }, { "shift-damping-right", 660.0, 620.0 },
        { "shimmer-feedback-left", 420.0, 360.0 }, { "shimmer-feedback-right", 420.0, 620.0 },
        { "shimmer-return-left", 180.0, 360.0 }, { "shimmer-return-right", 180.0, 620.0 },
        { "shimmer-return-sum", -60.0, 500.0 }, { "feedback-recombine", -300.0, 260.0 },
        { "shimmer-output-left", 2'360.0, 260.0 }, { "shimmer-output-right", 2'360.0, 500.0 },
        { "motion-left", 920.0, 900.0 }, { "motion-right", 1'180.0, 900.0 },
    }};
    graph.layout.nodes.assign(positions.begin(), positions.end());
    graph.layout.viewport = { 320.0, 180.0, 0.34 };
    return graph;
}

} // namespace reverb::graph
