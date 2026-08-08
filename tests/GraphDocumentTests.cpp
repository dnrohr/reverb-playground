#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/GraphDocument.h>

TEST_CASE("Graph document stores stable node identity and type")
{
    reverb::graph::GraphDocument graph;
    graph.addNode({ .id = "input-left", .type = "input" });

    REQUIRE(reverb::graph::GraphDocument::schemaVersion == 1);
    REQUIRE(graph.nodes().size() == 1);
    REQUIRE(graph.nodes().front() == reverb::graph::Node { .id = "input-left", .type = "input" });
}
