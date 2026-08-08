#include <reverb/ui/EditorShell.h>

#include <array>

namespace reverb::ui {
namespace {

const auto background = juce::Colour::fromRGB(15, 18, 22);
const auto panel = juce::Colour::fromRGB(26, 31, 37);
const auto border = juce::Colour::fromRGB(58, 68, 78);
const auto text = juce::Colour::fromRGB(235, 239, 242);
const auto mutedText = juce::Colour::fromRGB(146, 158, 169);
const auto cyan = juce::Colour::fromRGB(69, 208, 204);
const auto amber = juce::Colour::fromRGB(244, 181, 76);
const auto danger = juce::Colour::fromRGB(226, 78, 83);

void styleLabel(juce::Label& label, const juce::Colour colour, const float height, const int style)
{
    label.setColour(juce::Label::textColourId, colour);
    label.setFont(juce::Font(juce::FontOptions(height, style)));
}

} // namespace

EditorShell::EditorShell(Callbacks callbacks)
    : callbacks_(std::move(callbacks))
{
    eyebrow_.setText("AUDIBLE REFERENCE  /  ENGINE 0.1", juce::dontSendNotification);
    styleLabel(eyebrow_, cyan, 13.0F, juce::Font::bold);
    title_.setText("Barr architecture, made audible.", juce::dontSendNotification);
    styleLabel(title_, text, 32.0F, juce::Font::bold);
    subtitle_.setText("Live stereo input enters a shared mono diffusion path, then opens into distinct wet outputs.", juce::dontSendNotification);
    styleLabel(subtitle_, mutedText, 15.0F, juce::Font::plain);
    status_.setJustificationType(juce::Justification::centredLeft);
    styleLabel(status_, text, 13.0F, juce::Font::plain);
    gainLabel_.setText("MASTER AUDITION GAIN", juce::dontSendNotification);
    styleLabel(gainLabel_, mutedText, 12.0F, juce::Font::bold);

    const std::array<juce::Component*, 10> components {
        &eyebrow_, &title_, &subtitle_, &status_, &gainLabel_,
        &deviceButton_, &impulseButton_, &muteButton_, &resetButton_, &gain_,
    };
    for (auto* component : components)
        addAndMakeVisible(component);

    deviceButton_.onClick = [this] { callbacks_.chooseAudioDevice(); };
    impulseButton_.onClick = [this] {
        callbacks_.triggerImpulse();
        impulseFlashTicks_ = 10;
        impulseButton_.setButtonText("IMPULSE SENT");
    };
    muteButton_.setClickingTogglesState(true);
    muteButton_.onClick = [this] { callbacks_.setEmergencyMuted(muteButton_.getToggleState()); };
    resetButton_.onClick = [this] { callbacks_.resetSafety(); };

    gain_.setSliderStyle(juce::Slider::LinearHorizontal);
    gain_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 28);
    gain_.setRange(0.0, 1.0, 0.001);
    gain_.setValue(callbacks_.masterGain(), juce::dontSendNotification);
    gain_.setDoubleClickReturnValue(true, 0.5);
    gain_.setColour(juce::Slider::trackColourId, cyan);
    gain_.setColour(juce::Slider::thumbColourId, text);
    gain_.setColour(juce::Slider::textBoxTextColourId, text);
    gain_.setColour(juce::Slider::textBoxBackgroundColourId, panel);
    gain_.onValueChange = [this] { callbacks_.setMasterGain(static_cast<float>(gain_.getValue())); };
    muteButton_.setToggleState(callbacks_.emergencyMuted(), juce::dontSendNotification);

    impulseButton_.setColour(juce::TextButton::buttonColourId, cyan.darker(0.35F));
    impulseButton_.setColour(juce::TextButton::textColourOffId, text);
    muteButton_.setColour(juce::TextButton::buttonColourId, panel);
    muteButton_.setColour(juce::TextButton::buttonOnColourId, danger);
    resetButton_.setColour(juce::TextButton::buttonColourId, amber.darker(0.55F));
    deviceButton_.setColour(juce::TextButton::buttonColourId, panel.brighter(0.08F));
    startTimerHz(10);
}

void EditorShell::paint(juce::Graphics& graphics)
{
    graphics.fillAll(background);
    auto bounds = getLocalBounds().toFloat().reduced(32.0F);
    bounds.removeFromTop(142.0F);
    auto signalBounds = bounds.removeFromTop(180.0F);
    graphics.setColour(panel);
    graphics.fillRoundedRectangle(signalBounds, 14.0F);
    graphics.setColour(border);
    graphics.drawRoundedRectangle(signalBounds, 14.0F, 1.0F);
    drawSignalChain(graphics, signalBounds.reduced(22.0F));

    auto controls = bounds.reduced(0.0F, 18.0F);
    graphics.setColour(panel);
    graphics.fillRoundedRectangle(controls, 14.0F);
    graphics.setColour(border);
    graphics.drawRoundedRectangle(controls, 14.0F, 1.0F);
}

void EditorShell::drawSignalChain(juce::Graphics& graphics, const juce::Rectangle<float> bounds)
{
    constexpr std::array labels { "STEREO IN", "MONO SUM", "LOW-PASS", "4x ALLPASS", "STEREO OUT" };
    const auto gap = 18.0F;
    const auto boxWidth = (bounds.getWidth() - gap * static_cast<float>(labels.size() - 1))
        / static_cast<float>(labels.size());
    auto x = bounds.getX();
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const juce::Rectangle<float> box { x, bounds.getCentreY() - 31.0F, boxWidth, 62.0F };
        if (index > 0) {
            graphics.setColour(index == labels.size() - 1 ? amber : cyan.withAlpha(0.72F));
            graphics.drawLine(x - gap, bounds.getCentreY(), x, bounds.getCentreY(), 2.0F);
        }
        graphics.setColour(index == 0 || index == labels.size() - 1 ? border.brighter(0.25F) : border);
        graphics.fillRoundedRectangle(box, 8.0F);
        graphics.setColour(text);
        graphics.setFont(juce::Font(juce::FontOptions(12.0F, juce::Font::bold)));
        graphics.drawText(labels[index], box, juce::Justification::centred);
        x += boxWidth + gap;
    }

    graphics.setColour(mutedText);
    graphics.setFont(juce::Font(juce::FontOptions(11.0F)));
    graphics.drawText(
        "MONO CABLES  /  DELAYS IN MILLISECONDS  /  WET ONLY",
        bounds.withTop(bounds.getBottom() - 22.0F),
        juce::Justification::centred);
}

void EditorShell::resized()
{
    auto bounds = getLocalBounds().reduced(32);
    eyebrow_.setBounds(bounds.removeFromTop(22));
    title_.setBounds(bounds.removeFromTop(46));
    subtitle_.setBounds(bounds.removeFromTop(34));
    status_.setBounds(bounds.removeFromTop(34));
    bounds.removeFromTop(186);

    auto controls = bounds.reduced(22, 24);
    auto topRow = controls.removeFromTop(44);
    deviceButton_.setBounds(topRow.removeFromLeft(180));
    topRow.removeFromLeft(12);
    impulseButton_.setBounds(topRow.removeFromLeft(180));
    topRow.removeFromLeft(12);
    muteButton_.setBounds(topRow.removeFromLeft(180));
    topRow.removeFromLeft(12);
    resetButton_.setBounds(topRow.removeFromLeft(150));
    controls.removeFromTop(18);
    gainLabel_.setBounds(controls.removeFromTop(20));
    gain_.setBounds(controls.removeFromTop(40));
}

void EditorShell::timerCallback()
{
    if (!gain_.isMouseButtonDown())
        gain_.setValue(callbacks_.masterGain(), juce::dontSendNotification);
    muteButton_.setToggleState(callbacks_.emergencyMuted(), juce::dontSendNotification);
    const auto safetyLatched = callbacks_.isSafetyLatched();
    if (impulseFlashTicks_ > 0 && --impulseFlashTicks_ == 0)
        impulseButton_.setButtonText("TRIGGER IMPULSE");
    if (muteButton_.getToggleState())
        status_.setText("EMERGENCY MUTE ACTIVE  /  OUTPUT SILENT", juce::dontSendNotification);
    else
        status_.setText(callbacks_.statusText(), juce::dontSendNotification);
    resetButton_.setEnabled(safetyLatched);
    status_.setColour(
        juce::Label::textColourId,
        safetyLatched || muteButton_.getToggleState() ? danger : text);
}

} // namespace reverb::ui
