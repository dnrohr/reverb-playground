#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "PluginProcessor.h"

#include <reverb/ui/EditorSizing.h>

#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <iterator>
#include <ranges>
#include <string>

namespace {

class HostChangeListener final : public juce::AudioProcessorListener {
public:
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& details) override
    {
        nonParameterStateChanged = nonParameterStateChanged || details.nonParameterStateChanged;
    }

    bool nonParameterStateChanged {};
};

std::string factoryPatch(const std::string& name)
{
    std::ifstream stream(std::string(REVERB_FACTORY_PATCH_DIR) + "/" + name);
    REQUIRE(stream.good());
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

} // namespace

TEST_CASE("Standalone and hosted editors begin from a stable preferred content size")
{
    using reverb::ui::EditorSize;
    using reverb::ui::preferredEditorSize;
    REQUIRE(preferredEditorSize(false, 3840, 2160) == EditorSize { 1280, 800 });
    REQUIRE(preferredEditorSize(true, 1536, 960) == EditorSize { 1280, 800 });
    REQUIRE(preferredEditorSize(true, 1920, 1080) == EditorSize { 1280, 800 });
    REQUIRE(preferredEditorSize(true, 600, 300) == EditorSize { 1280, 800 });
    REQUIRE(preferredEditorSize(true, 0, 0) == EditorSize { 1280, 800 });
}

TEST_CASE("Plugin host state round-trips the complete graph before audio preparation")
{
    ReverbPlaygroundProcessor source;
    HostChangeListener hostChange;
    source.addListener(&hostChange);
    REQUIRE(source.getNumPrograms() == 1);
    REQUIRE(source.getProgramName(0) == "Default");
    const auto patch = factoryPatch("level-gated-room.rvp.json");
    const auto stored = nlohmann::json::parse(source.storePatchStateJson(patch).toStdString());
    REQUIRE(stored.at("accepted") == true);
    REQUIRE(hostChange.nonParameterStateChanged);
    source.removeListener(&hostChange);
    source.setMasterGain(0.37F);
    source.setEmergencyMuted(true);

    juce::MemoryBlock state;
    source.getStateInformation(state);
    REQUIRE(state.getSize() > patch.size());

    ReverbPlaygroundProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(restored.masterGain() == Catch::Approx(0.37F));
    REQUIRE(restored.isEmergencyMuted());

    const auto snapshot = nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString());
    REQUIRE(snapshot.at("productVersion") == REVERB_PRODUCT_VERSION);
    REQUIRE(snapshot.at("buildCommit") == REVERB_BUILD_COMMIT);
    REQUIRE(snapshot.at("restoredPatch").at("semantic").at("nodes").size() == 13);
    REQUIRE(snapshot.at("restoredPatch").at("layout").at("viewport").at("zoom").get<double>() > 0.0);

    restored.prepareToPlay(48'000.0, 64);
    REQUIRE(nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString()).contains("restoredPatch"));
}

TEST_CASE("Plugin host state restores the complete Gravity factory and macro values")
{
    auto gravity = nlohmann::json::parse(factoryPatch("gravity-diffusion.rvp.json"));
    const std::array expectedValues { -0.72, 0.41, 0.63, -0.28, 0.84 };
    const std::array macroIds { "gravity", "size", "feedback", "damping", "modulation" };
    for (auto& node : gravity.at("semantic").at("nodes")) {
        const auto id = node.at("id").get<std::string>();
        for (std::size_t index = 0; index < macroIds.size(); ++index) {
            if (id != macroIds[index]) continue;
            for (auto& parameter : node.at("parameters")) {
                if (parameter.at("id") == "value") parameter["value"] = expectedValues[index];
            }
        }
    }

    ReverbPlaygroundProcessor source;
    const auto stored = nlohmann::json::parse(
        source.storePatchStateJson(gravity.dump()).toStdString());
    REQUIRE(stored.at("accepted") == true);
    juce::MemoryBlock state;
    source.getStateInformation(state);

    ReverbPlaygroundProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto snapshot = nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString());
    const auto& restoredGraph = snapshot.at("restoredPatch");
    REQUIRE(restoredGraph.at("semantic").at("nodes").size() == 58);
    REQUIRE(restoredGraph.at("semantic").at("connections").size() == 94);
    REQUIRE(restoredGraph.at("layout").at("nodes").size() == 58);
    for (const auto& node : restoredGraph.at("semantic").at("nodes")) {
        const auto id = node.at("id").get<std::string>();
        for (std::size_t index = 0; index < macroIds.size(); ++index) {
            if (id != macroIds[index]) continue;
            const auto parameter = std::ranges::find_if(node.at("parameters"), [](const auto& value) {
                return value.at("id") == "value";
            });
            REQUIRE(parameter != node.at("parameters").end());
            REQUIRE(parameter->at("value").get<double>() == Catch::Approx(expectedValues[index]));
        }
    }
    restored.prepareToPlay(96'000.0, 1'024);
    REQUIRE(nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString())
        .at("restoredPatch") == restoredGraph);
}

TEST_CASE("Legacy host state without a graph restores safe audition controls")
{
    juce::ValueTree legacy("ReverbPlayground");
    legacy.setProperty("masterGain", 0.25F, nullptr);
    legacy.setProperty("emergencyMuted", false, nullptr);
    juce::MemoryBlock bytes;
    juce::MemoryOutputStream stream(bytes, false);
    legacy.writeToStream(stream);

    ReverbPlaygroundProcessor restored;
    restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    REQUIRE(restored.masterGain() == Catch::Approx(0.25F));
    REQUIRE_FALSE(restored.isEmergencyMuted());
    REQUIRE_FALSE(nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString()).contains("restoredPatch"));
}
