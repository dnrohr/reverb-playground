#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/RuntimeSnapshot.h>

#include <array>
#include <nlohmann/json.hpp>

ReverbPlaygroundProcessor::ReverbPlaygroundProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void ReverbPlaygroundProcessor::prepareToPlay(const double sampleRate, int)
{
    harness_.prepare(sampleRate);
}

void ReverbPlaygroundProcessor::releaseResources()
{
}

bool ReverbPlaygroundProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ReverbPlaygroundProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2) {
        buffer.clear();
        return;
    }

    const auto sampleCount = static_cast<std::size_t>(buffer.getNumSamples());
    const std::span<const float> inputLeft { buffer.getReadPointer(0), sampleCount };
    const std::span<const float> inputRight { buffer.getReadPointer(1), sampleCount };
    const std::span<float> outputLeft { buffer.getWritePointer(0), sampleCount };
    const std::span<float> outputRight { buffer.getWritePointer(1), sampleCount };
    harness_.process(inputLeft, inputRight, outputLeft, outputRight);
}

juce::AudioProcessorEditor* ReverbPlaygroundProcessor::createEditor()
{
    return new ReverbPlaygroundEditor(*this);
}

bool ReverbPlaygroundProcessor::hasEditor() const
{
    return true;
}

const juce::String ReverbPlaygroundProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ReverbPlaygroundProcessor::acceptsMidi() const { return false; }
bool ReverbPlaygroundProcessor::producesMidi() const { return false; }
bool ReverbPlaygroundProcessor::isMidiEffect() const { return false; }
double ReverbPlaygroundProcessor::getTailLengthSeconds() const { return 2.0; }
int ReverbPlaygroundProcessor::getNumPrograms() { return 1; }
int ReverbPlaygroundProcessor::getCurrentProgram() { return 0; }
void ReverbPlaygroundProcessor::setCurrentProgram(int) {}
const juce::String ReverbPlaygroundProcessor::getProgramName(int) { return {}; }
void ReverbPlaygroundProcessor::changeProgramName(int, const juce::String&) {}
void ReverbPlaygroundProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    juce::ValueTree state("ReverbPlayground");
    state.setProperty("masterGain", harness_.masterGain(), nullptr);
    state.setProperty("emergencyMuted", harness_.isEmergencyMuted(), nullptr);
    juce::MemoryOutputStream stream(destinationData, false);
    state.writeToStream(stream);
}

void ReverbPlaygroundProcessor::setStateInformation(const void* data, const int sizeInBytes)
{
    const auto state = juce::ValueTree::readFromData(data, static_cast<std::size_t>(sizeInBytes));
    if (!state.isValid() || !state.hasType("ReverbPlayground"))
        return;
    harness_.setMasterGain(static_cast<float>(state.getProperty("masterGain", 0.5F)));
    harness_.setEmergencyMuted(static_cast<bool>(state.getProperty("emergencyMuted", false)));
}

void ReverbPlaygroundProcessor::triggerImpulse() noexcept { harness_.triggerImpulse(); }
void ReverbPlaygroundProcessor::setMasterGain(const float value) noexcept { harness_.setMasterGain(value); }
void ReverbPlaygroundProcessor::setEmergencyMuted(const bool muted) noexcept { harness_.setEmergencyMuted(muted); }
void ReverbPlaygroundProcessor::requestSafetyReset() noexcept { harness_.requestSafetyReset(); }
float ReverbPlaygroundProcessor::masterGain() const noexcept { return harness_.masterGain(); }
bool ReverbPlaygroundProcessor::isEmergencyMuted() const noexcept { return harness_.isEmergencyMuted(); }
bool ReverbPlaygroundProcessor::isSafetyLatched() const noexcept { return harness_.isSafetyLatched(); }
double ReverbPlaygroundProcessor::activeSampleRate() const noexcept { return harness_.sampleRate(); }

juce::String ReverbPlaygroundProcessor::runtimeSnapshotJson() const
{
    const auto identityErrors = reverb::graph::validateBarrRuntimeIdentity(
        reverb::graph::makeBarrReferenceGraph());
    jassert(identityErrors.empty());
    constexpr auto count = static_cast<std::size_t>(reverb::dsp::BarrParameterId::count);
    std::array<double, count> values {};
    for (std::size_t index = 0; index < values.size(); ++index)
        values[index] = harness_.runtimeParameter(static_cast<reverb::dsp::BarrParameterId>(index));
    const auto json = reverb::graph::writeBarrRuntimeSnapshotJson(activeSampleRate(), values);
    return juce::String::fromUTF8(json.data(), static_cast<int>(json.size()));
}

double ReverbPlaygroundProcessor::setRuntimeParameter(
    const juce::String& nodeId,
    const juce::String& parameterId,
    const double value) noexcept
{
    const auto id = reverb::dsp::findBarrReferenceParameter(
        nodeId.toStdString(), parameterId.toStdString());
    if (!id.has_value()) {
        jassertfalse;
        return value;
    }
    harness_.setRuntimeParameter(*id, value);
    return harness_.runtimeParameter(*id);
}

juce::String ReverbPlaygroundProcessor::startImpulseCapture(
    const double lengthMilliseconds,
    const double stopThresholdDb,
    const bool muteLiveInput)
{
    const auto bounded = harness_.requestImpulseCapture({
        .maximumLengthMilliseconds = lengthMilliseconds,
        .stopThresholdDb = stopThresholdDb,
        .muteLiveInput = muteLiveInput,
        .impulseLevel = 0.1F,
    });
    captureLengthMilliseconds_.store(bounded.maximumLengthMilliseconds, std::memory_order_release);
    captureStopThresholdDb_.store(bounded.stopThresholdDb, std::memory_order_release);
    captureMutesLiveInput_.store(bounded.muteLiveInput, std::memory_order_release);
    return impulseCaptureStatusJson();
}

juce::String ReverbPlaygroundProcessor::impulseCaptureStatusJson() const
{
    const auto state = harness_.captureState();
    const auto stateName = state == reverb::dsp::ImpulseCaptureState::armed ? "armed"
        : state == reverb::dsp::ImpulseCaptureState::capturing ? "capturing"
        : state == reverb::dsp::ImpulseCaptureState::complete ? "complete" : "idle";
    const auto sampleRate = activeSampleRate();
    const auto frames = harness_.capturedFrames();
    const nlohmann::ordered_json json {
        { "state", stateName },
        { "generation", harness_.captureGeneration() },
        { "capturedFrames", frames },
        { "capturedMilliseconds", sampleRate > 0.0 ? 1'000.0 * static_cast<double>(frames) / sampleRate : 0.0 },
        { "maximumLengthMilliseconds", captureLengthMilliseconds_.load(std::memory_order_acquire) },
        { "stopThresholdDb", captureStopThresholdDb_.load(std::memory_order_acquire) },
        { "muteLiveInput", captureMutesLiveInput_.load(std::memory_order_acquire) },
        { "impulseLevel", 0.1 },
    };
    const auto text = json.dump();
    return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

juce::String ReverbPlaygroundProcessor::impulseCaptureJson() const
{
    const auto capture = harness_.copyLatestCapture();
    const nlohmann::ordered_json json {
        { "formatVersion", 1 },
        { "generation", capture.generation },
        { "sampleRate", capture.sampleRate },
        { "frameCount", capture.left.size() },
        { "maximumLengthMilliseconds", capture.config.maximumLengthMilliseconds },
        { "stopThresholdDb", capture.config.stopThresholdDb },
        { "muteLiveInput", capture.config.muteLiveInput },
        { "impulseLevel", capture.config.impulseLevel },
        { "stopReason", capture.stoppedAtThreshold ? "threshold" : "maximum-length" },
        { "left", capture.left },
        { "right", capture.right },
    };
    const auto text = json.dump();
    return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

bool ReverbPlaygroundProcessor::setEnergyTelemetryEnabled(const bool enabled) noexcept
{
    harness_.setEnergyTelemetryEnabled(enabled);
    return enabled;
}

juce::String ReverbPlaygroundProcessor::energyTelemetryJson() const
{
    const auto snapshot = harness_.energyTelemetrySnapshot();
    auto nodes = nlohmann::ordered_json::array();
    for (std::size_t index = 0; index < snapshot.rms.size(); ++index) {
        const auto id = reverb::dsp::barrEnergyLaneNodeId(
            static_cast<reverb::dsp::BarrEnergyLane>(index));
        nodes.push_back({ { "nodeId", std::string(id) }, { "rms", snapshot.rms[index] } });
    }
    const nlohmann::ordered_json json {
        { "formatVersion", 1 },
        { "enabled", snapshot.enabled },
        { "coherent", snapshot.coherent },
        { "generation", snapshot.generation },
        { "observedSampleValues", snapshot.observedSampleValues },
        { "nodes", std::move(nodes) },
    };
    const auto text = json.dump();
    return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

juce::String ReverbPlaygroundProcessor::runtimeDiagnosticsJson() const
{
    const auto snapshot = harness_.runtimeDiagnosticsSnapshot();
    const auto violationName = snapshot.lastViolation == reverb::dsp::SafetyViolation::nonFinite
        ? "non-finite"
        : snapshot.lastViolation == reverb::dsp::SafetyViolation::runawayLevel ? "runaway" : "none";
    const auto channelName = snapshot.lastViolationChannel == reverb::dsp::SafetyChannel::left
        ? "left"
        : snapshot.lastViolationChannel == reverb::dsp::SafetyChannel::right ? "right" : "none";
    nlohmann::ordered_json safetyEvent = nullptr;
    if (snapshot.safetyEventCoherent && snapshot.lastViolation != reverb::dsp::SafetyViolation::none) {
        safetyEvent = {
            { "generation", snapshot.safetyEventGeneration },
            { "kind", violationName },
            { "channel", channelName },
            { "sampleIndex", snapshot.lastViolationSampleIndex },
            { "graphRevision", snapshot.lastViolationRevision },
        };
    }
    const auto sampleRate = activeSampleRate();
    const nlohmann::ordered_json json {
        { "formatVersion", 1 },
        { "activeGraphRevision", snapshot.activeRevision },
        { "workloadEstimate", {
            { "basis", "static-estimate" },
            { "scalarOperationsPerSample", reverb::dsp::RuntimeDiagnostics::estimatedScalarOperationsPerSample },
            { "scalarOperationsPerSecond", sampleRate > 0.0
                ? reverb::dsp::RuntimeDiagnostics::estimatedScalarOperationsPerSample * sampleRate : 0.0 },
        } },
        { "liveCpu", {
            { "basis", "measured" },
            { "processedBlocks", snapshot.processedBlocks },
            { "loadPercent", snapshot.liveLoadPercent },
            { "peakLoadPercent", snapshot.peakLoadPercent },
        } },
        { "delayMemory", {
            { "basis", "prepared-allocation" },
            { "lineCount", snapshot.delayLineCount },
            { "bytes", snapshot.delayMemoryBytes },
        } },
        { "clipping", {
            { "basis", "measured" },
            { "samples", snapshot.clippedSamples },
            { "blocks", snapshot.clippedBlocks },
        } },
        { "mute", {
            { "manual", isEmergencyMuted() },
            { "safetyLatched", isSafetyLatched() },
            { "active", isEmergencyMuted() || isSafetyLatched() },
        } },
        { "safetyEventCoherent", snapshot.safetyEventCoherent },
        { "lastSafetyEvent", std::move(safetyEvent) },
        { "recoveryCount", snapshot.recoveryCount },
    };
    const auto text = json.dump();
    return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverbPlaygroundProcessor();
}
