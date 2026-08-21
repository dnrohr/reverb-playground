#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/graph/RuntimeSnapshot.h>

#include <algorithm>
#include <array>
#include <nlohmann/json.hpp>
#include <stdexcept>

ReverbPlaygroundProcessor::ReverbPlaygroundProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void ReverbPlaygroundProcessor::prepareToPlay(
    const double sampleRate, const int maximumExpectedSamplesPerBlock)
{
    harness_.prepare(sampleRate);
    const auto maximumBlockSize = static_cast<std::size_t>(std::max(1, maximumExpectedSamplesPerBlock));
    audioFileSource_.prepare(sampleRate, maximumBlockSize);
    graphInputLeft_.assign(maximumBlockSize, 0.0F);
    graphInputRight_.assign(maximumBlockSize, 0.0F);
    previousSourceLeft_.assign(maximumBlockSize, 0.0F);
    previousSourceRight_.assign(maximumBlockSize, 0.0F);
    activeAuditionSourceMode_ = auditionSourceMode_.load(std::memory_order_acquire);
    transitionFromSourceMode_ = activeAuditionSourceMode_;
    sourceTransitionFramesRemaining_ = 0;
    sourceTransitionFramesTotal_ = static_cast<std::size_t>(std::max(1.0, std::round(sampleRate * 0.010)));
    graphLeftGuard_.prepare(sampleRate);
    graphRightGuard_.prepare(sampleRate);
    graphCapture_.prepare(sampleRate);
    graphDiagnostics_.prepare(sampleRate, reverb::dsp::BarrReference::delayLineCount(), 0);
    graphSafetyLatched_.store(false, std::memory_order_release);
    graphSampleRate_.store(sampleRate, std::memory_order_release);
    graphMaximumBlockSize_.store(maximumBlockSize, std::memory_order_release);
    if (const auto restored = hostPatchState_.document(); restored.has_value()) {
        static_cast<void>(graphHost_.requestCompilation(*restored, sampleRate, maximumBlockSize, true));
        graphAudioEnabled_.store(true, std::memory_order_release);
    } else {
        static_cast<void>(graphHost_.requestCompilation(
            reverb::graph::makeBarrReferenceGraph(), sampleRate, maximumBlockSize, true));
        graphAudioEnabled_.store(false, std::memory_order_release);
    }
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
    const std::span<const float> deviceInputLeft { buffer.getReadPointer(0), sampleCount };
    const std::span<const float> deviceInputRight { buffer.getReadPointer(1), sampleCount };
    const std::span<float> outputLeft { buffer.getWritePointer(0), sampleCount };
    const std::span<float> outputRight { buffer.getWritePointer(1), sampleCount };
    if (sampleCount > graphInputLeft_.size()) {
        buffer.clear();
        return;
    }
    const auto routedLeft = std::span<float>(graphInputLeft_).first(sampleCount);
    const auto routedRight = std::span<float>(graphInputRight_).first(sampleCount);
    const auto previousLeft = std::span<float>(previousSourceLeft_).first(sampleCount);
    const auto previousRight = std::span<float>(previousSourceRight_).first(sampleCount);
    const auto renderSource = [&](const reverb::audio::AuditionSourceMode mode,
                                  const std::span<float> left,
                                  const std::span<float> right) {
        if (mode == reverb::audio::AuditionSourceMode::liveInput) {
            std::ranges::copy(deviceInputLeft, left.begin());
            std::ranges::copy(deviceInputRight, right.begin());
        } else if (mode == reverb::audio::AuditionSourceMode::audioFile) {
            audioFileSource_.process(left, right);
        } else {
            std::ranges::fill(left, 0.0F);
            std::ranges::fill(right, 0.0F);
        }
    };

    const auto desiredSourceMode = auditionSourceMode_.load(std::memory_order_acquire);
    if (desiredSourceMode != activeAuditionSourceMode_ && sourceTransitionFramesRemaining_ == 0) {
        transitionFromSourceMode_ = activeAuditionSourceMode_;
        sourceTransitionFramesRemaining_ = sourceTransitionFramesTotal_;
    }
    if (sourceTransitionFramesRemaining_ > 0) {
        renderSource(transitionFromSourceMode_, previousLeft, previousRight);
        renderSource(desiredSourceMode, routedLeft, routedRight);
        for (std::size_t index = 0; index < sampleCount; ++index) {
            const auto elapsed = sourceTransitionFramesTotal_ - sourceTransitionFramesRemaining_;
            const auto phase = std::min(1.0, static_cast<double>(elapsed + index + 1)
                / static_cast<double>(sourceTransitionFramesTotal_));
            const auto oldGain = static_cast<float>(std::cos(phase * juce::MathConstants<double>::halfPi));
            const auto newGain = static_cast<float>(std::sin(phase * juce::MathConstants<double>::halfPi));
            routedLeft[index] = previousLeft[index] * oldGain + routedLeft[index] * newGain;
            routedRight[index] = previousRight[index] * oldGain + routedRight[index] * newGain;
        }
        if (sampleCount >= sourceTransitionFramesRemaining_) {
            if (transitionFromSourceMode_ == reverb::audio::AuditionSourceMode::audioFile
                && desiredSourceMode != reverb::audio::AuditionSourceMode::audioFile
                && audioFileSource_.pauseFromAudioThread())
                resumeFileOnReturn_.store(true, std::memory_order_release);
            activeAuditionSourceMode_ = desiredSourceMode;
            sourceTransitionFramesRemaining_ = 0;
        } else {
            sourceTransitionFramesRemaining_ -= sampleCount;
        }
    } else {
        renderSource(activeAuditionSourceMode_, routedLeft, routedRight);
    }
    const std::span<const float> inputLeft = routedLeft;
    const std::span<const float> inputRight = routedRight;
    if (activeAuditionSourceMode_ == reverb::audio::AuditionSourceMode::testImpulse
        && sourceTransitionFramesRemaining_ == 0
        && impulseRequested_.exchange(false, std::memory_order_acq_rel)) {
        harness_.triggerImpulse();
        graphImpulsePending_.store(true, std::memory_order_release);
    }
    if (transportGraphResetPending_.exchange(false, std::memory_order_acq_rel)) {
        harness_.resetSignalState();
        graphHost_.resetActiveRuntimes();
        graphLeftGuard_.reset();
        graphRightGuard_.reset();
    }
    if (!processedAudition_.load(std::memory_order_acquire)) {
        if (graphSafetyLatched_.load(std::memory_order_acquire) || harness_.isSafetyLatched()) {
            buffer.clear();
            return;
        }
        std::ranges::copy(inputLeft, outputLeft.begin());
        std::ranges::copy(inputRight, outputRight.begin());
        if (harness_.isEmergencyMuted()) {
            buffer.clear();
            return;
        }
        const auto gain = harness_.masterGain();
        for (auto& sample : outputLeft) sample *= gain;
        for (auto& sample : outputRight) sample *= gain;
        const auto leftStatus = graphLeftGuard_.inspectAndMute(outputLeft);
        const auto rightStatus = graphRightGuard_.inspectAndMute(outputRight);
        if (leftStatus.violation != reverb::dsp::SafetyViolation::none
            || rightStatus.violation != reverb::dsp::SafetyViolation::none) {
            graphSafetyLatched_.store(true, std::memory_order_release);
            buffer.clear();
        }
        return;
    }
    if (!graphAudioEnabled_.load(std::memory_order_acquire) || !graphHost_.hasRuntime()) {
        harness_.process(inputLeft, inputRight, outputLeft, outputRight);
        return;
    }

    const auto diagnosticsStart = graphDiagnostics_.beginBlock();
    const auto finishGraphDiagnostics = [this, diagnosticsStart, sampleCount](const std::size_t clips = 0) {
        graphDiagnostics_.endBlock(diagnosticsStart, sampleCount, clips);
    };
    if (graphSafetyResetPending_.exchange(false, std::memory_order_acq_rel)) {
        const auto wasLatched = graphSafetyLatched_.load(std::memory_order_acquire);
        graphHost_.resetActiveRuntimes();
        graphLeftGuard_.reset();
        graphRightGuard_.reset();
        graphSafetyLatched_.store(false, std::memory_order_release);
        if (wasLatched) graphDiagnostics_.recordRecovery();
    }
    if (graphSafetyLatched_.load(std::memory_order_acquire)) {
        buffer.clear();
        finishGraphDiagnostics();
        return;
    }

    const auto captureStarted = graphCapture_.beginIfRequested();
    if (captureStarted) graphHost_.resetActiveRuntimes();
    const auto captureConfig = graphCapture_.activeConfig();
    const auto captureActive = graphCapture_.state() == reverb::dsp::ImpulseCaptureState::capturing;
    auto renderLeft = inputLeft;
    auto renderRight = inputRight;
    const auto manualImpulse = graphImpulsePending_.exchange(false, std::memory_order_acq_rel);
    if ((captureStarted || manualImpulse || (captureActive && captureConfig.muteLiveInput))
        && sampleCount <= graphInputLeft_.size()) {
        if (captureActive && captureConfig.muteLiveInput) {
            std::ranges::fill(graphInputLeft_, 0.0F);
            std::ranges::fill(graphInputRight_, 0.0F);
        } else {
            std::ranges::copy(inputLeft, graphInputLeft_.begin());
            std::ranges::copy(inputRight, graphInputRight_.begin());
        }
        if (sampleCount > 0 && (captureStarted || manualImpulse))
            graphInputLeft_.front() += captureStarted ? captureConfig.impulseLevel : 1.0F;
        renderLeft = std::span<const float>(graphInputLeft_).first(sampleCount);
        renderRight = std::span<const float>(graphInputRight_).first(sampleCount);
    }
    graphHost_.process(renderLeft, renderRight, outputLeft, outputRight);
    graphDiagnostics_.setActiveRevision(graphHost_.activeRevision());
    graphCapture_.append(outputLeft, outputRight);
    if (harness_.isEmergencyMuted()) {
        buffer.clear();
        finishGraphDiagnostics();
        return;
    }
    const auto gain = harness_.masterGain();
    for (auto& sample : outputLeft) sample *= gain;
    for (auto& sample : outputRight) sample *= gain;
    const auto leftStatus = graphLeftGuard_.inspectAndMute(outputLeft);
    const auto rightStatus = graphRightGuard_.inspectAndMute(outputRight);
    if (leftStatus.violation != reverb::dsp::SafetyViolation::none
        || rightStatus.violation != reverb::dsp::SafetyViolation::none) {
        if (leftStatus.violation != reverb::dsp::SafetyViolation::none)
            graphDiagnostics_.recordSafety(leftStatus, reverb::dsp::SafetyChannel::left);
        else
            graphDiagnostics_.recordSafety(rightStatus, reverb::dsp::SafetyChannel::right);
        graphSafetyLatched_.store(true, std::memory_order_release);
        buffer.clear();
    }
    finishGraphDiagnostics(leftStatus.clippedSamples + rightStatus.clippedSamples);
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
const juce::String ReverbPlaygroundProcessor::getProgramName(int index)
{
    return index == 0 ? juce::String { "Default" } : juce::String {};
}
void ReverbPlaygroundProcessor::changeProgramName(int, const juce::String&) {}
void ReverbPlaygroundProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    juce::ValueTree state("ReverbPlayground");
    state.setProperty("formatVersion", 1, nullptr);
    state.setProperty("masterGain", harness_.masterGain(), nullptr);
    state.setProperty("emergencyMuted", harness_.isEmergencyMuted(), nullptr);
    if (const auto patch = hostPatchState_.snapshot(); patch.has_value())
        state.setProperty("graphPatchJson", juce::String::fromUTF8(patch->data(), static_cast<int>(patch->size())), nullptr);
    juce::MemoryOutputStream stream(destinationData, false);
    state.writeToStream(stream);
}

void ReverbPlaygroundProcessor::setStateInformation(const void* data, const int sizeInBytes)
{
    const auto state = juce::ValueTree::readFromData(data, static_cast<std::size_t>(sizeInBytes));
    if (!state.isValid() || !state.hasType("ReverbPlayground"))
        return;
    const auto patchJson = state.getProperty("graphPatchJson").toString();
    if (patchJson.isNotEmpty()) {
        std::string error;
        if (!hostPatchState_.store(patchJson.toStdString(), error))
            return;
    }

    harness_.setMasterGain(static_cast<float>(state.getProperty("masterGain", 0.5F)));
    harness_.setEmergencyMuted(static_cast<bool>(state.getProperty("emergencyMuted", false)));
    if (patchJson.isNotEmpty()) {
        const auto sampleRate = graphSampleRate_.load(std::memory_order_acquire);
        const auto maximumBlockSize = graphMaximumBlockSize_.load(std::memory_order_acquire);
        if (sampleRate > 0.0 && maximumBlockSize > 0) {
            static_cast<void>(graphHost_.requestCompilation(
                *hostPatchState_.document(), sampleRate, maximumBlockSize, true));
            graphAudioEnabled_.store(true, std::memory_order_release);
        }
    } else {
        hostPatchState_.clear();
        graphAudioEnabled_.store(false, std::memory_order_release);
    }
}

void ReverbPlaygroundProcessor::triggerImpulse() noexcept
{
    auditionSourceMode_.store(reverb::audio::AuditionSourceMode::testImpulse, std::memory_order_release);
    impulseRequested_.store(true, std::memory_order_release);
}
void ReverbPlaygroundProcessor::setMasterGain(const float value) noexcept { harness_.setMasterGain(value); }
void ReverbPlaygroundProcessor::setEmergencyMuted(const bool muted) noexcept { harness_.setEmergencyMuted(muted); }
void ReverbPlaygroundProcessor::requestSafetyReset() noexcept
{
    harness_.requestSafetyReset();
    graphSafetyResetPending_.store(true, std::memory_order_release);
}
float ReverbPlaygroundProcessor::masterGain() const noexcept { return harness_.masterGain(); }
bool ReverbPlaygroundProcessor::isEmergencyMuted() const noexcept { return harness_.isEmergencyMuted(); }
bool ReverbPlaygroundProcessor::isSafetyLatched() const noexcept
{
    return harness_.isSafetyLatched() || graphSafetyLatched_.load(std::memory_order_acquire);
}
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
    auto json = nlohmann::ordered_json::parse(
        reverb::graph::writeBarrRuntimeSnapshotJson(activeSampleRate(), values));
    json["productVersion"] = REVERB_PRODUCT_VERSION;
    json["buildCommit"] = REVERB_BUILD_COMMIT;
    if (const auto restored = hostPatchState_.snapshot(); restored.has_value())
        json["restoredPatch"] = nlohmann::ordered_json::parse(*restored);
    const auto text = json.dump(2);
    return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

double ReverbPlaygroundProcessor::setRuntimeParameter(
    const juce::String& nodeId,
    const juce::String& parameterId,
    const double value) noexcept
{
    if (parameterId == "value" && graphAudioEnabled_.load(std::memory_order_acquire)
        && graphHost_.setMacroValue(nodeId.toStdString(), value))
        return std::clamp(value, -1.0, 1.0);
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
    const auto graphMode = graphAudioEnabled_.load(std::memory_order_acquire);
    graphCaptureMode_.store(graphMode, std::memory_order_release);
    const reverb::dsp::ImpulseCaptureConfig requested {
        .maximumLengthMilliseconds = lengthMilliseconds,
        .stopThresholdDb = stopThresholdDb,
        .muteLiveInput = muteLiveInput,
        .impulseLevel = 0.1F,
    };
    const auto bounded = graphMode
        ? graphCapture_.request(requested)
        : harness_.requestImpulseCapture(requested);
    captureLengthMilliseconds_.store(bounded.maximumLengthMilliseconds, std::memory_order_release);
    captureStopThresholdDb_.store(bounded.stopThresholdDb, std::memory_order_release);
    captureMutesLiveInput_.store(bounded.muteLiveInput, std::memory_order_release);
    return impulseCaptureStatusJson();
}

juce::String ReverbPlaygroundProcessor::impulseCaptureStatusJson() const
{
    const auto graphMode = graphCaptureMode_.load(std::memory_order_acquire);
    const auto state = graphMode ? graphCapture_.state() : harness_.captureState();
    const auto stateName = state == reverb::dsp::ImpulseCaptureState::armed ? "armed"
        : state == reverb::dsp::ImpulseCaptureState::capturing ? "capturing"
        : state == reverb::dsp::ImpulseCaptureState::complete ? "complete" : "idle";
    const auto sampleRate = activeSampleRate();
    const auto frames = graphMode ? graphCapture_.capturedFrames() : harness_.capturedFrames();
    const nlohmann::ordered_json json {
        { "state", stateName },
        { "generation", graphMode ? graphCapture_.generation() : harness_.captureGeneration() },
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
    const auto capture = graphCaptureMode_.load(std::memory_order_acquire)
        ? graphCapture_.copyLatest()
        : harness_.copyLatestCapture();
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
    const auto graphMode = graphAudioEnabled_.load(std::memory_order_acquire);
    const auto snapshot = graphMode
        ? graphDiagnostics_.snapshot()
        : harness_.runtimeDiagnosticsSnapshot();
    const auto topology = graphHost_.publicationSnapshot();
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
        { "activeGraphRevision", graphMode
            ? topology.activeRevision : snapshot.activeRevision },
        { "topologyPublication", {
            { "requestedRevision", topology.requestedRevision },
            { "pendingRevision", topology.pendingRevision },
            { "activeRevision", topology.activeRevision },
            { "failedRevision", topology.failedRevision },
            { "supersededRequests", topology.supersededRequests },
            { "completedCompilations", topology.completedCompilations },
            { "reclaimedRuntimes", topology.reclaimedRuntimes },
            { "crossfadeFromRevision", topology.crossfadeFromRevision },
            { "crossfadePositionSamples", topology.crossfadePositionSamples },
            { "crossfadeTotalSamples", topology.crossfadeTotalSamples },
            { "completedCrossfades", topology.completedCrossfades },
            { "lastCrossfadeFromRevision", topology.lastCrossfadeFromRevision },
            { "lastCrossfadeToRevision", topology.lastCrossfadeToRevision },
            { "activeDelayLineCount", topology.activeDelayLineCount },
            { "activeDelayMemoryBytes", topology.activeDelayMemoryBytes },
            { "failure", topology.failure },
        } },
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
            { "lineCount", graphMode ? topology.activeDelayLineCount : snapshot.delayLineCount },
            { "bytes", graphMode ? topology.activeDelayMemoryBytes : snapshot.delayMemoryBytes },
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

juce::String ReverbPlaygroundProcessor::audioFileTransportJson() const
{
    const auto snapshot = audioFileSource_.snapshot();
    const nlohmann::ordered_json json {
        { "formatVersion", 1 },
        { "sourceMode", reverb::audio::auditionSourceModeName(auditionSourceMode()) },
        { "state", reverb::audio::transportStateName(snapshot.state) },
        { "fileName", snapshot.fileName },
        { "format", snapshot.format },
        { "error", snapshot.error },
        { "sourceSampleRate", snapshot.sourceSampleRate },
        { "outputSampleRate", snapshot.outputSampleRate },
        { "frameCount", snapshot.frameCount },
        { "cursorSourceFrame", snapshot.cursorSourceFrame },
        { "channels", snapshot.channels },
        { "generation", snapshot.generation },
        { "loop", {
            { "enabled", snapshot.loopEnabled },
            { "startSourceFrame", snapshot.loopStartSourceFrame },
            { "endSourceFrame", snapshot.loopEndSourceFrame },
        } },
        { "underrunEvents", snapshot.underrunEvents },
        { "underrunFrames", snapshot.underrunFrames },
        { "sanitizedSourceSamples", snapshot.sanitizedSourceSamples },
        { "ringCapacityFrames", snapshot.ringCapacityFrames },
        { "preparedBytes", snapshot.preparedBytes },
        { "prepared", snapshot.prepared },
    };
    const auto text = json.dump();
    return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

bool ReverbPlaygroundProcessor::loadAudioFile(const juce::File& file, std::string& error)
{
    const auto loaded = audioFileSource_.loadFile(file, error);
    if (loaded)
        auditionSourceMode_.store(reverb::audio::AuditionSourceMode::audioFile, std::memory_order_release);
    return loaded;
}

void ReverbPlaygroundProcessor::setAuditionSourceMode(
    const reverb::audio::AuditionSourceMode mode)
{
    if (mode == reverb::audio::AuditionSourceMode::audioFile
        && resumeFileOnReturn_.exchange(false, std::memory_order_acq_rel))
        audioFileSource_.play();
    auditionSourceMode_.store(mode, std::memory_order_release);
}

void ReverbPlaygroundProcessor::setProcessedAudition(const bool processed) noexcept
{
    processedAudition_.store(processed, std::memory_order_release);
}

bool ReverbPlaygroundProcessor::isProcessedAudition() const noexcept
{
    return processedAudition_.load(std::memory_order_acquire);
}

reverb::audio::AuditionSourceMode ReverbPlaygroundProcessor::auditionSourceMode() const noexcept
{
    return auditionSourceMode_.load(std::memory_order_acquire);
}

void ReverbPlaygroundProcessor::playAudioFile()
{
    resumeFileOnReturn_.store(false, std::memory_order_release);
    audioFileSource_.play();
    auditionSourceMode_.store(reverb::audio::AuditionSourceMode::audioFile, std::memory_order_release);
}

void ReverbPlaygroundProcessor::pauseAudioFile()
{
    resumeFileOnReturn_.store(false, std::memory_order_release);
    audioFileSource_.pause();
}
void ReverbPlaygroundProcessor::stopAudioFile()
{
    resumeFileOnReturn_.store(false, std::memory_order_release);
    audioFileSource_.stop();
    transportGraphResetPending_.store(true, std::memory_order_release);
}

bool ReverbPlaygroundProcessor::seekAudioFile(
    const std::int64_t sourceFrame, std::string& error)
{
    const auto accepted = audioFileSource_.seek(sourceFrame, error);
    if (accepted) {
        resumeFileOnReturn_.store(false, std::memory_order_release);
        transportGraphResetPending_.store(true, std::memory_order_release);
    }
    return accepted;
}

bool ReverbPlaygroundProcessor::setAudioFileLoop(
    const bool enabled,
    const std::int64_t startSourceFrame,
    const std::int64_t endSourceFrame,
    std::string& error)
{
    return audioFileSource_.setLoop(enabled, startSourceFrame, endSourceFrame, error);
}

juce::String ReverbPlaygroundProcessor::publishGraphJson(const juce::String& patchJson)
{
    try {
        auto document = reverb::graph::parsePatchJson(patchJson.toStdString());
        const auto sampleRate = graphSampleRate_.load(std::memory_order_acquire);
        const auto maximumBlockSize = graphMaximumBlockSize_.load(std::memory_order_acquire);
        if (sampleRate <= 0.0 || maximumBlockSize == 0)
            return R"({"accepted":false,"revision":0,"error":"audio runtime is not prepared"})";
        std::string stateError;
        if (!hostPatchState_.store(patchJson.toStdString(), stateError))
            throw std::runtime_error(stateError);
        const auto revision = graphHost_.requestCompilation(
            std::move(document), sampleRate, maximumBlockSize, true);
        graphAudioEnabled_.store(true, std::memory_order_release);
        updateHostDisplay(juce::AudioProcessorListener::ChangeDetails {}
                              .withNonParameterStateChanged(true));
        const nlohmann::ordered_json result {
            { "accepted", true }, { "revision", revision }, { "error", "" },
        };
        const auto text = result.dump();
        return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
    } catch (const std::exception& error) {
        const nlohmann::ordered_json result {
            { "accepted", false }, { "revision", 0 }, { "error", error.what() },
        };
        const auto text = result.dump();
        return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
    }
}

juce::String ReverbPlaygroundProcessor::storePatchStateJson(const juce::String& patchJson)
{
    std::string error;
    const auto accepted = hostPatchState_.store(patchJson.toStdString(), error);
    if (accepted)
        updateHostDisplay(juce::AudioProcessorListener::ChangeDetails {}
                              .withNonParameterStateChanged(true));
    const nlohmann::ordered_json result {
        { "accepted", accepted }, { "error", error },
    };
    const auto text = result.dump();
    return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverbPlaygroundProcessor();
}
