#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/render/EnvelopeMeasurements.h>
#include <reverb/render/OfflineRenderer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace {

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    REQUIRE(stream.good());
    return { std::istreambuf_iterator<char> { stream }, std::istreambuf_iterator<char> {} };
}

reverb::graph::GraphDocument loadFactory(const std::string& filename)
{
    return reverb::graph::parsePatchJson(
        readFile(std::filesystem::path { REVERB_FACTORY_PATCH_DIR } / filename));
}

double parameter(const reverb::graph::GraphDocument& graph, const std::string& nodeId, const std::string& parameterId)
{
    const auto node = std::ranges::find(graph.nodes, nodeId, &reverb::graph::Node::id);
    REQUIRE(node != graph.nodes.end());
    const auto value = std::ranges::find(node->parameters, parameterId, &reverb::graph::Parameter::id);
    REQUIRE(value != node->parameters.end());
    return value->value;
}

void requireFiniteBounded(const reverb::render::RenderResult& rendered, const double bound)
{
    REQUIRE(std::ranges::all_of(rendered.left, [bound](const auto sample) {
        return std::isfinite(sample) && std::abs(sample) <= bound;
    }));
    REQUIRE(std::ranges::all_of(rendered.right, [bound](const auto sample) {
        return std::isfinite(sample) && std::abs(sample) <= bound;
    }));
}

reverb::render::RenderResult renderImpulseAtLevel(
    const reverb::graph::GraphDocument& graph, const double sampleRate,
    const std::size_t frameCount, const float level)
{
    constexpr std::size_t blockSize = 256;
    auto compiled = reverb::graph::compileFeedbackGraph(graph, sampleRate, blockSize);
    REQUIRE(compiled.valid());
    std::vector<float> inputLeft(frameCount, 0.0F);
    std::vector<float> inputRight(frameCount, 0.0F);
    reverb::render::RenderResult result {
        std::vector<float>(frameCount, 0.0F), std::vector<float>(frameCount, 0.0F),
    };
    inputLeft.front() = level;
    for (std::size_t start = 0; start < frameCount; start += blockSize) {
        const auto count = std::min(blockSize, frameCount - start);
        compiled.runtime->process(
            std::span<const float>(inputLeft).subspan(start, count),
            std::span<const float>(inputRight).subspan(start, count),
            std::span<float>(result.left).subspan(start, count),
            std::span<float>(result.right).subspan(start, count));
    }
    return result;
}

} // namespace

TEST_CASE("Factory patches load round trip and use only visible public primitives")
{
    const std::set<std::string> publicTypes {
        "stereo-input", "stereo-output", "gain", "sum", "delay", "allpass",
        "lowpass", "envelope-follower", "hold-gate",
    };
    for (const auto filename : {
             "causal-reverse-envelope.rvp.json", "level-gated-room.rvp.json",
         }) {
        const auto graph = loadFactory(filename);
        REQUIRE(reverb::graph::validate(graph).valid());
        REQUIRE(std::ranges::all_of(graph.nodes, [&publicTypes](const auto& node) {
            return publicTypes.contains(node.type);
        }));
        const auto compiled = reverb::graph::compileFeedbackGraph(graph, 48'000.0, 256);
        REQUIRE(compiled.valid());
        const auto written = reverb::graph::writePatchJson(graph);
        REQUIRE(reverb::graph::parsePatchJson(written) == graph);
    }
}

TEST_CASE("Reverse Envelope exposes strictly increasing visible delay weights")
{
    const auto graph = loadFactory("causal-reverse-envelope.rvp.json");
    const std::array delays {
        parameter(graph, "rise-early-45ms", "delay"),
        parameter(graph, "rise-middle-115ms", "delay"),
        parameter(graph, "rise-peak-210ms", "delay"),
    };
    const std::array weights {
        std::abs(parameter(graph, "weight-early-0-25", "gain")),
        std::abs(parameter(graph, "weight-middle-0-55", "gain")),
        std::abs(parameter(graph, "weight-peak-0-95", "gain")),
    };
    REQUIRE(std::ranges::is_sorted(delays, std::ranges::less {}));
    REQUIRE(std::ranges::is_sorted(weights, std::ranges::less {}));
    REQUIRE(delays[0] < delays[1]);
    REQUIRE(delays[1] < delays[2]);
    REQUIRE(weights[0] < weights[1]);
    REQUIRE(weights[1] < weights[2]);
}

TEST_CASE("Factory impulse envelopes are measurably reverse-rising and level-gated")
{
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0 }) {
        const auto frames = static_cast<std::size_t>(sampleRate);
        const auto reverse = reverb::render::renderOffline({
            loadFactory("causal-reverse-envelope.rvp.json"),
            reverb::render::InputKind::impulse, sampleRate, frames,
        });
        const auto gated = reverb::render::renderOffline({
            loadFactory("level-gated-room.rvp.json"),
            reverb::render::InputKind::impulse, sampleRate, frames,
        });
        requireFiniteBounded(reverse, 1.0);
        requireFiniteBounded(gated, 1.0);
        const auto reverseEnvelope = reverb::render::measureEnvelope(
            reverse.left, reverse.right, sampleRate);
        const auto gatedEnvelope = reverb::render::measureEnvelope(
            gated.left, gated.right, sampleRate);
        REQUIRE(reverseEnvelope.timeToPeakMilliseconds.has_value());
        REQUIRE(gatedEnvelope.timeToPeakMilliseconds.has_value());
        REQUIRE(gatedEnvelope.peakToCutoffMilliseconds.has_value());
        REQUIRE(*reverseEnvelope.timeToPeakMilliseconds == Catch::Approx(195.0).margin(1.0));
        REQUIRE(*gatedEnvelope.timeToPeakMilliseconds <= 10.0);
        REQUIRE(*gatedEnvelope.peakToCutoffMilliseconds >= 180.0);
        REQUIRE(*gatedEnvelope.peakToCutoffMilliseconds <= 210.0);
        REQUIRE(gatedEnvelope.maximumDropDecibelsPerWindow > 18.0);
        REQUIRE(reverseEnvelope.maximumDropDecibelsPerWindow < 8.0);
        REQUIRE_FALSE(gatedEnvelope.rt60Meaningful);
        REQUIRE(reverseEnvelope.rt60Meaningful);
        REQUIRE(reverseEnvelope.residualEnergyRatio < 1.0e-4);
        REQUIRE(gatedEnvelope.residualEnergyRatio < 1.0e-4);
        REQUIRE(reverse.left != reverse.right);
        REQUIRE(gated.left != gated.right);
    }
}

TEST_CASE("Level-Gated Room opens for the safe live audition impulse at every supported rate")
{
    const auto graph = loadFactory("level-gated-room.rvp.json");
    REQUIRE(parameter(graph, "left-level-gate", "threshold") == Catch::Approx(0.004));
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0 }) {
        const auto rendered = renderImpulseAtLevel(
            graph, sampleRate, static_cast<std::size_t>(sampleRate / 2.0), 0.1F);
        requireFiniteBounded(rendered, 0.1);
        REQUIRE(std::ranges::any_of(rendered.left, [](const auto sample) {
            return std::abs(sample) > 1.0e-5F;
        }));
        REQUIRE(std::ranges::any_of(rendered.right, [](const auto sample) {
            return std::abs(sample) > 1.0e-5F;
        }));
    }
}

TEST_CASE("Factory patches remain finite under bounded stereo noise at every supported rate")
{
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0 }) {
        for (const auto filename : {
                 "causal-reverse-envelope.rvp.json", "level-gated-room.rvp.json",
             }) {
            const auto rendered = reverb::render::renderOffline({
                loadFactory(filename), reverb::render::InputKind::boundedNoise,
                sampleRate, static_cast<std::size_t>(sampleRate / 2.0),
            });
            requireFiniteBounded(rendered, 1.0);
        }
    }
}
