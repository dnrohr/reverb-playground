#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

namespace reverb::ui {

class EditorShell final : public juce::Component,
                          public juce::FileDragAndDropTarget,
                          private juce::Timer {
public:
    struct Callbacks final {
        std::function<void()> triggerImpulse;
        std::function<void(float)> setWetGain;
        std::function<void(float)> setDryGain;
        std::function<void(float, float, float, bool)> setComparisonAudition;
        std::function<void(bool)> setEmergencyMuted;
        std::function<void()> resetSafety;
        std::function<void()> chooseAudioDevice;
        std::function<juce::String()> statusText;
        std::function<float()> wetGain;
        std::function<float()> dryGain;
        std::function<juce::String()> auditionGainsJson;
        std::function<bool()> emergencyMuted;
        std::function<bool()> isSafetyLatched;
        std::function<juce::String()> runtimeSnapshotJson;
        std::function<double(const juce::String&, const juce::String&, double)> setRuntimeParameter;
        std::function<juce::String(double, double)> startImpulseCapture;
        std::function<juce::String()> impulseCaptureStatusJson;
        std::function<juce::String()> impulseCaptureJson;
        std::function<bool(bool)> setEnergyTelemetryEnabled;
        std::function<juce::String()> energyTelemetryJson;
        std::function<juce::String()> runtimeDiagnosticsJson;
        std::function<juce::String(const juce::String&)> publishGraphJson;
        std::function<juce::String(const juce::String&)> previewGraphJson;
        std::function<juce::String(const juce::String&)> storePatchStateJson;
        bool standaloneAuditionAvailable {};
        std::function<juce::String(const juce::File&)> loadAudioFile;
        std::function<void(int)> setAuditionSourceMode;
        std::function<void()> playAudioFile;
        std::function<void()> pauseAudioFile;
        std::function<void()> stopAudioFile;
        std::function<juce::String(std::int64_t)> seekAudioFile;
        std::function<juce::String(bool, std::int64_t, std::int64_t)> setAudioFileLoop;
        std::function<juce::String()> audioFileTransportJson;
        std::function<juce::String(const juce::File&, int, bool)> startProcessedFileExport;
        std::function<void()> cancelProcessedFileExport;
        std::function<juce::String()> processedFileExportJson;
    };

    explicit EditorShell(Callbacks callbacks);

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    std::optional<juce::WebBrowserComponent::Resource> getWebResource(const juce::String& path) const;
    void chooseAudioFile();
    void loadAudioFile(const juce::File& file);
    void seekFromWaveform(float x);
    void updateTransport();
    void chooseExportFile();
    void setAuditionDrawerExpanded(bool expanded);

    Callbacks callbacks_;
    juce::Label status_;
    juce::Label wetGainLabel_;
    juce::Label dryGainLabel_;
    juce::TextButton deviceButton_ { "AUDIO DEVICE..." };
    juce::TextButton impulseButton_ { "QUICK IMPULSE" };
    juce::TextButton muteButton_ { "EMERGENCY MUTE" };
    juce::TextButton resetButton_ { "RESET SAFETY" };
    juce::Slider wetGain_;
    juce::Slider dryGain_;
    juce::ComboBox sourceMode_;
    juce::TextButton fileButton_ { "LOAD FILE..." };
    juce::TextButton filePlayButton_ { "PLAY" };
    juce::TextButton fileStopButton_ { "STOP" };
    juce::TextButton drawerButton_ { "+" };
    juce::ToggleButton loopButton_ { "LOOP" };
    juce::ComboBox exportRange_;
    juce::TextButton exportButton_ { "EXPORT WAV..." };
    juce::Label fileLabel_;
    juce::Label transportLabel_;
    juce::Label mixDisclosureLabel_;
    juce::Slider seek_;
    juce::Slider loopRange_;
    double exportProgress_ {};
    juce::ProgressBar exportProgressBar_ { exportProgress_ };
    juce::AudioFormatManager formatManager_;
    juce::AudioThumbnailCache thumbnailCache_ { 5 };
    juce::AudioThumbnail thumbnail_ { 512, formatManager_, thumbnailCache_ };
    std::unique_ptr<juce::FileChooser> fileChooser_;
    std::unique_ptr<juce::FileChooser> exportChooser_;
    std::unique_ptr<juce::WebBrowserComponent> browser_;
    juce::Rectangle<int> waveformBounds_;
    std::int64_t fileFrameCount_ {};
    std::int64_t fileCursorFrame_ {};
    double fileSampleRate_ {};
    bool filePlaying_ {};
    bool updatingTransportControls_ {};
    bool auditionDrawerExpanded_ {};
    int impulseFlashTicks_ {};
};

} // namespace reverb::ui
