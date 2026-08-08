#include <reverb/graph/GraphDocument.h>

#include <utility>

namespace reverb::graph {

void GraphDocument::addNode(Node node)
{
    nodes_.push_back(std::move(node));
}

const std::vector<Node>& GraphDocument::nodes() const noexcept
{
    return nodes_;
}

} // namespace reverb::graph
