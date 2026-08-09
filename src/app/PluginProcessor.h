#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <reverb/dsp/LiveReferenceHarness.h>

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
    juce::String startImpulseCapture(double lengthMilliseconds, double stopThresholdDb, bool muteLiveInput);
    bool setEnergyTelemetryEnabled(bool enabled) noexcept;
    double setRuntimeParameter(const juce::String& nodeId, const juce::String& parameterId, double value) noexcept;

private:
    reverb::dsp::LiveReferenceHarness harness_;
    std::atomic<double> captureLengthMilliseconds_ { 2'000.0 };
    std::atomic<double> captureStopThresholdDb_ { -80.0 };
    std::atomic<bool> captureMutesLiveInput_ { true };
};
