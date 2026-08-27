#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/DenseFigureEightGraph.h>
#include <reverb/graph/GravityDiffusionGraph.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/render/DensityMeasurements.h>
#include <reverb/render/OfflineRenderer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>
#include <span>
#include <vector>

namespace {

std::pair<std::vector<float>, std::vector<float>> renderPartitioned(
    const reverb::graph::GraphDocument& graph, const std::size_t blockSize,
    const std::size_t frames = 48'000)
{
    auto compiled = reverb::graph::compileFeedbackGraph(graph, 48'000.0, blockSize);
    REQUIRE(compiled.valid());
    std::vector<float> left(frames), right(frames), inputLeft(blockSize), inputRight(blockSize);
    for (std::size_t offset = 0; offset < frames; offset += blockSize) {
        const auto count = std::min(blockSize, frames - offset);
        std::ranges::fill(inputLeft, 0.0F);
        if (offset == 0) inputLeft[0] = 1.0F;
        compiled.runtime->process(std::span(inputLeft).first(count), std::span(inputRight).first(count),
            std::span(left).subspan(offset, count), std::span(right).subspan(offset, count));
    }
    return { std::move(left), std::move(right) };
}

} // namespace

TEST_CASE("Figure-eight RT60 gains map traversal time to sixty-decibel decay")
{
    using Catch::Approx;
    constexpr auto rt60 = 2.4;
    constexpr auto branchA = 209.3;
    constexpr auto branchB = 242.9;
    const auto gainA = reverb::graph::feedbackGainForRt60(branchA, rt60);
    const auto gainB = reverb::graph::feedbackGainForRt60(branchB, rt60);
    const auto traversals = rt60 * 1'000.0 / (branchA + branchB);
    REQUIRE(std::pow(gainA * gainB, traversals) == Approx(0.001).epsilon(1.0e-10));
    REQUIRE(reverb::graph::feedbackGainForRt60(0.0, rt60) == 0.0);
    REQUIRE(reverb::graph::feedbackGainForRt60(branchA, 0.0) == 0.0);
}

TEST_CASE("Dense figure eight exposes two delayed cross-coupled branches and round trips exactly")
{
    const auto graph = reverb::graph::makeDenseFigureEightGraph();
    REQUIRE(graph.nodes.size() == 37);
    REQUIRE(graph.connections.size() == 44);
    const auto compiled = reverb::graph::compileFeedbackGraph(graph, 48'000.0, 256);
    for (const auto& error : compiled.errors)
        INFO(error);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.planDiagnostics.feedbackRegionCount == 1);
    for (const auto id : { "a-delay-1", "a-delay-2", "b-delay-1", "b-delay-2", "a-cross-gain", "b-cross-gain" })
        REQUIRE(std::ranges::any_of(graph.nodes, [&](const auto& node) { return node.id == id; }));
    REQUIRE(reverb::graph::parsePatchJson(reverb::graph::writePatchJson(graph)) == graph);
}

TEST_CASE("Dense figure eight remains finite and partition deterministic at supported rates")
{
    for (const auto rate : { 44'100.0, 48'000.0, 96'000.0 }) {
        const reverb::render::RenderRequest request { reverb::graph::makeDenseFigureEightGraph(),
            reverb::render::InputKind::impulse, rate, static_cast<std::size_t>(rate * 3.0) };
        const auto rendered = reverb::render::renderOffline(request);
        REQUIRE(std::ranges::all_of(rendered.left, [](const auto value) { return std::isfinite(value); }));
        REQUIRE(std::ranges::all_of(rendered.right, [](const auto value) { return std::isfinite(value); }));
        REQUIRE(std::ranges::all_of(rendered.left, [](const auto value) { return std::abs(value) < 1.0F; }));
        REQUIRE(rendered.left != rendered.right);
    }

    const auto small = renderPartitioned(reverb::graph::makeDenseFigureEightGraph(), 64);
    const auto uneven = renderPartitioned(reverb::graph::makeDenseFigureEightGraph(), 257);
    REQUIRE(std::ranges::equal(small.first, uneven.first));
    REQUIRE(std::ranges::equal(small.second, uneven.second));
}

TEST_CASE("Dense figure eight stays bounded at control extremes")
{
    const std::array controls {
        reverb::graph::DenseFigureEightControls { .rt60Seconds = 0.4, .dampingHertz = 1'200.0,
            .modulationDepthMilliseconds = 1.5, .wetLevel = 0.75 },
        reverb::graph::DenseFigureEightControls { .rt60Seconds = 8.0, .dampingHertz = 12'000.0,
            .modulationDepthMilliseconds = 0.0, .wetLevel = 0.75 },
    };
    for (const auto& control : controls) {
        const auto rendered = reverb::render::renderOffline({ reverb::graph::makeDenseFigureEightGraph(control),
            reverb::render::InputKind::impulse, 48'000.0, 96'000 });
        REQUIRE(std::ranges::all_of(rendered.left,
            [](const auto value) { return std::isfinite(value) && std::abs(value) < 1.0F; }));
        REQUIRE(std::ranges::all_of(rendered.right,
            [](const auto value) { return std::isfinite(value) && std::abs(value) < 1.0F; }));
    }
}

TEST_CASE("Dense figure eight materially improves late density over Barr and Gravity")
{
    constexpr double rate = 48'000.0;
    constexpr std::size_t frames = 144'000;
    const auto analyse = [](const auto& graph) {
        const reverb::render::RenderRequest request { graph, reverb::render::InputKind::impulse, rate, frames };
        const auto audio = reverb::render::renderOffline(request);
        return reverb::render::measureDensity(audio.left, audio.right, rate);
    };
    const auto barr = analyse(reverb::graph::makeBarrReferenceGraph());
    const auto gravity = analyse(reverb::graph::makeGravityDiffusionGraph(
        reverb::graph::GravityDiffusionControls { .gravity = 0.0, .size = 0.0, .feedback = 0.0,
            .damping = 0.0, .modulation = 0.0 }));
    const auto dense = analyse(reverb::graph::makeDenseFigureEightGraph());
    CAPTURE(barr.regions[1].echoDensity, gravity.regions[1].echoDensity, dense.regions[1].echoDensity,
        barr.regions[2].recurrence, gravity.regions[2].recurrence, dense.regions[2].recurrence);
    REQUIRE(dense.regions[1].echoDensity > std::max(barr.regions[1].echoDensity, gravity.regions[1].echoDensity) + 0.08);
    REQUIRE(dense.regions[2].activePeaksPerSecond > gravity.regions[2].activePeaksPerSecond * 2.0);
}
