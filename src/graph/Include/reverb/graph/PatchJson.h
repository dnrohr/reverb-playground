#pragma once

#include <reverb/graph/GraphDocument.h>

#include <string>
#include <string_view>

namespace reverb::graph {

[[nodiscard]] GraphDocument parsePatchJson(std::string_view jsonText);
[[nodiscard]] std::string writePatchJson(const GraphDocument& document);

} // namespace reverb::graph
