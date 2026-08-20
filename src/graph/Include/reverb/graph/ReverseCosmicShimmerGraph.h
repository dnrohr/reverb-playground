#pragma once

#include <reverb/graph/GraphDocument.h>

namespace reverb::graph {

struct ReverseCosmicShimmerControls final {
    double riseScale { 1.0 };
    double sizeMilliseconds { 181.0 };
    double normalFeedback { 0.42 };
    double shimmerFeedback { 0.09 };
    double dampingHertz { 4'600.0 };
    double modulationDepthMilliseconds { 1.1 };
    double wetLevel { 0.62 };
};

inline constexpr double reverseCosmicMaximumNormalFeedback = 0.50;
inline constexpr double reverseCosmicMaximumShimmerFeedback = 0.12;
inline constexpr double reverseCosmicMaximumCombinedFeedback = 0.62;

[[nodiscard]] GraphDocument makeReverseCosmicShimmerGraph(
    const ReverseCosmicShimmerControls& controls = {});

} // namespace reverb::graph
