#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/RuntimeSnapshot.h>

#include <array>

ReverbPlaygroundProcessor::ReverbPlaygroundProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void ReverbPlaygroundProcessor::prepareToPlay(const double sampleRate, int)
{
    harness_.prepare(sampleRate);
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
    if (buffer.getNumChannels() < 2) {
        buffer.clear();
        return;
    }

    const auto sampleCount = static_cast<std::size_t>(buffer.getNumSamples());
    const std::span<const float> inputLeft { buffer.getReadPointer(0), sampleCount };
    const std::span<const float> inputRight { buffer.getReadPointer(1), sampleCount };
    const std::span<float> outputLeft { buffer.getWritePointer(0), sampleCount };
    const std::span<float> outputRight { buffer.getWritePointer(1), sampleCount };
    harness_.process(inputLeft, inputRight, outputLeft, outputRight);
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
double ReverbPlaygroundProcessor::getTailLengthSeconds() const { return 2.0; }
int ReverbPlaygroundProcessor::getNumPrograms() { return 1; }
int ReverbPlaygroundProcessor::getCurrentProgram() { return 0; }
void ReverbPlaygroundProcessor::setCurrentProgram(int) {}
const juce::String ReverbPlaygroundProcessor::getProgramName(int) { return {}; }
void ReverbPlaygroundProcessor::changeProgramName(int, const juce::String&) {}
void ReverbPlaygroundProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    juce::ValueTree state("ReverbPlayground");
    state.setProperty("masterGain", harness_.masterGain(), nullptr);
    state.setProperty("emergencyMuted", harness_.isEmergencyMuted(), nullptr);
    juce::MemoryOutputStream stream(destinationData, false);
    state.writeToStream(stream);
}

void ReverbPlaygroundProcessor::setStateInformation(const void* data, const int sizeInBytes)
{
    const auto state = juce::ValueTree::readFromData(data, static_cast<std::size_t>(sizeInBytes));
    if (!state.isValid() || !state.hasType("ReverbPlayground"))
        return;
    harness_.setMasterGain(static_cast<float>(state.getProperty("masterGain", 0.5F)));
    harness_.setEmergencyMuted(static_cast<bool>(state.getProperty("emergencyMuted", false)));
}

void ReverbPlaygroundProcessor::triggerImpulse() noexcept { harness_.triggerImpulse(); }
void ReverbPlaygroundProcessor::setMasterGain(const float value) noexcept { harness_.setMasterGain(value); }
void ReverbPlaygroundProcessor::setEmergencyMuted(const bool muted) noexcept { harness_.setEmergencyMuted(muted); }
void ReverbPlaygroundProcessor::requestSafetyReset() noexcept { harness_.requestSafetyReset(); }
float ReverbPlaygroundProcessor::masterGain() const noexcept { return harness_.masterGain(); }
bool ReverbPlaygroundProcessor::isEmergencyMuted() const noexcept { return harness_.isEmergencyMuted(); }
bool ReverbPlaygroundProcessor::isSafetyLatched() const noexcept { return harness_.isSafetyLatched(); }
double ReverbPlaygroundProcessor::activeSampleRate() const noexcept { return harness_.sampleRate(); }

juce::String ReverbPlaygroundProcessor::runtimeSnapshotJson() const
{
    const auto identityErrors = reverb::graph::validateBarrRuntimeIdentity(
        reverb::graph::makeBarrReferenceGraph());
    jassert(identityErrors.empty());
    constexpr auto count = static_cast<std::size_t>(reverb::dsp::BarrParameterId::count);
    std::array<double, count> values {};
    for (std::size_t index = 0; index < values.size(); ++index)
        values[index] = harness_.runtimeParameter(static_cast<reverb::dsp::BarrParameterId>(index));
    const auto json = reverb::graph::writeBarrRuntimeSnapshotJson(activeSampleRate(), values);
    return juce::String::fromUTF8(json.data(), static_cast<int>(json.size()));
}

double ReverbPlaygroundProcessor::setRuntimeParameter(
    const juce::String& nodeId,
    const juce::String& parameterId,
    const double value) noexcept
{
    const auto id = reverb::dsp::findBarrReferenceParameter(
        nodeId.toStdString(), parameterId.toStdString());
    if (!id.has_value()) {
        jassertfalse;
        return value;
    }
    harness_.setRuntimeParameter(*id, value);
    return harness_.runtimeParameter(*id);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverbPlaygroundProcessor();
}
