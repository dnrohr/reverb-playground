#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/HostPatchState.h>

#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string factoryPatch(const std::string& name)
{
    std::ifstream stream(std::string(REVERB_FACTORY_PATCH_DIR) + "/" + name);
    REQUIRE(stream.good());
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

} // namespace

TEST_CASE("Host patch state validates and preserves the complete serialized document")
{
    reverb::graph::HostPatchState state;
    const auto patch = factoryPatch("causal-reverse-envelope.rvp.json");
    std::string error;

    REQUIRE(state.store(patch, error));
    REQUIRE(error.empty());
    REQUIRE(state.snapshot() == patch);
    const auto restored = state.document();
    REQUIRE(restored.has_value());
    REQUIRE(restored->nodes.size() == 17);
    REQUIRE(restored->layout.nodes.size() == restored->nodes.size());
    REQUIRE(restored->layout.viewport.zoom > 0.0);
}

TEST_CASE("Invalid or oversized host state cannot replace the last valid patch")
{
    reverb::graph::HostPatchState state;
    const auto valid = factoryPatch("level-gated-room.rvp.json");
    std::string error;
    REQUIRE(state.store(valid, error));

    REQUIRE_FALSE(state.store("{not-json", error));
    REQUIRE_FALSE(error.empty());
    REQUIRE(state.snapshot() == valid);

    const std::string oversized(reverb::graph::HostPatchState::maximumSerializedBytes + 1, 'x');
    REQUIRE_FALSE(state.store(oversized, error));
    REQUIRE(error.find("8 MiB") != std::string::npos);
    REQUIRE(state.snapshot() == valid);

    state.clear();
    REQUIRE_FALSE(state.snapshot().has_value());
    REQUIRE_FALSE(state.document().has_value());
}
