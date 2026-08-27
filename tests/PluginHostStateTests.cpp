#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "PluginProcessor.h"

#include <reverb/graph/PatchJson.h>
#include <reverb/ui/EditorSizing.h>

#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <iterator>
#include <ranges>
#include <string>
#include <thread>

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
    REQUIRE(preferredEditorSize(true, 1536, 960) == EditorSize { 1200, 720 });
    REQUIRE(preferredEditorSize(true, 1920, 1080) == EditorSize { 1200, 720 });
    REQUIRE(preferredEditorSize(true, 600, 300) == EditorSize { 1200, 720 });
    REQUIRE(preferredEditorSize(true, 0, 0) == EditorSize { 1200, 720 });
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

TEST_CASE("Plugin host state restores every visible Pitch Shift field after preparation")
{
    using namespace reverb::graph;
    GraphDocument document;
    document.nodes = {
        { "input", "stereo-input", {
            { "out-l", SignalType::audio, PortDirection::output },
            { "out-r", SignalType::audio, PortDirection::output },
        }, {} },
        { "pitch-shift-1", "pitch-shift", {
            { "in", SignalType::audio, PortDirection::input },
            { "semitones-mod", SignalType::control, PortDirection::input },
            { "grain-mod", SignalType::control, PortDirection::input },
            { "overlap-mod", SignalType::control, PortDirection::input },
            { "out", SignalType::audio, PortDirection::output },
        }, {
            { "semitones", 12.0, "semitones", ParameterModulation {
                "semitones-mod", 3.0, ModulationPolarity::bipolar, -12.0, 12.0 } },
            { "grain", 84.5, "milliseconds", ParameterModulation {
                "grain-mod", 14.0, ModulationPolarity::bipolar, 20.0, 120.0 } },
            { "overlap", 0.64, "normalized", ParameterModulation {
                "overlap-mod", 0.2, ModulationPolarity::bipolar, 0.1, 1.0 } },
            { "direction", 1.0, "direction" },
            { "phase", 0.373, "cycles" },
        } },
        { "output", "stereo-output", {
            { "in-l", SignalType::audio, PortDirection::input },
            { "in-r", SignalType::audio, PortDirection::input },
        }, {} },
    };
    document.connections = {
        { "input-l-to-pitch", { "input", "out-l" }, { "pitch-shift-1", "in" } },
        { "pitch-to-output-l", { "pitch-shift-1", "out" }, { "output", "in-l" } },
        { "input-r-to-output-r", { "input", "out-r" }, { "output", "in-r" } },
    };
    document.layout.nodes = {
        { "input", 20.0, 120.0 }, { "pitch-shift-1", 320.0, 120.0 }, { "output", 680.0, 120.0 },
    };
    document.layout.viewport = { 17.0, -23.0, 0.83 };
    document.qualityPolicy = QualityPolicy::high;
    const auto serialized = writePatchJson(document);

    ReverbPlaygroundProcessor source;
    REQUIRE(nlohmann::json::parse(source.storePatchStateJson(serialized).toStdString()).at("accepted") == true);
    juce::MemoryBlock state;
    source.getStateInformation(state);

    ReverbPlaygroundProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto beforePrepare = nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString()).at("restoredPatch");
    REQUIRE(beforePrepare == nlohmann::json::parse(serialized));
    restored.prepareToPlay(48'000.0, 64);
    const auto afterPrepare = nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString()).at("restoredPatch");
    REQUIRE(afterPrepare == beforePrepare);
    const auto& pitch = afterPrepare.at("semantic").at("nodes").at(1);
    CHECK(pitch.at("type") == "pitch-shift");
    CHECK(pitch.at("parameters").at(0).at("modulation").at("amount") == 3.0);
    CHECK(pitch.at("parameters").at(3).at("value") == 1.0);
    CHECK(afterPrepare.at("qualityPolicy") == "high");
}

TEST_CASE("Plugin host state restores the complete Safe Parallel Shimmer factory and edited level")
{
    auto shimmer = nlohmann::json::parse(factoryPatch("safe-parallel-shimmer.rvp.json"));
    for (auto& node : shimmer.at("semantic").at("nodes")) {
        if (node.at("id") != "shimmer-level") continue;
        for (auto& parameter : node.at("parameters")) {
            if (parameter.at("id") == "gain") parameter["value"] = 0.17;
        }
    }

    ReverbPlaygroundProcessor source;
    REQUIRE(nlohmann::json::parse(source.storePatchStateJson(shimmer.dump()).toStdString()).at("accepted") == true);
    juce::MemoryBlock state;
    source.getStateInformation(state);

    ReverbPlaygroundProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto beforePreparation = nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString()).at("restoredPatch");
    REQUIRE(beforePreparation.at("semantic").at("nodes").size() == 28);
    REQUIRE(beforePreparation.at("semantic").at("connections").size() == 32);
    REQUIRE(beforePreparation.at("layout").at("nodes").size() == 28);
    REQUIRE(beforePreparation == shimmer);

    restored.prepareToPlay(96'000.0, 1'024);
    REQUIRE(nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString())
        .at("restoredPatch") == beforePreparation);
}

TEST_CASE("Plugin host state restores the complete Split Feedback Shimmer factory and edited return")
{
    auto shimmer = nlohmann::json::parse(factoryPatch("split-feedback-shimmer.rvp.json"));
    for (auto& node : shimmer.at("semantic").at("nodes")) {
        if (node.at("id") != "shifted-feedback") continue;
        for (auto& parameter : node.at("parameters")) {
            if (parameter.at("id") == "gain") parameter["value"] = 0.08;
        }
    }

    ReverbPlaygroundProcessor source;
    REQUIRE(nlohmann::json::parse(source.storePatchStateJson(shimmer.dump()).toStdString()).at("accepted") == true);
    juce::MemoryBlock state;
    source.getStateInformation(state);

    ReverbPlaygroundProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto beforePreparation = nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString()).at("restoredPatch");
    REQUIRE(beforePreparation.at("semantic").at("nodes").size() == 25);
    REQUIRE(beforePreparation.at("semantic").at("connections").size() == 29);
    REQUIRE(beforePreparation.at("layout").at("nodes").size() == 25);
    REQUIRE(beforePreparation == shimmer);

    restored.prepareToPlay(96'000.0, 1'024);
    REQUIRE(nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString())
        .at("restoredPatch") == beforePreparation);
}

TEST_CASE("Plugin host state restores Reverse Cosmic Shimmer and its reverse-grain phase")
{
    auto shimmer = nlohmann::json::parse(factoryPatch("reverse-cosmic-shimmer.rvp.json"));
    for (auto& node : shimmer.at("semantic").at("nodes")) {
        if (node.at("id") != "reverse-pitch-right") continue;
        for (auto& parameter : node.at("parameters")) {
            if (parameter.at("id") == "phase") parameter["value"] = 0.417;
        }
    }

    ReverbPlaygroundProcessor source;
    REQUIRE(nlohmann::json::parse(source.storePatchStateJson(shimmer.dump()).toStdString()).at("accepted") == true);
    juce::MemoryBlock state;
    source.getStateInformation(state);

    ReverbPlaygroundProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto beforePreparation = nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString()).at("restoredPatch");
    REQUIRE(beforePreparation.at("semantic").at("nodes").size() == 45);
    REQUIRE(beforePreparation.at("semantic").at("connections").size() == 57);
    REQUIRE(beforePreparation.at("layout").at("nodes").size() == 45);
    REQUIRE(beforePreparation == shimmer);

    restored.prepareToPlay(96'000.0, 1'024);
    REQUIRE(nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString())
        .at("restoredPatch") == beforePreparation);
}

TEST_CASE("Plugin emergency mute and explicit recovery remain operational with Pitch Shift feedback")
{
    using namespace reverb::graph;
    const auto audioInput = [](std::string id) { return Port { std::move(id), SignalType::audio, PortDirection::input }; };
    const auto audioOutput = [](std::string id) { return Port { std::move(id), SignalType::audio, PortDirection::output }; };
    GraphDocument graph;
    graph.nodes = {
        { "input", "stereo-input", { audioOutput("out-l"), audioOutput("out-r") }, {} },
        { "sum", "sum", { audioInput("in-a"), audioInput("in-b"), audioOutput("out") }, {} },
        { "pitch", "pitch-shift", {
            audioInput("in"), { "semitones-mod", SignalType::control, PortDirection::input },
            { "grain-mod", SignalType::control, PortDirection::input },
            { "overlap-mod", SignalType::control, PortDirection::input }, audioOutput("out"),
        }, {
            { "semitones", 12.0, "semitones" }, { "grain", 60.0, "milliseconds" },
            { "overlap", 0.5, "normalized" }, { "direction", 0.0, "direction" },
        } },
        { "feedback", "gain", { audioInput("in"), audioOutput("out") }, { { "gain", 0.35, "linear" } } },
        { "delay", "delay", { audioInput("in"), audioOutput("out") }, { { "delay", 11.0, "milliseconds" } } },
        { "output", "stereo-output", { audioInput("in-l"), audioInput("in-r") }, {} },
    };
    graph.connections = {
        { "input-sum", { "input", "out-l" }, { "sum", "in-a" } },
        { "delay-sum", { "delay", "out" }, { "sum", "in-b" } },
        { "sum-pitch", { "sum", "out" }, { "pitch", "in" } },
        { "pitch-feedback", { "pitch", "out" }, { "feedback", "in" } },
        { "feedback-delay", { "feedback", "out" }, { "delay", "in" } },
        { "sum-left", { "sum", "out" }, { "output", "in-l" } },
        { "sum-right", { "sum", "out" }, { "output", "in-r" } },
    };

    ReverbPlaygroundProcessor processor;
    REQUIRE(nlohmann::json::parse(processor.storePatchStateJson(writePatchJson(graph)).toStdString()).at("accepted") == true);
    processor.prepareToPlay(48'000.0, 64);
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    for (auto attempt = 0; attempt < 1'000; ++attempt) {
        buffer.clear();
        processor.processBlock(buffer, midi);
        const auto diagnostics = nlohmann::json::parse(processor.runtimeDiagnosticsJson().toStdString());
        if (diagnostics.at("topologyPublication").at("activeRevision") == 1) break;
        std::this_thread::yield();
    }
    REQUIRE(nlohmann::json::parse(processor.runtimeDiagnosticsJson().toStdString())
        .at("topologyPublication").at("activeRevision") == 1);

    processor.setEmergencyMuted(true);
    buffer.clear();
    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        std::fill_n(buffer.getWritePointer(channel), buffer.getNumSamples(), 100.0F);
    processor.processBlock(buffer, midi);
    REQUIRE(buffer.getMagnitude(0, buffer.getNumSamples()) == 0.0F);
    REQUIRE_FALSE(processor.isSafetyLatched());

    processor.setEmergencyMuted(false);
    for (auto block = 0; block < 1'000 && !processor.isSafetyLatched(); ++block) {
        for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            std::fill_n(buffer.getWritePointer(channel), buffer.getNumSamples(), 100.0F);
        processor.processBlock(buffer, midi);
    }
    REQUIRE(processor.isSafetyLatched());
    REQUIRE(buffer.getMagnitude(0, buffer.getNumSamples()) == 0.0F);

    processor.requestSafetyReset();
    buffer.clear();
    processor.processBlock(buffer, midi);
    REQUIRE_FALSE(processor.isSafetyLatched());
    REQUIRE(buffer.getMagnitude(0, buffer.getNumSamples()) == 0.0F);
}

TEST_CASE("Plugin reports active compiled latency from the message thread and keeps dry bypass explicit")
{
    using namespace reverb::graph;
    const auto audioInput = [](std::string id) { return Port { std::move(id), SignalType::audio, PortDirection::input }; };
    const auto audioOutput = [](std::string id) { return Port { std::move(id), SignalType::audio, PortDirection::output }; };
    GraphDocument graph;
    graph.nodes = {
        { "input", "stereo-input", { audioOutput("out-l"), audioOutput("out-r") }, {} },
        { "delay", "delay", { audioInput("in"), audioOutput("out") }, { { "delay", 10.0, "milliseconds" } } },
        { "pitch", "pitch-shift", {
            audioInput("in"), { "semitones-mod", SignalType::control, PortDirection::input },
            { "grain-mod", SignalType::control, PortDirection::input },
            { "overlap-mod", SignalType::control, PortDirection::input }, audioOutput("out"),
        }, {
            { "semitones", 12.0, "semitones" }, { "grain", 60.0, "milliseconds" },
            { "overlap", 0.5, "normalized" }, { "direction", 0.0, "direction" },
        } },
        { "output", "stereo-output", { audioInput("in-l"), audioInput("in-r") }, {} },
    };
    graph.connections = {
        { "input-delay", { "input", "out-l" }, { "delay", "in" } },
        { "delay-pitch", { "delay", "out" }, { "pitch", "in" } },
        { "pitch-left", { "pitch", "out" }, { "output", "in-l" } },
        { "pitch-right", { "pitch", "out" }, { "output", "in-r" } },
    };

    ReverbPlaygroundProcessor processor;
    REQUIRE(nlohmann::json::parse(processor.storePatchStateJson(writePatchJson(graph)).toStdString()).at("accepted") == true);
    processor.prepareToPlay(48'000.0, 64);
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    for (auto attempt = 0; attempt < 2'000; ++attempt) {
        buffer.clear();
        processor.processBlock(buffer, midi);
        if (nlohmann::json::parse(processor.runtimeDiagnosticsJson().toStdString())
                .at("topologyPublication").at("activeRevision") == 1) break;
        std::this_thread::yield();
    }

    // Hosts that defer a dynamic update do not affect audio or the compiled truth.
    REQUIRE(processor.getLatencySamples() == 0);
    auto diagnostics = nlohmann::json::parse(processor.runtimeDiagnosticsJson().toStdString());
    REQUIRE(diagnostics.at("latency").at("samples") == 17'762);
    REQUIRE(diagnostics.at("latency").at("hostReportedSamples") == 0);
    processor.synchronizeHostLatencyForCurrentGraph();
    REQUIRE(processor.getLatencySamples() == 17'762);

    processor.setProcessedAudition(false);
    processor.synchronizeHostLatencyForCurrentGraph();
    REQUIRE(processor.getLatencySamples() == 0);
    processor.setProcessedAudition(true);
    processor.synchronizeHostLatencyForCurrentGraph();
    REQUIRE(processor.getLatencySamples() == 17'762);

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    ReverbPlaygroundProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    restored.prepareToPlay(48'000.0, 64);
    for (auto attempt = 0; attempt < 2'000; ++attempt) {
        buffer.clear();
        restored.processBlock(buffer, midi);
        if (nlohmann::json::parse(restored.runtimeDiagnosticsJson().toStdString())
                .at("topologyPublication").at("activeRevision") == 1) break;
        std::this_thread::yield();
    }
    restored.synchronizeHostLatencyForCurrentGraph();
    REQUIRE(restored.getLatencySamples() == 17'762);

    GraphDocument direct;
    direct.nodes = {
        { "input", "stereo-input", { audioOutput("out-l"), audioOutput("out-r") }, {} },
        { "output", "stereo-output", { audioInput("in-l"), audioInput("in-r") }, {} },
    };
    direct.connections = {
        { "left", { "input", "out-l" }, { "output", "in-l" } },
        { "right", { "input", "out-r" }, { "output", "in-r" } },
    };
    REQUIRE(nlohmann::json::parse(processor.publishGraphJson(writePatchJson(direct)).toStdString()).at("accepted") == true);
    for (auto attempt = 0; attempt < 2'000; ++attempt) {
        buffer.clear();
        processor.processBlock(buffer, midi);
        if (nlohmann::json::parse(processor.runtimeDiagnosticsJson().toStdString())
                .at("topologyPublication").at("activeRevision") == 2) break;
        std::this_thread::yield();
    }
    REQUIRE(processor.getLatencySamples() == 17'762);
    processor.synchronizeHostLatencyForCurrentGraph();
    REQUIRE(processor.getLatencySamples() == 0);
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
