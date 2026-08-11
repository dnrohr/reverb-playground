#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "PluginProcessor.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
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
