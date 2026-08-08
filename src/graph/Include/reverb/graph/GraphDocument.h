#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace reverb::graph {

struct Node final {
    std::string id;
    std::string type;

    friend bool operator==(const Node&, const Node&) = default;
};

class GraphDocument final {
public:
    static constexpr std::uint32_t schemaVersion = 1;

    void addNode(Node node);
    [[nodiscard]] const std::vector<Node>& nodes() const noexcept;

private:
    std::vector<Node> nodes_;
};

} // namespace reverb::graph
