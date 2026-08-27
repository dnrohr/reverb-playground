#pragma once

#include <reverb/graph/GraphDocument.h>

namespace reverb::graph {

struct DenseFigureEightControls final {
    double rt60Seconds { 2.4 };
    double dampingHertz { 6'200.0 };
    double modulationDepthMilliseconds { 0.7 };
    double wetLevel { 0.58 };
};

[[nodiscard]] double feedbackGainForRt60(double traversalMilliseconds, double rt60Seconds) noexcept;
[[nodiscard]] GraphDocument makeDenseFigureEightGraph(const DenseFigureEightControls& controls = {});

} // namespace reverb::graph
