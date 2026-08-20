#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/graph/ReverseCosmicShimmerGraph.h>
#include <reverb/graph/SafeParallelShimmerGraph.h>
#include <reverb/graph/SplitFeedbackShimmerGraph.h>
#include <reverb/render/EnvelopeMeasurements.h>
#include <reverb/render/OfflineRenderer.h>

#include <nlohmann/json.hpp>

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

nlohmann::json loadFactoryCatalog()
{
    return nlohmann::json::parse(readFile(
        std::filesystem::path { REVERB_FACTORY_PATCH_DIR } / "catalog.json"));
}

reverb::graph::GraphDocument loadCatalogFactory(const nlohmann::json& entry)
{
    if (entry.at("document").at("kind") == "native-runtime")
        return reverb::graph::makeBarrReferenceGraph();
    return loadFactory(std::filesystem::path {
        entry.at("document").at("path").get<std::string>() }.filename().string());
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

TEST_CASE("Factory catalog declares the complete licensed and traceable shipped set")
{
    const auto catalog = loadFactoryCatalog();
    REQUIRE(catalog.at("catalogVersion") == 1);
    REQUIRE(catalog.at("patches").size() == 8);
    const std::set<std::string> expectedIds {
        "barr-reference", "causal-reverse-envelope", "level-gated-room", "modulated-cosmic-reverse", "gravity-diffusion", "safe-parallel-shimmer", "split-feedback-shimmer", "reverse-cosmic-shimmer",
    };
    const std::set<std::string> expectedFamilies {
        "barr-reference", "reverse-style", "gated", "modulated-reverse-style", "gravity-diffusion", "parallel-shimmer", "feedback-shimmer", "reverse-cosmic-shimmer",
    };
    std::set<std::string> ids;
    std::set<std::string> families;
    const auto root = std::filesystem::path { REVERB_FACTORY_PATCH_DIR }.parent_path();
    for (const auto& entry : catalog.at("patches")) {
        ids.insert(entry.at("id").get<std::string>());
        families.insert(entry.at("family").get<std::string>());
        REQUIRE(entry.at("status") == "complete");
        REQUIRE(entry.at("document").at("schemaVersion") == 2);
        REQUIRE(entry.at("document").at("engineVersion") == "0.1");
        REQUIRE(entry.at("license").at("expression") == "AGPL-3.0-only");
        REQUIRE(std::filesystem::is_regular_file(root / entry.at("license").at("file").get<std::string>()));
        REQUIRE(std::filesystem::is_regular_file(root / entry.at("document").at("path").get<std::string>()));
        REQUIRE(std::filesystem::is_regular_file(root / entry.at("provenance").at("source").get<std::string>()));
        REQUIRE_FALSE(entry.at("provenance").at("kind").get<std::string>().empty());
        REQUIRE_FALSE(entry.at("provenance").at("description").get<std::string>().empty());
    }
    REQUIRE(ids == expectedIds);
    REQUIRE(families == expectedFamilies);
}

TEST_CASE("Every catalog factory loads validates renders finite and round trips")
{
    const std::set<std::string> publicTypes {
        "stereo-input", "stereo-output", "gain", "sum", "delay", "allpass",
        "lowpass", "pitch-shift", "macro", "lfo", "control-map", "envelope-follower", "hold-gate",
    };
    const auto catalog = loadFactoryCatalog();
    for (const auto& entry : catalog.at("patches")) {
        const auto graph = loadCatalogFactory(entry);
        REQUIRE(reverb::graph::validate(graph).valid());
        REQUIRE(std::ranges::all_of(graph.nodes, [&publicTypes](const auto& node) {
            return publicTypes.contains(node.type);
        }));
        const auto compiled = reverb::graph::compileFeedbackGraph(graph, 48'000.0, 256);
        REQUIRE(compiled.valid());
        const auto written = reverb::graph::writePatchJson(graph);
        const auto serialized = nlohmann::json::parse(written);
        REQUIRE(serialized.at("schemaVersion") == entry.at("document").at("schemaVersion"));
        REQUIRE(serialized.at("engineVersion") == entry.at("document").at("engineVersion"));
        REQUIRE(reverb::graph::parsePatchJson(written) == graph);
        const auto rendered = reverb::render::renderOffline({
            graph, reverb::render::InputKind::impulse, 48'000.0, 24'000,
        });
        requireFiniteBounded(rendered, 1.0);
    }
}

TEST_CASE("Safe Parallel Shimmer factory exactly matches its public native builder")
{
    const auto checked = loadFactory("safe-parallel-shimmer.rvp.json");
    const auto authored = reverb::graph::makeSafeParallelShimmerGraph();
    REQUIRE(checked == authored);
    REQUIRE(reverb::graph::writePatchJson(checked) == readFile(
        std::filesystem::path { REVERB_FACTORY_PATCH_DIR } / "safe-parallel-shimmer.rvp.json"));
    REQUIRE(checked.nodes.size() == 28);
    REQUIRE(checked.connections.size() == 32);
}

TEST_CASE("Split Feedback Shimmer factory exactly matches its public native builder")
{
    const auto checked = loadFactory("split-feedback-shimmer.rvp.json");
    const auto authored = reverb::graph::makeSplitFeedbackShimmerGraph();
    REQUIRE(checked == authored);
    REQUIRE(reverb::graph::writePatchJson(checked) == readFile(
        std::filesystem::path { REVERB_FACTORY_PATCH_DIR } / "split-feedback-shimmer.rvp.json"));
    REQUIRE(checked.nodes.size() == 25);
    REQUIRE(checked.connections.size() == 29);
}

TEST_CASE("Reverse Cosmic Shimmer factory exactly matches its public native builder")
{
    const auto checked = loadFactory("reverse-cosmic-shimmer.rvp.json");
    const auto authored = reverb::graph::makeReverseCosmicShimmerGraph();
    REQUIRE(checked == authored);
    REQUIRE(reverb::graph::writePatchJson(checked) == readFile(
        std::filesystem::path { REVERB_FACTORY_PATCH_DIR } / "reverse-cosmic-shimmer.rvp.json"));
    REQUIRE(checked.nodes.size() == 45);
    REQUIRE(checked.connections.size() == 57);
}

TEST_CASE("Gravity Diffusion factory is the complete editable measured graph")
{
    const auto graph = loadFactory("gravity-diffusion.rvp.json");
    REQUIRE(graph.nodes.size() == 58);
    REQUIRE(graph.connections.size() == 94);
    REQUIRE(graph.layout.nodes.size() == graph.nodes.size());
    REQUIRE(std::ranges::count_if(graph.nodes, [](const auto& node) { return node.type == "macro"; }) == 5);
    REQUIRE(std::ranges::count_if(graph.nodes, [](const auto& node) { return node.type == "control-map"; }) == 8);
    REQUIRE(std::ranges::count_if(graph.nodes, [](const auto& node) { return node.type == "lfo"; }) == 2);
    REQUIRE(parameter(graph, "gravity", "value") == Catch::Approx(0.0));
    REQUIRE(parameter(graph, "size", "value") == Catch::Approx(-0.35));
    REQUIRE(parameter(graph, "feedback", "value") == Catch::Approx(1.0));
    REQUIRE(parameter(graph, "damping", "value") == Catch::Approx(0.0));
    REQUIRE(parameter(graph, "modulation", "value") == Catch::Approx(1.0));
    const auto gravity = std::ranges::find(graph.nodes, "gravity", &reverb::graph::Node::id);
    REQUIRE(gravity != graph.nodes.end());
    REQUIRE(gravity->presentation == "gravity");
    REQUIRE(gravity->name == "Gravity");
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

TEST_CASE("Modulated Cosmic Reverse exposes delayed damped feedback and slow independent drift")
{
    const auto graph = loadFactory("modulated-cosmic-reverse.rvp.json");
    REQUIRE(parameter(graph, "rise-early-80ms", "delay") < parameter(graph, "rise-middle-240ms", "delay"));
    REQUIRE(parameter(graph, "rise-middle-240ms", "delay") < parameter(graph, "rise-peak-520ms", "delay"));
    REQUIRE(std::abs(parameter(graph, "weight-early-0-18", "gain")) < std::abs(parameter(graph, "weight-middle-0-42", "gain")));
    REQUIRE(std::abs(parameter(graph, "weight-middle-0-42", "gain")) < std::abs(parameter(graph, "weight-peak-0-72", "gain")));
    REQUIRE(parameter(graph, "tank-space-173ms", "delay") >= 1.0);
    REQUIRE(parameter(graph, "feedback-0-58", "gain") == Catch::Approx(0.58));
    REQUIRE(parameter(graph, "tail-damping-4-8khz", "cutoff") == Catch::Approx(4'800.0));
    REQUIRE(parameter(graph, "slow-drift-a-0-11hz", "frequency") == Catch::Approx(0.11));
    REQUIRE(parameter(graph, "slow-drift-b-0-073hz", "frequency") == Catch::Approx(0.073));
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0 }) {
        const auto rendered = renderImpulseAtLevel(
            graph, sampleRate, static_cast<std::size_t>(sampleRate * 3.0), 0.1F);
        requireFiniteBounded(rendered, 0.1);
        const auto envelope = reverb::render::measureEnvelope(rendered.left, rendered.right, sampleRate);
        REQUIRE(envelope.timeToPeakMilliseconds.has_value());
        REQUIRE(*envelope.timeToPeakMilliseconds >= 450.0);
        REQUIRE(rendered.left != rendered.right);
    }
}

TEST_CASE("Factory patches remain finite under bounded stereo noise at every supported rate")
{
    const auto catalog = loadFactoryCatalog();
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0 }) {
        for (const auto& entry : catalog.at("patches")) {
            const auto rendered = reverb::render::renderOffline({
                loadCatalogFactory(entry), reverb::render::InputKind::boundedNoise,
                sampleRate, static_cast<std::size_t>(sampleRate / 2.0),
            });
            requireFiniteBounded(rendered, 1.0);
        }
    }
}

TEST_CASE("Gravity Diffusion factory impulse is finite at every supported rate")
{
    const auto graph = loadFactory("gravity-diffusion.rvp.json");
    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0 }) {
        const auto rendered = reverb::render::renderOffline({
            graph, reverb::render::InputKind::impulse, sampleRate,
            static_cast<std::size_t>(sampleRate * 3.0),
        });
        CAPTURE(sampleRate);
        requireFiniteBounded(rendered, 1.0);
        REQUIRE(std::ranges::any_of(rendered.left, [](const auto sample) {
            return std::abs(sample) > 1.0e-6F;
        }));
        REQUIRE(std::ranges::any_of(rendered.right, [](const auto sample) {
            return std::abs(sample) > 1.0e-6F;
        }));
    }
}

TEST_CASE("Gravity Diffusion factory macros sweep continuously without recompilation")
{
    constexpr std::size_t blockSize = 64;
    constexpr std::array macroIds { "gravity", "size", "feedback", "damping", "modulation" };
    auto compiled = reverb::graph::compileFeedbackGraph(
        loadFactory("gravity-diffusion.rvp.json"), 48'000.0, blockSize);
    REQUIRE(compiled.valid());

    std::array<float, blockSize> inputLeft {}, inputRight {}, outputLeft {}, outputRight {};
    std::uint32_t random = 0x96a441U;
    float previousLeft = 0.0F;
    float previousRight = 0.0F;
    double largestStep = 0.0;
    for (int block = 0; block < 4'000; ++block) {
        const auto phase = static_cast<double>(block) / 3'999.0;
        const std::array values {
            phase * 2.0 - 1.0,
            std::sin(phase * 6.283185307179586),
            std::cos(phase * 6.283185307179586),
            1.0 - phase * 2.0,
            std::sin(phase * 12.566370614359172),
        };
        for (std::size_t index = 0; index < macroIds.size(); ++index)
            REQUIRE(compiled.runtime->setMacroValue(macroIds[index], values[index]));
        for (std::size_t sample = 0; sample < blockSize; ++sample) {
            random = random * 1'664'525U + 1'013'904'223U;
            inputLeft[sample] = static_cast<float>(((random >> 8) / 16'777'215.0) * 0.2 - 0.1);
            random = random * 1'664'525U + 1'013'904'223U;
            inputRight[sample] = static_cast<float>(((random >> 8) / 16'777'215.0) * 0.2 - 0.1);
        }
        compiled.runtime->process(inputLeft, inputRight, outputLeft, outputRight);
        for (std::size_t sample = 0; sample < blockSize; ++sample) {
            REQUIRE(std::isfinite(outputLeft[sample]));
            REQUIRE(std::isfinite(outputRight[sample]));
            largestStep = std::max(largestStep,
                std::abs(static_cast<double>(outputLeft[sample] - previousLeft)));
            largestStep = std::max(largestStep,
                std::abs(static_cast<double>(outputRight[sample] - previousRight)));
            previousLeft = outputLeft[sample];
            previousRight = outputRight[sample];
        }
    }
    CAPTURE(largestStep);
    REQUIRE(largestStep < 1.0);
}
