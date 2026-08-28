#pragma once

#include <reverb/graph/GraphDocument.h>

#include <array>

namespace reverb::graph {

struct FourLineFdnControls final {
    double rt60Seconds { 2.1 };
    double dampingHertz { 7'000.0 };
    double modulationDepthMilliseconds { 0.45 };
    double wetLevel { 0.56 };
};

[[nodiscard]] std::array<double, 4> hadamard4(const std::array<double, 4>& input) noexcept;
[[nodiscard]] double fdnLineGainForRt60(double traversalMilliseconds, double rt60Seconds) noexcept;
[[nodiscard]] GraphDocument makeFourLineFdnGraph(const FourLineFdnControls& controls = {});

} // namespace reverb::graph
