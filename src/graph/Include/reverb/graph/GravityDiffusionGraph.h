#pragma once

#include <reverb/graph/GraphDocument.h>

#include <array>

namespace reverb::graph {

inline constexpr std::array<double, 8> gravityTapBases {
    0.09, 0.09, 0.12, 0.12, 0.14, 0.14, 0.15, 0.15,
};
inline constexpr std::array<double, 8> gravityTapSlopes {
    0.09, 0.09, 0.06, 0.06, -0.06, -0.06, -0.09, -0.09,
};

[[nodiscard]] std::array<double, 8> gravityTapWeights(double gravity) noexcept;
[[nodiscard]] GraphDocument makeGravityDiffusionGraph(double gravity = 0.0);

} // namespace reverb::graph
