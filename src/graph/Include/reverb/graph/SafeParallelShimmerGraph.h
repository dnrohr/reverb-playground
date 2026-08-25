#pragma once

#include <reverb/graph/GraphDocument.h>

namespace reverb::graph {

struct SafeParallelShimmerControls final {
    double reverbDecay { 0.55 };
    double shimmerLevel { 0.22 };
    double shimmerDampingHertz { 6'000.0 };
    double wetBalance { 0.75 };
};

inline constexpr double safeParallelShimmerAlignmentMilliseconds = 360.01;
inline constexpr double safeParallelShimmerHighpassHertz = 250.0;
inline constexpr double safeParallelShimmerSemitones = 12.0;

[[nodiscard]] GraphDocument makeSafeParallelShimmerGraph(
    const SafeParallelShimmerControls& controls = {});

} // namespace reverb::graph
