#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/PitchShiftContract.h>
#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/graph/SafeParallelShimmerGraph.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numbers>
#include <random>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using reverb::graph::GraphDocument;

const reverb::graph::Node& node(const GraphDocument& graph, const std::string_view id)
{
    const auto found = std::ranges::find(graph.nodes, id, &reverb::graph::Node::id);
    REQUIRE(found != graph.nodes.end());
    return *found;
}

double parameter(const GraphDocument& graph, const std::string_view nodeId, const std::string_view parameterId)
{
    const auto& foundNode = node(graph, nodeId);
    const auto found = std::ranges::find(foundNode.parameters, parameterId, &reverb::graph::Parameter::id);
    REQUIRE(found != foundNode.parameters.end());
    return found->value;
}

bool canReach(const GraphDocument& graph, const std::string_view from, const std::string_view to)
{
    std::vector<std::string> pending { std::string(from) };
    std::set<std::string> visited;
    while (!pending.empty()) {
        auto current = std::move(pending.back());
        pending.pop_back();
        if (current == to) return true;
        if (!visited.insert(current).second) continue;
        for (const auto& connection : graph.connections) {
            if (connection.from.nodeId == current)
                pending.push_back(connection.to.nodeId);
        }
    }
    return false;
}

bool canReturnTo(const GraphDocument& graph, const std::string_view nodeId)
{
    for (const auto& connection : graph.connections) {
        if (connection.from.nodeId == nodeId && canReach(graph, connection.to.nodeId, nodeId))
            return true;
    }
    return false;
}

bool hasCable(const GraphDocument& graph, const std::string_view from, const std::string_view to)
{
    return std::ranges::any_of(graph.connections, [&](const auto& connection) {
        return connection.from.nodeId == from && connection.to.nodeId == to;
    });
}

double tonePower(const std::span<const float> samples, const double sampleRate, const double frequency)
{
    auto real = 0.0;
    auto imaginary = 0.0;
    const auto step = 2.0 * std::numbers::pi * frequency / sampleRate;
    for (std::size_t frame = 0; frame < samples.size(); ++frame) {
        const auto window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi
            * static_cast<double>(frame) / static_cast<double>(samples.size() - 1));
        const auto phase = step * static_cast<double>(frame);
        real += static_cast<double>(samples[frame]) * window * std::cos(phase);
        imaginary -= static_cast<double>(samples[frame]) * window * std::sin(phase);
    }
    return real * real + imaginary * imaginary;
}

double bandPeakPower(
    const std::span<const float> samples, const double sampleRate,
    const double centerFrequency, const double relativeHalfWidth = 0.025)
{
    auto peak = 0.0;
    constexpr auto steps = 50;
    for (auto index = 0; index <= steps; ++index) {
        const auto position = -1.0 + 2.0 * static_cast<double>(index) / steps;
        peak = std::max(peak, tonePower(samples, sampleRate,
            centerFrequency * (1.0 + relativeHalfWidth * position)));
    }
    return peak;
}

} // namespace

TEST_CASE("Safe Parallel Shimmer is a complete public mono-cable construction")
{
    const auto graph = reverb::graph::makeSafeParallelShimmerGraph();
    const std::set<std::string> publicTypes {
        "stereo-input", "stereo-output", "gain", "sum", "delay", "allpass", "lowpass", "pitch-shift",
    };

    REQUIRE(reverb::graph::validate(graph).valid());
    REQUIRE(graph.nodes.size() == 28);
    REQUIRE(graph.connections.size() == 32);
    REQUIRE(graph.layout.nodes.size() == graph.nodes.size());
    REQUIRE(std::ranges::all_of(graph.nodes, [&](const auto& item) {
        return publicTypes.contains(item.type);
    }));
    REQUIRE(std::ranges::count(graph.nodes, std::string("pitch-shift"), &reverb::graph::Node::type) == 1);
    REQUIRE(std::ranges::all_of(graph.connections, [&](const auto& connection) {
        const auto& source = node(graph, connection.from.nodeId);
        const auto& destination = node(graph, connection.to.nodeId);
        const auto fromPort = std::ranges::find(source.ports, connection.from.portId, &reverb::graph::Port::id);
        const auto toPort = std::ranges::find(destination.ports, connection.to.portId, &reverb::graph::Port::id);
        return fromPort != source.ports.end() && toPort != destination.ports.end()
            && fromPort->signal == reverb::graph::SignalType::audio
            && toPort->signal == reverb::graph::SignalType::audio;
    }));

    const auto roundTrip = reverb::graph::parsePatchJson(reverb::graph::writePatchJson(graph));
    REQUIRE(roundTrip == graph);
}

TEST_CASE("Safe Parallel Shimmer keeps the shifted branch structurally outside feedback")
{
    const auto graph = reverb::graph::makeSafeParallelShimmerGraph();
    const auto compiled = reverb::graph::compileFeedbackGraph(graph, 48'000.0, 256);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.feedbackComponents == std::vector<std::vector<std::string>> {
        { "feedback-delay", "reverb-decay", "tank-damping", "tank-delay",
            "tank-diffusion-a", "tank-diffusion-b", "tank-entry" },
    });
    REQUIRE(canReach(graph, "shimmer-pitch", "output"));
    REQUIRE_FALSE(canReturnTo(graph, "shimmer-pitch"));
    REQUIRE_FALSE(canReach(graph, "shimmer-pitch", "tank-entry"));
    REQUIRE_FALSE(canReach(graph, "shimmer-level", "reverb-decay"));

    REQUIRE(hasCable(graph, "tank-damping", "shimmer-highpass-sum"));
    REQUIRE(hasCable(graph, "tank-damping", "shimmer-highpass-lowpass"));
    REQUIRE(hasCable(graph, "shimmer-highpass-lowpass", "shimmer-highpass-invert"));
    REQUIRE(hasCable(graph, "shimmer-highpass-invert", "shimmer-highpass-sum"));
    REQUIRE(parameter(graph, "shimmer-highpass-invert", "gain") == -1.0);
}

TEST_CASE("Safe Parallel Shimmer controls retain separate bounded responsibilities")
{
    const reverb::graph::SafeParallelShimmerControls requested {
        .reverbDecay = 0.63,
        .shimmerLevel = 0.27,
        .shimmerDampingHertz = 4'800.0,
        .wetBalance = 0.70,
    };
    const auto graph = reverb::graph::makeSafeParallelShimmerGraph(requested);
    REQUIRE(parameter(graph, "reverb-decay", "gain") == Catch::Approx(0.63));
    REQUIRE(parameter(graph, "shimmer-level", "gain") == Catch::Approx(0.27));
    REQUIRE(parameter(graph, "shimmer-damping", "cutoff") == Catch::Approx(4'800.0));
    REQUIRE(parameter(graph, "wet-balance", "gain") == Catch::Approx(0.70));
    REQUIRE(parameter(graph, "normal-level", "gain") == Catch::Approx(0.50));
    REQUIRE(parameter(graph, "shimmer-pitch", "semitones") == Catch::Approx(12.0));

    const auto bounded = reverb::graph::makeSafeParallelShimmerGraph({ 5.0, 5.0, 50'000.0, 5.0 });
    REQUIRE(parameter(bounded, "reverb-decay", "gain") == Catch::Approx(0.72));
    REQUIRE(parameter(bounded, "shimmer-level", "gain") == Catch::Approx(0.30));
    REQUIRE(parameter(bounded, "shimmer-damping", "cutoff") == Catch::Approx(12'000.0));
    REQUIRE(parameter(bounded, "wet-balance", "gain") == Catch::Approx(0.80));
}

TEST_CASE("Safe Parallel Shimmer aligns latency and fits the delay-memory budget")
{
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0, 192'000.0 }) {
        const auto graph = reverb::graph::makeSafeParallelShimmerGraph();
        const auto compiled = reverb::graph::compileFeedbackGraph(graph, sampleRate, 256);
        CAPTURE(sampleRate, compiled.errors);
        REQUIRE(compiled.valid());
        const auto alignmentSamples = static_cast<std::size_t>(std::llround(
            reverb::graph::safeParallelShimmerAlignmentMilliseconds * sampleRate / 1'000.0));
        const auto pitchLatency = reverb::dsp::pitch_shift::reportedLatencySamples(sampleRate);
        const auto difference = alignmentSamples > pitchLatency
            ? alignmentSamples - pitchLatency : pitchLatency - alignmentSamples;
        REQUIRE(difference <= 2);
        REQUIRE(compiled.delayMemory.withinBudget());
        REQUIRE(compiled.delayMemory.allocatedBytes < 4U * 1024U * 1024U);
        REQUIRE(compiled.delayMemory.lineCount == 12);
    }
}

TEST_CASE("Safe Parallel Shimmer renders finite bounded decorrelated stereo at qualified rates")
{
    constexpr std::size_t blockSize = 256;
    for (const auto sampleRate : reverb::dsp::pitch_shift::qualificationSampleRates) {
        auto compiled = reverb::graph::compileFeedbackGraph(
            reverb::graph::makeSafeParallelShimmerGraph(), sampleRate, blockSize);
        REQUIRE(compiled.valid());
        std::mt19937 generator(0x5348494dU);
        std::uniform_real_distribution<float> noise(-0.025F, 0.025F);
        std::array<float, blockSize> inputLeft {}, inputRight {}, outputLeft {}, outputRight {};
        const auto blocks = static_cast<std::size_t>(std::ceil(sampleRate * 2.0 / blockSize));
        auto peak = 0.0F;
        auto energy = 0.0;
        auto stereoDifference = 0.0;
        for (std::size_t block = 0; block < blocks; ++block) {
            for (auto& sample : inputLeft) sample = noise(generator);
            for (auto& sample : inputRight) sample = noise(generator);
            if (block == 0) inputLeft[0] = 0.1F;
            compiled.runtime->process(inputLeft, inputRight, outputLeft, outputRight);
            for (std::size_t frame = 0; frame < blockSize; ++frame) {
                REQUIRE(std::isfinite(outputLeft[frame]));
                REQUIRE(std::isfinite(outputRight[frame]));
                peak = std::max({ peak, std::abs(outputLeft[frame]), std::abs(outputRight[frame]) });
                energy += static_cast<double>(outputLeft[frame]) * outputLeft[frame]
                    + static_cast<double>(outputRight[frame]) * outputRight[frame];
                const auto difference = static_cast<double>(outputLeft[frame]) - outputRight[frame];
                stereoDifference += difference * difference;
            }
        }
        CAPTURE(sampleRate, peak, energy, stereoDifference);
        REQUIRE(peak < 1.0F);
        REQUIRE(energy > 0.0);
        REQUIRE(stereoDifference > energy * 0.001);
    }
}

TEST_CASE("Safe Parallel Shimmer produces one octave halo without a later octave staircase")
{
    constexpr auto sampleRate = 48'000.0;
    constexpr std::size_t blockSize = 256;
    constexpr auto seconds = 4.0;
    constexpr auto sourceFrequency = 330.0;
    constexpr auto haloFrequency = sourceFrequency * 2.0;
    auto compiled = reverb::graph::compileFeedbackGraph(
        reverb::graph::makeSafeParallelShimmerGraph(), sampleRate, blockSize);
    REQUIRE(compiled.valid());
    const auto frames = static_cast<std::size_t>(seconds * sampleRate);
    std::vector<float> left(frames), right(frames), outputLeft(frames), outputRight(frames), mono(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const auto sample = static_cast<float>(0.08 * std::sin(
            2.0 * std::numbers::pi * sourceFrequency * static_cast<double>(frame) / sampleRate));
        left[frame] = sample;
        right[frame] = sample;
    }
    for (std::size_t start = 0; start < frames; start += blockSize) {
        const auto count = std::min(blockSize, frames - start);
        compiled.runtime->process(
            std::span<const float>(left).subspan(start, count),
            std::span<const float>(right).subspan(start, count),
            std::span<float>(outputLeft).subspan(start, count),
            std::span<float>(outputRight).subspan(start, count));
    }
    std::ranges::transform(outputLeft, outputRight, mono.begin(), [](const auto leftSample, const auto rightSample) {
        return 0.5F * (leftSample + rightSample);
    });

    const auto inspectWindow = [&](const double startSeconds) {
        const auto samples = std::span<const float>(mono).subspan(
            static_cast<std::size_t>(startSeconds * sampleRate),
            static_cast<std::size_t>(0.6 * sampleRate));
        return std::array {
            bandPeakPower(samples, sampleRate, sourceFrequency),
            bandPeakPower(samples, sampleRate, haloFrequency),
            bandPeakPower(samples, sampleRate, haloFrequency * 2.0),
            bandPeakPower(samples, sampleRate, haloFrequency * 4.0),
        };
    };
    const auto early = inspectWindow(1.2);
    const auto late = inspectWindow(3.0);
    CAPTURE(early, late);
    REQUIRE(early[0] > 0.0);
    REQUIRE(early[1] > early[0] * 0.0025);
    REQUIRE(early[1] > early[2] * 20.0);
    REQUIRE(early[1] > early[3] * 50.0);
    REQUIRE(late[1] > late[0] * 0.0025);
    REQUIRE(late[1] > late[2] * 20.0);
    REQUIRE(late[1] > late[3] * 50.0);

    std::ifstream artifactStream(std::filesystem::path { REVERB_MEASUREMENTS_DIR }
        / "safe-parallel-shimmer-v1.json", std::ios::binary);
    REQUIRE(artifactStream.good());
    const auto artifact = nlohmann::json::parse(std::string {
        std::istreambuf_iterator<char> { artifactStream }, std::istreambuf_iterator<char> {} });
    REQUIRE(artifact.at("formatVersion") == 1);
    REQUIRE(artifact.at("engineVersion") == "0.1");
    REQUIRE(artifact.at("patchId") == "safe-parallel-shimmer");
    REQUIRE(artifact.at("qualification").at("result") == "pass");
    const auto decibels = [](const double numerator, const double denominator) {
        return 10.0 * std::log10(numerator / denominator);
    };
    REQUIRE(decibels(early[1], early[0]) == Catch::Approx(
        artifact.at("analysis").at("early").at("haloVsSourceDb").get<double>()).margin(0.01));
    REQUIRE(decibels(early[2], early[1]) == Catch::Approx(
        artifact.at("analysis").at("early").at("octave1320VsHaloDb").get<double>()).margin(0.01));
    REQUIRE(decibels(late[1], late[0]) == Catch::Approx(
        artifact.at("analysis").at("late").at("haloVsSourceDb").get<double>()).margin(0.01));
    REQUIRE(decibels(late[2], late[1]) == Catch::Approx(
        artifact.at("analysis").at("late").at("octave1320VsHaloDb").get<double>()).margin(0.01));
}
