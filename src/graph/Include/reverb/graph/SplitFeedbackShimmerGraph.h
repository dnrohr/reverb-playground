#pragma once

#include <reverb/graph/GraphDocument.h>

namespace reverb::graph {

struct SplitFeedbackShimmerControls final {
    double normalFeedback { 0.48 };
    double shiftedFeedback { 0.10 };
    double preShiftHighpassHertz { 320.0 };
    double postShiftLowpassHertz { 5'200.0 };
    double wetLevel { 0.68 };
    double pitchSemitones { 12.0 };
    double sizeMilliseconds { 149.0 };
};

inline constexpr double splitShimmerMaximumNormalFeedback = 0.58;
inline constexpr double splitShimmerMaximumShiftedFeedback = 0.14;
inline constexpr double splitShimmerMaximumCombinedFeedback =
    splitShimmerMaximumNormalFeedback + splitShimmerMaximumShiftedFeedback;
inline constexpr double splitShimmerMinimumPitchSemitones = 7.0;
inline constexpr double splitShimmerMaximumPitchSemitones = 12.0;
inline constexpr double splitShimmerMinimumSizeMilliseconds = 90.0;
inline constexpr double splitShimmerMaximumSizeMilliseconds = 240.0;

[[nodiscard]] GraphDocument makeSplitFeedbackShimmerGraph(
    const SplitFeedbackShimmerControls& controls = {});

} // namespace reverb::graph
