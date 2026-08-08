#include <reverb/ui/EditorShell.h>
#include <BinaryData.h>

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
    status_.setJustificationType(juce::Justification::centredLeft);
    styleLabel(status_, text, 13.0F, juce::Font::plain);
    gainLabel_.setText("MASTER AUDITION GAIN", juce::dontSendNotification);
    styleLabel(gainLabel_, mutedText, 12.0F, juce::Font::bold);

    const std::array<juce::Component*, 7> components {
        &status_, &gainLabel_,
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

    auto options = juce::WebBrowserComponent::Options {}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2 {}
            .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("reverb-playground-webview-m2-3")))
        .withNativeIntegrationEnabled()
        .withNativeFunction("setRuntimeParameter", [this](const auto& arguments, auto complete) {
            if (arguments.size() != 3) {
                complete(juce::var());
                return;
            }
            complete(callbacks_.setRuntimeParameter(
                arguments[0].toString(),
                arguments[1].toString(),
                static_cast<double>(arguments[2])));
        })
        .withResourceProvider([this](const auto& path) { return getWebResource(path); });
    browser_ = std::make_unique<juce::WebBrowserComponent>(std::move(options));
    addAndMakeVisible(*browser_);
    browser_->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz(10);
}

void EditorShell::paint(juce::Graphics& graphics)
{
    graphics.fillAll(background);
    auto controls = getLocalBounds().toFloat().removeFromTop(88.0F);
    graphics.setColour(panel);
    graphics.fillRect(controls);
    graphics.setColour(border);
    graphics.drawLine(0.0F, controls.getBottom() - 1.0F, controls.getRight(), controls.getBottom() - 1.0F);
}

std::optional<juce::WebBrowserComponent::Resource> EditorShell::getWebResource(const juce::String& path) const
{
    const auto requested = path == "/" ? juce::String("index.html") : path.trimCharactersAtStart("/");
    if (requested == "runtime-snapshot.json") {
        const auto json = callbacks_.runtimeSnapshotJson();
        std::vector<std::byte> bytes(static_cast<std::size_t>(json.getNumBytesAsUTF8()));
        std::memcpy(bytes.data(), json.toRawUTF8(), bytes.size());
        return juce::WebBrowserComponent::Resource { std::move(bytes), "application/json" };
    }
    const char* data = nullptr;
    int size = 0;
    juce::String mime;
    if (requested == "index.html") { data = BinaryData::index_html; size = BinaryData::index_htmlSize; mime = "text/html"; }
    else if (requested == "editor.css") { data = BinaryData::editor_css; size = BinaryData::editor_cssSize; mime = "text/css"; }
    else if (requested == "editor.js") { data = BinaryData::editor_js; size = BinaryData::editor_jsSize; mime = "text/javascript"; }
    else return std::nullopt;

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    std::memcpy(bytes.data(), data, static_cast<std::size_t>(size));
    return juce::WebBrowserComponent::Resource { std::move(bytes), std::move(mime) };
}

void EditorShell::resized()
{
    auto bounds = getLocalBounds();
    auto controls = bounds.removeFromTop(88).reduced(14, 10);
    auto topRow = controls.removeFromTop(31);
    status_.setBounds(topRow.removeFromLeft(300));
    deviceButton_.setBounds(topRow.removeFromLeft(145));
    topRow.removeFromLeft(8);
    impulseButton_.setBounds(topRow.removeFromLeft(145));
    topRow.removeFromLeft(8);
    muteButton_.setBounds(topRow.removeFromLeft(145));
    topRow.removeFromLeft(8);
    resetButton_.setBounds(topRow.removeFromLeft(125));
    auto gainRow = controls.removeFromTop(34);
    gainLabel_.setBounds(gainRow.removeFromLeft(190));
    gain_.setBounds(gainRow.removeFromLeft(340));
    if (browser_ != nullptr) {
#if JUCE_WINDOWS
        if (const auto* display = juce::Desktop::getInstance().getDisplays()
                .getDisplayForPoint(localPointToGlobal(bounds.getCentre()).toFloat())) {
            // WebView2 applies the monitor scale to its child HWND after JUCE has supplied
            // logical bounds. Compensate once so the child is not clipped by its parent.
            const auto scale = static_cast<float>(display->scale);
            browser_->setBounds(bounds.withSizeKeepingCentre(
                juce::roundToInt(static_cast<float>(bounds.getWidth()) / scale),
                juce::roundToInt(static_cast<float>(bounds.getHeight()) / scale))
                .withPosition(bounds.getPosition()));
            return;
        }
#endif
        browser_->setBounds(bounds);
    }
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
