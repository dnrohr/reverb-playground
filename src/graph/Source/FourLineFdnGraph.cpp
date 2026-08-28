#include <reverb/graph/FourLineFdnGraph.h>

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
        { "gain", value, "linear", ParameterModulation { "gain-mod",
            std::max(std::abs(minimum), std::abs(maximum)), ModulationPolarity::bipolar,
            minimum, maximum } },
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

Node allpass(std::string id, const double milliseconds, const double depth)
{
    const auto clampDepth = std::max(depth, 0.001);
    return { std::move(id), "allpass", { audioIn(), controlIn("delay-mod"),
        controlIn("coefficient-mod"), audioOut() }, {
        { "delay", milliseconds, "milliseconds", ParameterModulation { "delay-mod", depth,
            ModulationPolarity::bipolar, milliseconds - clampDepth, milliseconds + clampDepth } },
        { "coefficient", 0.53, "unitless", ParameterModulation { "coefficient-mod", 0.15,
            ModulationPolarity::bipolar, 0.35, 0.72 } },
    } };
}

Node lowpass(std::string id, const double cutoff)
{
    return { std::move(id), "lowpass", { audioIn(), controlIn("cutoff-mod"), audioOut() }, {
        { "cutoff", cutoff, "hertz", ParameterModulation { "cutoff-mod", 3'000.0,
            ModulationPolarity::bipolar, 1'200.0, 14'000.0 } },
    } };
}

Node lfo(std::string id, const double frequency, const double phase)
{
    return { std::move(id), "lfo", { controlIn("frequency-mod"), controlIn("phase-mod"),
        controlIn("waveform-mod"), controlIn("run-mode-mod"), controlOut() }, {
        { "frequency", frequency, "hertz", ParameterModulation { "frequency-mod", 0.12,
            ModulationPolarity::bipolar, 0.01, 0.3 } },
        { "phase", phase, "cycles", ParameterModulation { "phase-mod", 0.25,
            ModulationPolarity::bipolar, 0.0, 0.999 } },
        { "waveform", 0.0, "waveform", ParameterModulation { "waveform-mod", 1.0,
            ModulationPolarity::bipolar, 0.0, 1.0 } },
        { "run-mode", 0.0, "run-mode", ParameterModulation { "run-mode-mod", 1.0,
            ModulationPolarity::bipolar, 0.0, 1.0 } },
    } };
}

Connection cable(std::string id, std::string from, std::string fromPort, std::string to, std::string toPort)
{
    return { std::move(id), { std::move(from), std::move(fromPort) },
        { std::move(to), std::move(toPort) } };
}

} // namespace

std::array<double, 4> hadamard4(const std::array<double, 4>& x) noexcept
{
    return { 0.5 * (x[0] + x[1] + x[2] + x[3]),
        0.5 * (x[0] - x[1] + x[2] - x[3]),
        0.5 * (x[0] + x[1] - x[2] - x[3]),
        0.5 * (x[0] - x[1] - x[2] + x[3]) };
}

double fdnLineGainForRt60(const double traversalMilliseconds, const double rt60Seconds) noexcept
{
    if (!std::isfinite(traversalMilliseconds) || !std::isfinite(rt60Seconds)
        || traversalMilliseconds <= 0.0 || rt60Seconds <= 0.0) return 0.0;
    return std::clamp(std::pow(10.0, -3.0 * traversalMilliseconds / (1'000.0 * rt60Seconds)), 0.0, 0.98);
}

GraphDocument makeFourLineFdnGraph(const FourLineFdnControls& requested)
{
    const auto rt60 = std::clamp(requested.rt60Seconds, 0.35, 8.0);
    const auto damping = std::clamp(requested.dampingHertz, 1'200.0, 14'000.0);
    const auto motion = std::clamp(requested.modulationDepthMilliseconds, 0.0, 1.25);
    const auto wet = std::clamp(requested.wetLevel, 0.0, 0.7);
    constexpr std::array delayTimes { 53.9, 67.7, 79.9, 97.1 };
    constexpr std::array diffusionTimes { 5.3, 7.1, 8.9, 11.3 };
    constexpr std::array injectionSigns { 0.32, -0.32, 0.32, -0.32 };
    constexpr std::array outputLeft { 0.50, 0.32, -0.25, 0.18 };
    constexpr std::array outputRight { -0.21, 0.28, 0.34, 0.47 };
    constexpr double h[4][4] {
        { 0.5, 0.5, 0.5, 0.5 }, { 0.5, -0.5, 0.5, -0.5 },
        { 0.5, 0.5, -0.5, -0.5 }, { 0.5, -0.5, -0.5, 0.5 },
    };

    GraphDocument graph;
    graph.nodes = {
        { "input", "stereo-input", { audioOut("out-l"), audioOut("out-r") }, {} },
        gain("input-left-half", 0.5, 0.0, 1.0), gain("input-right-half", 0.5, 0.0, 1.0),
        sum("input-mono"), allpass("input-diffusion-a", 3.7, 0.0),
        allpass("input-diffusion-b", 7.9, 0.0), lfo("motion-a", 0.083, 0.0),
        lfo("motion-b", 0.057, 0.37),
    };
    graph.connections = {
        cable("input-left", "input", "out-l", "input-left-half", "in"),
        cable("input-right", "input", "out-r", "input-right-half", "in"),
        cable("input-left-sum", "input-left-half", "out", "input-mono", "in-a"),
        cable("input-right-sum", "input-right-half", "out", "input-mono", "in-b"),
        cable("input-diffuse-a", "input-mono", "out", "input-diffusion-a", "in"),
        cable("input-diffuse-b", "input-diffusion-a", "out", "input-diffusion-b", "in"),
    };

    for (std::size_t line = 0; line < 4; ++line) {
        const auto suffix = std::to_string(line + 1);
        graph.nodes.push_back(gain("inject-" + suffix, injectionSigns[line]));
        graph.nodes.push_back(sum("line-entry-" + suffix));
        graph.nodes.push_back(allpass("line-diffusion-" + suffix, diffusionTimes[line], motion));
        graph.nodes.push_back(delay("line-delay-" + suffix, delayTimes[line]));
        graph.nodes.push_back(lowpass("line-damping-" + suffix, damping * (1.0 - 0.035 * line)));
        graph.nodes.push_back(gain("line-return-" + suffix,
            fdnLineGainForRt60(delayTimes[line] + diffusionTimes[line], rt60), 0.0, 0.98));
        graph.connections.push_back(cable("inject-source-" + suffix, "input-diffusion-b", "out", "inject-" + suffix, "in"));
        graph.connections.push_back(cable("inject-entry-" + suffix, "inject-" + suffix, "out", "line-entry-" + suffix, "in-a"));
        graph.connections.push_back(cable("line-diffuse-" + suffix, "line-entry-" + suffix, "out", "line-diffusion-" + suffix, "in"));
        graph.connections.push_back(cable("line-delay-" + suffix, "line-diffusion-" + suffix, "out", "line-delay-" + suffix, "in"));
        graph.connections.push_back(cable("line-damp-" + suffix, "line-delay-" + suffix, "out", "line-damping-" + suffix, "in"));
        graph.connections.push_back(cable("line-return-" + suffix, "line-damping-" + suffix, "out", "line-return-" + suffix, "in"));
        graph.connections.push_back(cable("motion-" + suffix, line % 2 == 0 ? "motion-a" : "motion-b", "out",
            "line-diffusion-" + suffix, "delay-mod"));
    }

    for (std::size_t output = 0; output < 4; ++output) {
        const auto out = std::to_string(output + 1);
        for (std::size_t input = 0; input < 4; ++input) {
            const auto in = std::to_string(input + 1);
            graph.nodes.push_back(gain("matrix-" + out + "-from-" + in, h[output][input]));
            graph.connections.push_back(cable("matrix-feed-" + out + "-" + in,
                "line-return-" + in, "out", "matrix-" + out + "-from-" + in, "in"));
        }
        graph.nodes.push_back(sum("matrix-" + out + "-sum-a"));
        graph.nodes.push_back(sum("matrix-" + out + "-sum-b"));
        graph.nodes.push_back(sum("matrix-" + out + "-out"));
        graph.connections.push_back(cable("matrix-" + out + "-a1", "matrix-" + out + "-from-1", "out", "matrix-" + out + "-sum-a", "in-a"));
        graph.connections.push_back(cable("matrix-" + out + "-a2", "matrix-" + out + "-from-2", "out", "matrix-" + out + "-sum-a", "in-b"));
        graph.connections.push_back(cable("matrix-" + out + "-b1", "matrix-" + out + "-from-3", "out", "matrix-" + out + "-sum-b", "in-a"));
        graph.connections.push_back(cable("matrix-" + out + "-b2", "matrix-" + out + "-from-4", "out", "matrix-" + out + "-sum-b", "in-b"));
        graph.connections.push_back(cable("matrix-" + out + "-combine-a", "matrix-" + out + "-sum-a", "out", "matrix-" + out + "-out", "in-a"));
        graph.connections.push_back(cable("matrix-" + out + "-combine-b", "matrix-" + out + "-sum-b", "out", "matrix-" + out + "-out", "in-b"));
        graph.connections.push_back(cable("matrix-return-" + out, "matrix-" + out + "-out", "out", "line-entry-" + out, "in-b"));
    }

    for (std::size_t line = 0; line < 4; ++line) {
        const auto suffix = std::to_string(line + 1);
        graph.nodes.push_back(gain("left-pickup-" + suffix, wet * outputLeft[line]));
        graph.nodes.push_back(gain("right-pickup-" + suffix, wet * outputRight[line]));
        graph.connections.push_back(cable("left-pickup-" + suffix, "line-damping-" + suffix, "out", "left-pickup-" + suffix, "in"));
        graph.connections.push_back(cable("right-pickup-" + suffix, "line-damping-" + suffix, "out", "right-pickup-" + suffix, "in"));
    }
    for (const auto channel : { std::string("left"), std::string("right") }) {
        graph.nodes.push_back(sum(channel + "-sum-a"));
        graph.nodes.push_back(sum(channel + "-sum-b"));
        graph.nodes.push_back(sum(channel + "-sum"));
        graph.connections.push_back(cable(channel + "-a1", channel + "-pickup-1", "out", channel + "-sum-a", "in-a"));
        graph.connections.push_back(cable(channel + "-a2", channel + "-pickup-2", "out", channel + "-sum-a", "in-b"));
        graph.connections.push_back(cable(channel + "-b1", channel + "-pickup-3", "out", channel + "-sum-b", "in-a"));
        graph.connections.push_back(cable(channel + "-b2", channel + "-pickup-4", "out", channel + "-sum-b", "in-b"));
        graph.connections.push_back(cable(channel + "-combine-a", channel + "-sum-a", "out", channel + "-sum", "in-a"));
        graph.connections.push_back(cable(channel + "-combine-b", channel + "-sum-b", "out", channel + "-sum", "in-b"));
    }
    graph.nodes.push_back({ "output", "stereo-output", { audioIn("in-l"), audioIn("in-r") }, {} });
    graph.connections.push_back(cable("left-output", "left-sum", "out", "output", "in-l"));
    graph.connections.push_back(cable("right-output", "right-sum", "out", "output", "in-r"));

    graph.layout.nodes = { { "input", -1'300.0, 0.0 }, { "input-left-half", -1'100.0, -100.0 },
        { "input-right-half", -1'100.0, 100.0 }, { "input-mono", -880.0, 0.0 },
        { "input-diffusion-a", -660.0, 0.0 }, { "input-diffusion-b", -440.0, 0.0 },
        { "motion-a", -400.0, 850.0 }, { "motion-b", -180.0, 850.0 } };
    for (std::size_t line = 0; line < 4; ++line) {
        const auto suffix = std::to_string(line + 1);
        const auto y = -660.0 + 440.0 * line;
        graph.layout.nodes.push_back({ "inject-" + suffix, -180.0, y });
        graph.layout.nodes.push_back({ "line-entry-" + suffix, 40.0, y });
        graph.layout.nodes.push_back({ "line-diffusion-" + suffix, 260.0, y });
        graph.layout.nodes.push_back({ "line-delay-" + suffix, 480.0, y });
        graph.layout.nodes.push_back({ "line-damping-" + suffix, 700.0, y });
        graph.layout.nodes.push_back({ "line-return-" + suffix, 920.0, y });
        graph.layout.nodes.push_back({ "left-pickup-" + suffix, 2'620.0, y - 55.0 });
        graph.layout.nodes.push_back({ "right-pickup-" + suffix, 2'620.0, y + 55.0 });
    }
    for (std::size_t output = 0; output < 4; ++output) {
        const auto out = std::to_string(output + 1);
        const auto y = -660.0 + 440.0 * output;
        for (std::size_t input = 0; input < 4; ++input)
            graph.layout.nodes.push_back({ "matrix-" + out + "-from-" + std::to_string(input + 1), 1'160.0 + 220.0 * input, y });
        graph.layout.nodes.push_back({ "matrix-" + out + "-sum-a", 2'080.0, y - 70.0 });
        graph.layout.nodes.push_back({ "matrix-" + out + "-sum-b", 2'080.0, y + 70.0 });
        graph.layout.nodes.push_back({ "matrix-" + out + "-out", 2'300.0, y });
    }
    graph.layout.nodes.insert(graph.layout.nodes.end(), { { "left-sum-a", 2'880.0, -360.0 },
        { "left-sum-b", 2'880.0, -190.0 }, { "left-sum", 3'100.0, -275.0 },
        { "right-sum-a", 2'880.0, 190.0 }, { "right-sum-b", 2'880.0, 360.0 },
        { "right-sum", 3'100.0, 275.0 }, { "output", 3'340.0, 0.0 } });
    graph.layout.viewport = { 240.0, 120.0, 0.28 };
    return graph;
}

} // namespace reverb::graph
