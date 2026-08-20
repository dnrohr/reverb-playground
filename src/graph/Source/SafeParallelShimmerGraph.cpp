#include <reverb/graph/SafeParallelShimmerGraph.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

Node pitchShift()
{
    return { "shimmer-pitch", "pitch-shift", {
        audioIn(), controlIn("semitones-mod"), controlIn("grain-mod"),
        controlIn("overlap-mod"), audioOut(),
    }, {
        { "semitones", safeParallelShimmerSemitones, "semitones" },
        { "grain", 60.0, "milliseconds" },
        { "overlap", 0.5, "normalized" },
        { "direction", 0.0, "direction" },
    } };
}

Connection cable(std::string id, std::string from, std::string fromPort, std::string to, std::string toPort)
{
    return { std::move(id), { std::move(from), std::move(fromPort) },
        { std::move(to), std::move(toPort) } };
}

} // namespace

GraphDocument makeSafeParallelShimmerGraph(const SafeParallelShimmerControls& requested)
{
    const SafeParallelShimmerControls controls {
        .reverbDecay = std::clamp(requested.reverbDecay, 0.0, 0.72),
        .shimmerLevel = std::clamp(requested.shimmerLevel, 0.0, 0.30),
        .shimmerDampingHertz = std::clamp(requested.shimmerDampingHertz, 800.0, 12'000.0),
        .wetBalance = std::clamp(requested.wetBalance, 0.0, 0.80),
    };

    GraphDocument graph;
    graph.nodes = {
        { "input", "stereo-input", { audioOut("out-l"), audioOut("out-r") }, {} },
        gain("input-left-half", 0.5), gain("input-right-half", 0.5), sum("input-mono-sum"),
        allpass("input-diffusion-a", 4.7), allpass("input-diffusion-b", 8.9), sum("tank-entry"),
        allpass("tank-diffusion-a", 13.7), delay("tank-delay", 149.0),
        allpass("tank-diffusion-b", 23.9), lowpass("tank-damping", 6'500.0),
        gain("reverb-decay", controls.reverbDecay), delay("feedback-delay", 61.0),
        delay("normal-latency-alignment", safeParallelShimmerAlignmentMilliseconds),
        gain("normal-level", 0.50),
        lowpass("shimmer-highpass-lowpass", safeParallelShimmerHighpassHertz),
        gain("shimmer-highpass-invert", -1.0), sum("shimmer-highpass-sum"),
        pitchShift(), lowpass("shimmer-damping", controls.shimmerDampingHertz),
        allpass("shimmer-diffusion-a", 17.3), allpass("shimmer-diffusion-b", 29.1),
        gain("shimmer-level", controls.shimmerLevel), sum("parallel-wet-sum"),
        gain("wet-balance", controls.wetBalance),
        allpass("left-extraction", 11.9), allpass("right-extraction", 19.7),
        { "output", "stereo-output", { audioIn("in-l"), audioIn("in-r") }, {} },
    };

    graph.connections = {
        cable("input-left-gain", "input", "out-l", "input-left-half", "in"),
        cable("input-right-gain", "input", "out-r", "input-right-half", "in"),
        cable("input-left-sum", "input-left-half", "out", "input-mono-sum", "in-a"),
        cable("input-right-sum", "input-right-half", "out", "input-mono-sum", "in-b"),
        cable("input-diffusion-a", "input-mono-sum", "out", "input-diffusion-a", "in"),
        cable("input-diffusion-b", "input-diffusion-a", "out", "input-diffusion-b", "in"),
        cable("input-tank", "input-diffusion-b", "out", "tank-entry", "in-a"),
        cable("feedback-tank", "feedback-delay", "out", "tank-entry", "in-b"),
        cable("tank-diffusion-a", "tank-entry", "out", "tank-diffusion-a", "in"),
        cable("tank-delay", "tank-diffusion-a", "out", "tank-delay", "in"),
        cable("tank-diffusion-b", "tank-delay", "out", "tank-diffusion-b", "in"),
        cable("tank-damping", "tank-diffusion-b", "out", "tank-damping", "in"),
        cable("tank-decay", "tank-damping", "out", "reverb-decay", "in"),
        cable("tank-feedback-delay", "reverb-decay", "out", "feedback-delay", "in"),
        cable("normal-alignment", "tank-damping", "out", "normal-latency-alignment", "in"),
        cable("normal-level", "normal-latency-alignment", "out", "normal-level", "in"),
        cable("highpass-direct", "tank-damping", "out", "shimmer-highpass-sum", "in-a"),
        cable("highpass-lowpass", "tank-damping", "out", "shimmer-highpass-lowpass", "in"),
        cable("highpass-invert", "shimmer-highpass-lowpass", "out", "shimmer-highpass-invert", "in"),
        cable("highpass-subtract", "shimmer-highpass-invert", "out", "shimmer-highpass-sum", "in-b"),
        cable("shimmer-pitch", "shimmer-highpass-sum", "out", "shimmer-pitch", "in"),
        cable("shimmer-damping", "shimmer-pitch", "out", "shimmer-damping", "in"),
        cable("shimmer-diffusion-a", "shimmer-damping", "out", "shimmer-diffusion-a", "in"),
        cable("shimmer-diffusion-b", "shimmer-diffusion-a", "out", "shimmer-diffusion-b", "in"),
        cable("shimmer-level", "shimmer-diffusion-b", "out", "shimmer-level", "in"),
        cable("normal-wet-sum", "normal-level", "out", "parallel-wet-sum", "in-a"),
        cable("shimmer-wet-sum", "shimmer-level", "out", "parallel-wet-sum", "in-b"),
        cable("wet-balance", "parallel-wet-sum", "out", "wet-balance", "in"),
        cable("left-extraction", "wet-balance", "out", "left-extraction", "in"),
        cable("right-extraction", "wet-balance", "out", "right-extraction", "in"),
        cable("left-output", "left-extraction", "out", "output", "in-l"),
        cable("right-output", "right-extraction", "out", "output", "in-r"),
    };

    graph.layout.nodes = {
        { "input", -1'100.0, -140.0 }, { "input-left-half", -900.0, -220.0 },
        { "input-right-half", -900.0, -60.0 }, { "input-mono-sum", -680.0, -140.0 },
        { "input-diffusion-a", -460.0, -140.0 }, { "input-diffusion-b", -240.0, -140.0 },
        { "tank-entry", 0.0, -140.0 }, { "tank-diffusion-a", 220.0, -140.0 },
        { "tank-delay", 440.0, -140.0 }, { "tank-diffusion-b", 660.0, -140.0 },
        { "tank-damping", 880.0, -140.0 }, { "reverb-decay", 660.0, 100.0 },
        { "feedback-delay", 260.0, 100.0 },
        { "normal-latency-alignment", 1'100.0, -300.0 }, { "normal-level", 1'340.0, -300.0 },
        { "shimmer-highpass-lowpass", 1'080.0, 80.0 }, { "shimmer-highpass-invert", 1'300.0, 80.0 },
        { "shimmer-highpass-sum", 1'520.0, -20.0 }, { "shimmer-pitch", 1'740.0, -20.0 },
        { "shimmer-damping", 1'960.0, -20.0 }, { "shimmer-diffusion-a", 2'180.0, -20.0 },
        { "shimmer-diffusion-b", 2'400.0, -20.0 }, { "shimmer-level", 2'620.0, -20.0 },
        { "parallel-wet-sum", 2'860.0, -220.0 }, { "wet-balance", 3'080.0, -220.0 },
        { "left-extraction", 3'300.0, -320.0 }, { "right-extraction", 3'300.0, -120.0 },
        { "output", 3'540.0, -220.0 },
    };
    graph.layout.viewport = { 1'120.0, -20.0, 0.38 };
    return graph;
}

} // namespace reverb::graph
