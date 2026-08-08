#include "PluginEditor.h"

ReverbPlaygroundEditor::ReverbPlaygroundEditor(ReverbPlaygroundProcessor& processor)
    : AudioProcessorEditor(processor)
{
    addAndMakeVisible(shell_);
    setResizable(true, true);
    setResizeLimits(640, 400, 1920, 1200);
    setSize(960, 600);
}

void ReverbPlaygroundEditor::resized()
{
    shell_.setBounds(getLocalBounds());
}
