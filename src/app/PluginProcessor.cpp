#include "PluginProcessor.h"

#include "PluginEditor.h"

ReverbPlaygroundProcessor::ReverbPlaygroundProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void ReverbPlaygroundProcessor::prepareToPlay(double, int)
{
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
    const auto inputChannels = getTotalNumInputChannels();
    const auto outputChannels = getTotalNumOutputChannels();

    for (auto channel = inputChannels; channel < outputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
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
double ReverbPlaygroundProcessor::getTailLengthSeconds() const { return 0.0; }
int ReverbPlaygroundProcessor::getNumPrograms() { return 1; }
int ReverbPlaygroundProcessor::getCurrentProgram() { return 0; }
void ReverbPlaygroundProcessor::setCurrentProgram(int) {}
const juce::String ReverbPlaygroundProcessor::getProgramName(int) { return {}; }
void ReverbPlaygroundProcessor::changeProgramName(int, const juce::String&) {}
void ReverbPlaygroundProcessor::getStateInformation(juce::MemoryBlock&) {}
void ReverbPlaygroundProcessor::setStateInformation(const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverbPlaygroundProcessor();
}
