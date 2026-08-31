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
    REQUIRE(restored.wetGain() == Catch::Approx(0.25F));
    REQUIRE(restored.dryGain() == Catch::Approx(0.0F));
    REQUIRE_FALSE(restored.isEmergencyMuted());
    REQUIRE_FALSE(nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString()).contains("restoredPatch"));
}

TEST_CASE("Temporary graph preview compiles without mutating saved host state")
{
    ReverbPlaygroundProcessor processor; processor.prepareToPlay(48'000.0, 64);
    const auto saved = factoryPatch("gravity-diffusion.rvp.json");
    REQUIRE(nlohmann::json::parse(processor.storePatchStateJson(saved).toStdString()).at("accepted") == true);
    const auto preview = factoryPatch("causal-reverse-envelope.rvp.json");
    const auto result = nlohmann::json::parse(processor.previewGraphJson(preview).toStdString());
    REQUIRE(result.at("accepted") == true); REQUIRE(result.at("revision").get<std::uint64_t>() > 0);
    const auto snapshot = nlohmann::json::parse(processor.runtimeSnapshotJson().toStdString());
    REQUIRE(snapshot.at("restoredPatch") == nlohmann::json::parse(saved));
    juce::MemoryBlock bytes; processor.getStateInformation(bytes);
    ReverbPlaygroundProcessor restored; restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    REQUIRE(nlohmann::json::parse(restored.runtimeSnapshotJson().toStdString()).at("restoredPatch") == nlohmann::json::parse(saved));
}

TEST_CASE("Host state stores independent wet and dry gains without the obsolete master gain")
{
    ReverbPlaygroundProcessor source;
    source.setWetGain(0.73F);
    source.setDryGain(0.21F);
    juce::MemoryBlock bytes;
    source.getStateInformation(bytes);
    const auto state = juce::ValueTree::readFromData(bytes.getData(), bytes.getSize());
    REQUIRE(static_cast<int>(state.getProperty("formatVersion")) == 2);
    REQUIRE_FALSE(state.hasProperty("masterGain"));
    REQUIRE(static_cast<float>(state.getProperty("wetGain")) == Catch::Approx(0.73F));
    REQUIRE(static_cast<float>(state.getProperty("dryGain")) == Catch::Approx(0.21F));

    ReverbPlaygroundProcessor restored;
    restored.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
    REQUIRE(restored.wetGain() == Catch::Approx(0.73F));
    REQUIRE(restored.dryGain() == Catch::Approx(0.21F));
}

TEST_CASE("Plugin survives the VST3 validator sample-rate sequence")
{
    ReverbPlaygroundProcessor processor;
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    for (const auto sampleRate : { 22'050.0, 32'000.0, 44'100.0, 48'000.0,
             88'200.0, 96'000.0, 192'000.0, 384'000.0, 1'234.5678,
             12'345.678, 123'456.78, 1'234'567.8 }) {
        CAPTURE(sampleRate);
        processor.prepareToPlay(sampleRate, buffer.getNumSamples());
        buffer.clear();
        buffer.setSample(0, 0, 0.1F);
        buffer.setSample(1, 0, 0.1F);
        processor.processBlock(buffer, midi);
        for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (auto frame = 0; frame < buffer.getNumSamples(); ++frame)
                REQUIRE(std::isfinite(buffer.getSample(channel, frame)));
    }
}

TEST_CASE("Energy telemetry follows the active compiled graph revision and is free when disabled")
{
    ReverbPlaygroundProcessor processor;
    processor.prepareToPlay(48'000.0, 128);
    const auto patch = factoryPatch("reverse-cosmic-shimmer.rvp.json");
    const auto published = nlohmann::json::parse(processor.publishGraphJson(patch).toStdString());
    REQUIRE(published.at("accepted") == true);
    const auto requestedRevision = published.at("revision").get<std::uint64_t>();
    REQUIRE(processor.setEnergyTelemetryEnabled(true));
    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer midi;
    for (auto block = 0; block < 200; ++block) {
        buffer.clear();
        // Keep exciting both inputs while the graph compiles asynchronously. This
        // makes the assertion independent of how quickly a Debug CI runner swaps
        // the requested revision onto the audio thread.
        buffer.setSample(0, 0, 0.1F);
        buffer.setSample(1, 0, 0.1F);
        processor.processBlock(buffer, midi);
        const auto energy = nlohmann::json::parse(processor.energyTelemetryJson().toStdString());
        if (energy.at("revision") == requestedRevision && energy.at("generation").get<int>() > 0) {
            REQUIRE(energy.at("coherent") == true);
            REQUIRE(energy.at("nodes").size() > 10);
            REQUIRE(std::ranges::any_of(energy.at("nodes"), [](const auto& node) {
                return node.at("rms").template get<float>() > 0.0F;
            }));
            processor.setEnergyTelemetryEnabled(false);
            const auto disabled = nlohmann::json::parse(processor.energyTelemetryJson().toStdString());
            REQUIRE(disabled.at("enabled") == false);
            REQUIRE(disabled.at("nodes").empty());
            return;
        }
        std::this_thread::yield();
    }
    FAIL("compiled graph energy did not publish its active revision");
}
