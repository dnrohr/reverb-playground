#pragma once

#include "PluginProcessor.h"

#include <reverb/ui/EditorShell.h>

class ReverbPlaygroundEditor final : public juce::AudioProcessorEditor {
public:
    explicit ReverbPlaygroundEditor(ReverbPlaygroundProcessor& processor);

    void resized() override;

private:
    reverb::ui::EditorShell shell_;
};
