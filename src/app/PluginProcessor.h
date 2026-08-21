#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <reverb/audio/PreparedAudioFileSource.h>
#include <reverb/dsp/LiveReferenceHarness.h>
#include <reverb/dsp/NumericalSafetyGuard.h>
#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/HostPatchState.h>
#include <reverb/render/ProcessedFileExporter.h>

#include <atomic>
#include <vector>

class ReverbPlaygroundProcessor final : public juce::AudioProcessor {
public:
    ReverbPlaygroundProcessor();

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void triggerImpulse() noexcept;
    void setMasterGain(float linearGain) noexcept;
    void setEmergencyMuted(bool muted) noexcept;
    void requestSafetyReset() noexcept;
    [[nodiscard]] float masterGain() const noexcept;
    [[nodiscard]] bool isEmergencyMuted() const noexcept;
    [[nodiscard]] bool isSafetyLatched() const noexcept;
    [[nodiscard]] double activeSampleRate() const noexcept;
    [[nodiscard]] juce::String runtimeSnapshotJson() const;
    [[nodiscard]] juce::String impulseCaptureStatusJson() const;
    [[nodiscard]] juce::String impulseCaptureJson() const;
    [[nodiscard]] juce::String energyTelemetryJson() const;
    [[nodiscard]] juce::String runtimeDiagnosticsJson() const;
    [[nodiscard]] juce::String audioFileTransportJson() const;
    [[nodiscard]] juce::String publishGraphJson(const juce::String& patchJson);
    [[nodiscard]] juce::String storePatchStateJson(const juce::String& patchJson);
    juce::String startImpulseCapture(double lengthMilliseconds, double stopThresholdDb, bool muteLiveInput);
    bool setEnergyTelemetryEnabled(bool enabled) noexcept;
    double setRuntimeParameter(const juce::String& nodeId, const juce::String& parameterId, double value) noexcept;
    bool loadAudioFile(const juce::File& file, std::string& error);
    void setAuditionSourceMode(reverb::audio::AuditionSourceMode mode);
    [[nodiscard]] reverb::audio::AuditionSourceMode auditionSourceMode() const noexcept;
    void setProcessedAudition(bool processed) noexcept;
    [[nodiscard]] bool isProcessedAudition() const noexcept;
    void playAudioFile();
    void pauseAudioFile();
    void stopAudioFile();
    bool seekAudioFile(std::int64_t sourceFrame, std::string& error);
    bool setAudioFileLoop(bool enabled, std::int64_t startSourceFrame, std::int64_t endSourceFrame, std::string& error);
    bool startProcessedFileExport(const juce::File& destination, reverb::render::FileExportMode mode,
        bool overwriteConfirmed, std::string& error);
    void cancelProcessedFileExport() noexcept;
    [[nodiscard]] juce::String processedFileExportJson() const;

private:
    reverb::audio::PreparedAudioFileSource audioFileSource_;
    reverb::render::ProcessedFileExporter fileExporter_;
    juce::File loadedAudioFile_;
    reverb::dsp::LiveReferenceHarness harness_;
    reverb::graph::AcyclicRuntimeHost graphHost_;
    reverb::graph::HostPatchState hostPatchState_;
    reverb::dsp::NumericalSafetyGuard graphLeftGuard_;
    reverb::dsp::NumericalSafetyGuard graphRightGuard_;
    reverb::dsp::ImpulseCapture graphCapture_;
    reverb::dsp::RuntimeDiagnostics graphDiagnostics_;
    std::vector<float> graphInputLeft_;
    std::vector<float> graphInputRight_;
    std::vector<float> previousSourceLeft_;
    std::vector<float> previousSourceRight_;
    std::atomic<double> graphSampleRate_ {};
    std::atomic<std::size_t> graphMaximumBlockSize_ {};
    std::atomic<bool> graphAudioEnabled_ {};
    std::atomic<bool> graphImpulsePending_ {};
    std::atomic<bool> graphSafetyResetPending_ {};
    std::atomic<bool> graphSafetyLatched_ {};
    std::atomic<bool> transportGraphResetPending_ {};
    std::atomic<bool> impulseRequested_ {};
    std::atomic<bool> resumeFileOnReturn_ {};
    std::atomic<bool> processedAudition_ { true };
    std::atomic<bool> graphCaptureMode_ {};
    std::atomic<double> captureLengthMilliseconds_ { 2'000.0 };
    std::atomic<double> captureStopThresholdDb_ { -80.0 };
    std::atomic<bool> captureMutesLiveInput_ { true };
    std::atomic<reverb::audio::AuditionSourceMode> auditionSourceMode_ {
        reverb::audio::AuditionSourceMode::liveInput
    };
    reverb::audio::AuditionSourceMode activeAuditionSourceMode_ {
        reverb::audio::AuditionSourceMode::liveInput
    };
    reverb::audio::AuditionSourceMode transitionFromSourceMode_ {
        reverb::audio::AuditionSourceMode::liveInput
    };
    std::size_t sourceTransitionFramesRemaining_ {};
    std::size_t sourceTransitionFramesTotal_ {};
};
