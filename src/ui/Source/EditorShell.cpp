#include <reverb/ui/EditorShell.h>

namespace reverb::ui {

EditorShell::EditorShell()
{
    title_.setText("Reverb Playground", juce::dontSendNotification);
    title_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(title_);
}

void EditorShell::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour::fromRGB(24, 27, 31));
}

void EditorShell::resized()
{
    title_.setBounds(getLocalBounds());
}

} // namespace reverb::ui
