#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/PatchJson.h>
#include <reverb/render/GravityReference.h>

#include <algorithm>
#include <cmath>
#include <ranges>

TEST_CASE("Gravity reference states are causal ordered dense and loudness matched")
{
    using Catch::Approx;
    const auto references = reverb::render::renderGravityReferences();
    REQUIRE(references.size() == 3);
    const auto& inverse = references[0];
    const auto& bloom = references[1];
    const auto& forward = references[2];
    REQUIRE(inverse.leftPcm16Fnv1a == 2'580'234'311'813'821'704ULL);
    REQUIRE(inverse.rightPcm16Fnv1a == 14'800'760'036'719'100'615ULL);
    REQUIRE(bloom.leftPcm16Fnv1a == 10'620'357'761'719'891'320ULL);
    REQUIRE(bloom.rightPcm16Fnv1a == 809'180'521'176'916'468ULL);
    REQUIRE(forward.leftPcm16Fnv1a == 1'957'219'577'285'692'479ULL);
    REQUIRE(forward.rightPcm16Fnv1a == 3'302'036'075'149'738'871ULL);
    REQUIRE(inverse.id == "inverse"); REQUIRE(bloom.id == "bloom"); REQUIRE(forward.id == "forward");
    REQUIRE(inverse.rawMetrics.onsetFrame > 0);
    REQUIRE(bloom.rawMetrics.onsetFrame > 0);
    REQUIRE(forward.rawMetrics.onsetFrame > 0);
    CAPTURE(inverse.rawMetrics.timeToPeakMs, bloom.rawMetrics.timeToPeakMs, forward.rawMetrics.timeToPeakMs,
        inverse.rawMetrics.earlyLateEnergyRatioDb, bloom.rawMetrics.earlyLateEnergyRatioDb,
        forward.rawMetrics.earlyLateEnergyRatioDb, inverse.rawMetrics.postPeakEnergyFraction,
        bloom.rawMetrics.postPeakEnergyFraction, forward.rawMetrics.postPeakEnergyFraction);
    REQUIRE(inverse.rawMetrics.timeToPeakMs > bloom.rawMetrics.timeToPeakMs + 100.0);
    REQUIRE(bloom.rawMetrics.timeToPeakMs > forward.rawMetrics.timeToPeakMs + 50.0);
    REQUIRE(inverse.rawMetrics.earlyLateEnergyRatioDb < -2.0);
    REQUIRE(bloom.rawMetrics.earlyLateEnergyRatioDb > inverse.rawMetrics.earlyLateEnergyRatioDb);
    REQUIRE(forward.rawMetrics.earlyLateEnergyRatioDb > bloom.rawMetrics.earlyLateEnergyRatioDb);
    REQUIRE(inverse.rawMetrics.postPeakEnergyFraction > 0.05);
    REQUIRE(bloom.rawMetrics.postPeakEnergyFraction > 0.20);
    CAPTURE(bloom.rawMetrics.occupiedTenMsWindowFraction,
        bloom.rawMetrics.strongestThreeWindowEnergyFraction);
    REQUIRE(bloom.rawMetrics.occupiedTenMsWindowFraction > 0.20);
    REQUIRE(bloom.rawMetrics.strongestThreeWindowEnergyFraction < 0.35);
    REQUIRE(forward.rawMetrics.postPeakEnergyFraction > 0.20);
    for (const auto& reference : references) {
        REQUIRE(reference.matchedMetrics.integratedEnergyDb == Approx(bloom.rawMetrics.integratedEnergyDb).margin(1.0e-4));
        REQUIRE(reference.matchedMetrics.peakLevelDbfs < -6.0);
        REQUIRE(std::ranges::all_of(reference.loudnessMatched.left, [](float value) { return std::isfinite(value); }));
        REQUIRE(std::ranges::all_of(reference.loudnessMatched.right, [](float value) { return std::isfinite(value); }));
        REQUIRE(reference.leftPcm16Fnv1a != 0);
        REQUIRE(reference.rightPcm16Fnv1a != 0);
    }
}

TEST_CASE("Gravity reference tuning is deterministic after serialization reload")
{
    const auto references = reverb::render::renderGravityReferences();
    for (const auto& reference : references) {
        const auto graph = reverb::graph::makeGravityDiffusionGraph(reference.controls);
        const auto reloaded = reverb::graph::parsePatchJson(reverb::graph::writePatchJson(graph));
        const reverb::render::RenderRequest request {
            reloaded, reverb::render::InputKind::impulse, 48'000.0, 240'000,
        };
        const auto afterReload = reverb::render::renderOffline(request);
        REQUIRE(afterReload.left == reference.raw.left);
        REQUIRE(afterReload.right == reference.raw.right);
        const auto json = reverb::render::writeGravityReferenceJson(reference, 48'000.0, 240'000);
        REQUIRE(json.find("\"referenceId\": \"" + reference.id + "\"") != std::string::npos);
    }
}
