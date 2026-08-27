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

class ReverbPlaygroundProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
    ReverbPlaygroundProcessor();
    ~ReverbPlaygroundProcessor() override;

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
    void setWetGain(float linearGain) noexcept;
    void setDryGain(float linearGain) noexcept;
    // Source compatibility for hosts/tests built against pre-M18.5; no longer exposed by the UI.
    void setMasterGain(float linearGain) noexcept { setWetGain(linearGain); }
    void setEmergencyMuted(bool muted) noexcept;
    void requestSafetyReset() noexcept;
    [[nodiscard]] float wetGain() const noexcept;
    [[nodiscard]] float dryGain() const noexcept;
    [[nodiscard]] float masterGain() const noexcept { return wetGain(); }
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
    juce::String startImpulseCapture(double lengthMilliseconds, double stopThresholdDb);
    bool setEnergyTelemetryEnabled(bool enabled) noexcept;
    double setRuntimeParameter(const juce::String& nodeId, const juce::String& parameterId, double value) noexcept;
    bool loadAudioFile(const juce::File& file, std::string& error);
    void setAuditionSourceMode(reverb::audio::AuditionSourceMode mode);
    [[nodiscard]] reverb::audio::AuditionSourceMode auditionSourceMode() const noexcept;
    void setProcessedAudition(bool processed) noexcept
    {
        setWetGain(processed ? 1.0F : 0.0F);
        setDryGain(processed ? 0.0F : 1.0F);
    }
    [[nodiscard]] bool isProcessedAudition() const noexcept { return dryGain() == 0.0F; }
    void playAudioFile();
    void pauseAudioFile();
    void stopAudioFile();
    bool seekAudioFile(std::int64_t sourceFrame, std::string& error);
    bool setAudioFileLoop(bool enabled, std::int64_t startSourceFrame, std::int64_t endSourceFrame, std::string& error);
    bool startProcessedFileExport(const juce::File& destination,
        bool overwriteConfirmed, std::string& error);
    bool startProcessedFileExport(const juce::File& destination, reverb::render::FileExportMode,
        bool overwriteConfirmed, std::string& error)
    {
        return startProcessedFileExport(destination, overwriteConfirmed, error);
    }
    void cancelProcessedFileExport() noexcept;
    [[nodiscard]] juce::String processedFileExportJson() const;
    // Message-thread synchronization point used by the timer and deterministic host tests.
    void synchronizeHostLatencyForCurrentGraph();

private:
    void timerCallback() override;
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
    std::atomic<float> wetGainTarget_ { 0.5F };
    std::atomic<float> dryGainTarget_ { 0.0F };
    float wetGainCurrent_ { 0.5F };
    float dryGainCurrent_ { 0.0F };
    std::atomic<bool> graphCaptureMode_ {};
    std::atomic<double> captureLengthMilliseconds_ { 2'000.0 };
    std::atomic<double> captureStopThresholdDb_ { -80.0 };
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
