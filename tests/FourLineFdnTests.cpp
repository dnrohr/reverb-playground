#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/FourLineFdnGraph.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/render/OfflineRenderer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

namespace {

std::pair<std::vector<float>, std::vector<float>> renderPartitioned(
    reverb::graph::PreparedAcyclicRuntime& runtime, const std::size_t blockSize,
    const std::size_t frames = 48'000)
{
    std::vector<float> left(frames), right(frames), inputLeft(blockSize), inputRight(blockSize);
    for (std::size_t offset = 0; offset < frames; offset += blockSize) {
        const auto count = std::min(blockSize, frames - offset);
        std::ranges::fill(inputLeft, 0.0F);
        if (offset == 0) inputLeft[0] = 1.0F;
        runtime.process(std::span(inputLeft).first(count), std::span(inputRight).first(count),
            std::span(left).subspan(offset, count), std::span(right).subspan(offset, count));
    }
    return { std::move(left), std::move(right) };
}

} // namespace

TEST_CASE("Normalized four-line Hadamard mixing preserves energy and is its own inverse")
{
    using Catch::Approx;
    const std::array<double, 4> input { 0.31, -0.72, 0.19, 0.44 };
    const auto mixed = reverb::graph::hadamard4(input);
    const auto restored = reverb::graph::hadamard4(mixed);
    const auto energy = [](const auto& values) {
        return std::inner_product(values.begin(), values.end(), values.begin(), 0.0);
    };
    REQUIRE(energy(mixed) == Approx(energy(input)).epsilon(1.0e-12));
    for (std::size_t index = 0; index < input.size(); ++index)
        REQUIRE(restored[index] == Approx(input[index]).epsilon(1.0e-12));

    for (std::size_t basis = 0; basis < 4; ++basis) {
        std::array<double, 4> unit {};
        unit[basis] = 1.0;
        REQUIRE(energy(reverb::graph::hadamard4(unit)) == Approx(1.0));
    }
}

TEST_CASE("Four-line FDN maps each unequal traversal to the requested RT60")
{
    using Catch::Approx;
    constexpr std::array traversal { 59.2, 74.8, 88.8, 108.4 };
    constexpr auto rt60 = 2.1;
    for (const auto milliseconds : traversal) {
        const auto gain = reverb::graph::fdnLineGainForRt60(milliseconds, rt60);
        REQUIRE(std::pow(gain, rt60 * 1'000.0 / milliseconds)
            == Approx(0.001).epsilon(1.0e-10));
    }
    REQUIRE(reverb::graph::fdnLineGainForRt60(0.0, rt60) == 0.0);
    REQUIRE(reverb::graph::fdnLineGainForRt60(traversal[0], 0.0) == 0.0);
}

TEST_CASE("Expanded four-line FDN exposes every matrix coefficient and delayed cycle")
{
    const auto graph = reverb::graph::makeFourLineFdnGraph();
    REQUIRE(graph.nodes.size() == 75);
    REQUIRE(graph.connections.size() == 100);
    REQUIRE(graph.layout.nodes.size() == graph.nodes.size());
    REQUIRE(std::ranges::count_if(graph.nodes, [](const auto& node) {
        return node.id.starts_with("matrix-") && node.id.find("-from-") != std::string::npos;
    }) == 16);
    REQUIRE(std::ranges::count_if(graph.nodes, [](const auto& node) {
        return node.id.starts_with("line-delay-");
    }) == 4);
    const auto compiled = reverb::graph::compileFeedbackGraph(graph, 48'000.0, 257);
    for (const auto& error : compiled.errors) INFO(error);
    REQUIRE(compiled.valid());
    REQUIRE(compiled.planDiagnostics.feedbackRegionCount == 1);
    REQUIRE(compiled.delayMemory.withinBudget());
    REQUIRE(reverb::graph::parsePatchJson(reverb::graph::writePatchJson(graph)) == graph);
}

TEST_CASE("Four-line FDN is finite bounded and stereo-distinct across rates and extremes")
{
    const std::array controls {
        reverb::graph::FourLineFdnControls {},
        reverb::graph::FourLineFdnControls { .rt60Seconds = 0.35, .dampingHertz = 1'200.0,
            .modulationDepthMilliseconds = 1.25, .wetLevel = 0.7 },
        reverb::graph::FourLineFdnControls { .rt60Seconds = 8.0, .dampingHertz = 14'000.0,
            .modulationDepthMilliseconds = 0.0, .wetLevel = 0.7 },
    };
    for (const auto rate : { 44'100.0, 48'000.0, 96'000.0 }) {
        for (const auto& control : controls) {
            const auto rendered = reverb::render::renderOffline({
                reverb::graph::makeFourLineFdnGraph(control), reverb::render::InputKind::impulse,
                rate, static_cast<std::size_t>(rate * 1.5),
            });
            REQUIRE(std::ranges::all_of(rendered.left,
                [](const auto value) { return std::isfinite(value) && std::abs(value) < 1.0F; }));
            REQUIRE(std::ranges::all_of(rendered.right,
                [](const auto value) { return std::isfinite(value) && std::abs(value) < 1.0F; }));
            REQUIRE(rendered.left != rendered.right);
        }
    }
}

TEST_CASE("Four-line FDN reset and host partitions are sample exact")
{
    const auto graph = reverb::graph::makeFourLineFdnGraph();
    auto small = reverb::graph::compileFeedbackGraph(graph, 48'000.0, 64);
    auto uneven = reverb::graph::compileFeedbackGraph(graph, 48'000.0, 257);
    REQUIRE(small.valid());
    REQUIRE(uneven.valid());
    const auto first = renderPartitioned(*small.runtime, 64);
    const auto second = renderPartitioned(*uneven.runtime, 257);
    REQUIRE(std::ranges::equal(first.first, second.first));
    REQUIRE(std::ranges::equal(first.second, second.second));
    small.runtime->reset();
    const auto reset = renderPartitioned(*small.runtime, 64);
    REQUIRE(std::ranges::equal(first.first, reset.first));
    REQUIRE(std::ranges::equal(first.second, reset.second));
}
