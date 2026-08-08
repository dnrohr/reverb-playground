#pragma once

#include <reverb/graph/GraphDocument.h>

#include <string>
#include <vector>

namespace reverb::graph {

inline constexpr int barrRuntimeContractVersion = 1;

[[nodiscard]] std::string writeBarrRuntimeSnapshotJson(double sampleRate);
[[nodiscard]] std::vector<std::string> validateBarrRuntimeIdentity(const GraphDocument& graph);

} // namespace reverb::graph
