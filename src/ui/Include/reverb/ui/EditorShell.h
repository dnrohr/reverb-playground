#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace reverb::ui {

class EditorShell final : public juce::Component {
public:
    EditorShell();

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    juce::Label title_;
};

} // namespace reverb::ui
