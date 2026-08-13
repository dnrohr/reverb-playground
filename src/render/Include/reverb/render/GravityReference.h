#pragma once

#include <reverb/graph/GravityDiffusionGraph.h>
#include <reverb/render/OfflineRenderer.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace reverb::render {

inline constexpr reverb::graph::GravityDiffusionControls gravityInverseReferenceControls {
    -1.0, 1.0, 0.5, 0.15, 0.25,
};
inline constexpr reverb::graph::GravityDiffusionControls gravityBloomReferenceControls {
    0.0, -0.35, 1.0, 0.0, 1.0,
};
inline constexpr reverb::graph::GravityDiffusionControls gravityForwardReferenceControls {
    1.0, -0.65, -0.8, 0.1, 0.15,
};

struct GravityShapeMetrics final {
    std::size_t onsetFrame {};
    double timeToPeakMs {};
    double earlyLateEnergyRatioDb {};
    double peakLevelDbfs {};
    double integratedEnergyDb {};
    double postPeakEnergyFraction {};
    double occupiedTenMsWindowFraction {};
    double strongestThreeWindowEnergyFraction {};
};

struct GravityReferenceRender final {
    std::string id;
    reverb::graph::GravityDiffusionControls controls;
    RenderResult raw;
    RenderResult loudnessMatched;
    GravityShapeMetrics rawMetrics;
    GravityShapeMetrics matchedMetrics;
    double loudnessMatchGain { 1.0 };
    std::uint64_t leftPcm16Fnv1a {};
    std::uint64_t rightPcm16Fnv1a {};
};

[[nodiscard]] GravityShapeMetrics measureGravityShape(
    const RenderResult& result, double sampleRate, double comparisonHorizonSeconds = 0.52);
[[nodiscard]] std::vector<GravityReferenceRender> renderGravityReferences(
    double sampleRate = 48'000.0, double durationSeconds = 5.0);
[[nodiscard]] std::string writeGravityReferenceJson(
    const GravityReferenceRender& reference, double sampleRate, std::size_t frameCount);

} // namespace reverb::render
