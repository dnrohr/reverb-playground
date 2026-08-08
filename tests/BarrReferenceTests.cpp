#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/BarrReference.h>
#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/PatchJson.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

TEST_CASE("Barr reference graph has stable serializable mono-sum and stereo-tap topology")
{
    const auto graph = reverb::graph::makeBarrReferenceGraph();
    REQUIRE(reverb::graph::validate(graph).valid());
    REQUIRE(graph.nodes.front().type == "stereo-input");
    REQUIRE(graph.nodes[1].type == "sum");
    REQUIRE(graph.nodes[graph.nodes.size() - 3].id == "left-tap");
    REQUIRE(graph.nodes[graph.nodes.size() - 2].id == "right-tap");

    const auto json = reverb::graph::writePatchJson(graph);
    REQUIRE(reverb::graph::parsePatchJson(json) == graph);
    REQUIRE(reverb::graph::writePatchJson(reverb::graph::parsePatchJson(json)) == json);
}

TEST_CASE("Barr reference impulse produces finite distinct stereo wet output")
{
    constexpr std::size_t sampleCount = 96'000;
    reverb::dsp::BarrReference reference;
    reference.prepare(48'000.0);
    std::vector<float> inputLeft(sampleCount, 0.0F);
    std::vector<float> inputRight(sampleCount, 0.0F);
    std::vector<float> outputLeft(sampleCount, 0.0F);
    std::vector<float> outputRight(sampleCount, 0.0F);
    inputLeft.front() = 1.0F;

    reference.process(inputLeft, inputRight, outputLeft, outputRight);

    REQUIRE(std::ranges::all_of(outputLeft, [](const float sample) { return std::isfinite(sample); }));
    REQUIRE(std::ranges::all_of(outputRight, [](const float sample) { return std::isfinite(sample); }));
    REQUIRE(outputLeft != outputRight);
    const auto leftEnergy = std::inner_product(outputLeft.begin(), outputLeft.end(), outputLeft.begin(), 0.0);
    const auto rightEnergy = std::inner_product(outputRight.begin(), outputRight.end(), outputRight.begin(), 0.0);
    REQUIRE(leftEnergy > 0.0);
    REQUIRE(rightEnergy > 0.0);

    reference.reset();
    std::ranges::fill(inputLeft, 0.0F);
    reference.process(inputLeft, inputRight, outputLeft, outputRight);
    REQUIRE(std::ranges::all_of(outputLeft, [](const float sample) { return sample == 0.0F; }));
    REQUIRE(std::ranges::all_of(outputRight, [](const float sample) { return sample == 0.0F; }));
}
