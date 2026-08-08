#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

namespace reverb::ui {

class EditorShell final : public juce::Component, private juce::Timer {
public:
    struct Callbacks final {
        std::function<void()> triggerImpulse;
        std::function<void(float)> setMasterGain;
        std::function<void(bool)> setEmergencyMuted;
        std::function<void()> resetSafety;
        std::function<void()> chooseAudioDevice;
        std::function<juce::String()> statusText;
        std::function<float()> masterGain;
        std::function<bool()> emergencyMuted;
        std::function<bool()> isSafetyLatched;
    };

    explicit EditorShell(Callbacks callbacks);

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    void timerCallback() override;
    static std::optional<juce::WebBrowserComponent::Resource> getWebResource(const juce::String& path);

    Callbacks callbacks_;
    juce::Label status_;
    juce::Label gainLabel_;
    juce::TextButton deviceButton_ { "AUDIO DEVICE..." };
    juce::TextButton impulseButton_ { "TRIGGER IMPULSE" };
    juce::TextButton muteButton_ { "EMERGENCY MUTE" };
    juce::TextButton resetButton_ { "RESET SAFETY" };
    juce::Slider gain_;
    std::unique_ptr<juce::WebBrowserComponent> browser_;
    int impulseFlashTicks_ {};
};

} // namespace reverb::ui
