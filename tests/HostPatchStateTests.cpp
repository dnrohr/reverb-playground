#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <reverb/graph/HostPatchState.h>
#include <reverb/graph/PatchJson.h>

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

TEST_CASE("Host patch state restores Macro identity and settings")
{
    using namespace reverb::graph;
    GraphDocument document;
    document.nodes = {
        { "macro-gravity", "macro", { { "out", SignalType::control, PortDirection::output } }, {
            { "value", 0.375, "normalized" }, { "default-value", -0.125, "normalized" },
            { "center-detent", 1.0, "boolean" },
        }, "Gravity" },
    };
    document.layout.nodes = { { "macro-gravity", 42.0, 84.0 } };

    HostPatchState state;
    std::string error;
    const auto serialized = writePatchJson(document);
    REQUIRE(state.store(serialized, error));
    const auto restored = state.document();
    REQUIRE(restored.has_value());
    REQUIRE(restored->nodes.size() == 1);
    CHECK(restored->nodes.front().id == "macro-gravity");
    CHECK(restored->nodes.front().name == "Gravity");
    CHECK(restored->nodes.front().parameters[0].value == Catch::Approx(0.375));
    CHECK(restored->nodes.front().parameters[1].value == Catch::Approx(-0.125));
    CHECK(writePatchJson(*restored) == serialized);
}
