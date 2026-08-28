#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/DenseFigureEightGraph.h>
#include <reverb/graph/FourLineFdnGraph.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/render/OfflineRenderer.h>
#include <reverb/render/DensityMeasurements.h>

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
    REQUIRE(graph.nodes.size() == 84);
    REQUIRE(graph.connections.size() == 118);
    REQUIRE(graph.layout.nodes.size() == graph.nodes.size());
    REQUIRE(std::ranges::count_if(graph.nodes, [](const auto& node) {
        return node.id.starts_with("matrix-") && node.id.find("-from-") != std::string::npos;
    }) == 16);
    REQUIRE(std::ranges::count_if(graph.nodes, [](const auto& node) {
        return node.id.starts_with("line-delay-");
    }) == 4);
    REQUIRE(std::ranges::count_if(graph.nodes,
        [](const auto& node) { return node.type == "macro"; }) == 3);
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

TEST_CASE("Four-line FDN size decay and width macros sweep click-safely")
{
    constexpr std::size_t blockSize = 128;
    auto compiled = reverb::graph::compileFeedbackGraph(
        reverb::graph::makeFourLineFdnGraph(), 48'000.0, blockSize);
    REQUIRE(compiled.valid());
    std::array<float, blockSize> inputLeft {}, inputRight {}, outputLeft {}, outputRight {};
    float previousLeft = 0.0F;
    float previousRight = 0.0F;
    double largestStep = 0.0;
    double monoEnergy = 0.0;
    for (int block = 0; block < 1'200; ++block) {
        const auto phase = static_cast<double>(block) / 1'199.0;
        REQUIRE(compiled.runtime->setMacroValue("size-macro", phase * 2.0 - 1.0));
        REQUIRE(compiled.runtime->setMacroValue("decay-macro", std::sin(phase * 6.283185307179586)));
        REQUIRE(compiled.runtime->setMacroValue("width-macro", block < 400 ? -1.0
            : block < 800 ? 0.0 : 1.0));
        inputLeft.fill(0.0F);
        inputRight.fill(0.0F);
        if (block % 53 == 0) inputLeft[0] = inputRight[0] = 0.05F;
        compiled.runtime->process(inputLeft, inputRight, outputLeft, outputRight);
        for (std::size_t sample = 0; sample < blockSize; ++sample) {
            REQUIRE(std::isfinite(outputLeft[sample]));
            REQUIRE(std::isfinite(outputRight[sample]));
            REQUIRE(std::abs(outputLeft[sample]) < 1.0F);
            REQUIRE(std::abs(outputRight[sample]) < 1.0F);
            largestStep = std::max(largestStep,
                std::abs(static_cast<double>(outputLeft[sample] - previousLeft)));
            largestStep = std::max(largestStep,
                std::abs(static_cast<double>(outputRight[sample] - previousRight)));
            const auto mono = 0.5 * static_cast<double>(outputLeft[sample] + outputRight[sample]);
            monoEnergy += mono * mono;
            previousLeft = outputLeft[sample];
            previousRight = outputRight[sample];
        }
    }
    CAPTURE(largestStep, monoEnergy);
    REQUIRE(largestStep < 0.5);
    REQUIRE(monoEnergy > 0.0);
}

TEST_CASE("Four-line FDN improves density while retaining compatible stereo")
{
    constexpr double rate = 48'000.0;
    constexpr std::size_t frames = 144'000;
    const auto analyse = [](const auto& graph) {
        const auto audio = reverb::render::renderOffline({ graph,
            reverb::render::InputKind::impulse, rate, frames });
        return reverb::render::measureDensity(audio.left, audio.right, rate);
    };
    const auto figureEight = analyse(reverb::graph::makeDenseFigureEightGraph());
    const auto fdn = analyse(reverb::graph::makeFourLineFdnGraph());
    CAPTURE(figureEight.regions[0].echoDensity, fdn.regions[0].echoDensity,
        figureEight.regions[1].echoDensity, fdn.regions[1].echoDensity,
        figureEight.regions[2].echoDensity, fdn.regions[2].echoDensity,
        figureEight.regions[0].activePeaksPerSecond, fdn.regions[0].activePeaksPerSecond,
        figureEight.regions[2].activePeaksPerSecond, fdn.regions[2].activePeaksPerSecond,
        figureEight.regions[2].recurrence, fdn.regions[2].recurrence,
        fdn.regions[2].stereoCorrelation);
    REQUIRE(fdn.regions[0].echoDensity > figureEight.regions[0].echoDensity + 0.10);
    REQUIRE(fdn.regions[1].echoDensity >= figureEight.regions[1].echoDensity);
    REQUIRE(fdn.regions[2].recurrence < figureEight.regions[2].recurrence * 0.7);
    REQUIRE(std::abs(fdn.regions[2].stereoCorrelation) < 0.98);
}
