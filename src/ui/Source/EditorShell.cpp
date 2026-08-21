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
    formatManager_.registerBasicFormats();
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
    gain_.setTitle("Master audition gain");
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

    const std::array<juce::Component*, 11> auditionComponents {
        &liveSourceButton_, &fileSourceButton_, &fileButton_, &filePlayButton_, &fileStopButton_,
        &impulseSourceButton_, &loopButton_, &processedButton_, &fileLabel_, &transportLabel_, &seek_,
    };
    for (auto* component : auditionComponents) {
        addAndMakeVisible(component);
        component->setVisible(callbacks_.standaloneAuditionAvailable);
    }
    addAndMakeVisible(loopRange_);
    loopRange_.setVisible(callbacks_.standaloneAuditionAvailable);
    fileLabel_.setText("DROP WAV, AIFF, OR FLAC HERE", juce::dontSendNotification);
    fileLabel_.setJustificationType(juce::Justification::centredLeft);
    transportLabel_.setJustificationType(juce::Justification::centredRight);
    styleLabel(fileLabel_, text, 12.0F, juce::Font::bold);
    styleLabel(transportLabel_, mutedText, 11.0F, juce::Font::plain);
    for (auto* button : { &liveSourceButton_, &fileSourceButton_, &fileButton_, &filePlayButton_,
             &fileStopButton_, &impulseSourceButton_ }) {
        button->setColour(juce::TextButton::buttonColourId, panel.brighter(0.08F));
        button->setColour(juce::TextButton::buttonOnColourId, cyan.darker(0.35F));
    }
    liveSourceButton_.setClickingTogglesState(false);
    fileSourceButton_.setClickingTogglesState(false);
    liveSourceButton_.onClick = [this] { callbacks_.setAuditionSourceMode(0); };
    fileSourceButton_.onClick = [this] { callbacks_.setAuditionSourceMode(1); };
    impulseSourceButton_.onClick = [this] {
        callbacks_.setAuditionSourceMode(2);
        callbacks_.triggerImpulse();
    };
    fileButton_.onClick = [this] { chooseAudioFile(); };
    filePlayButton_.onClick = [this] {
        if (filePlaying_) callbacks_.pauseAudioFile(); else callbacks_.playAudioFile();
    };
    fileStopButton_.onClick = [this] { callbacks_.stopAudioFile(); };
    loopButton_.onClick = [this] {
        if (updatingTransportControls_ || fileFrameCount_ <= 0) return;
        const auto start = static_cast<std::int64_t>(std::llround(loopRange_.getMinValue() * fileFrameCount_));
        const auto end = static_cast<std::int64_t>(std::llround(loopRange_.getMaxValue() * fileFrameCount_));
        const auto error = callbacks_.setAudioFileLoop(loopButton_.getToggleState(), start, end);
        if (error.isNotEmpty()) status_.setText(error, juce::dontSendNotification);
    };
    processedButton_.setClickingTogglesState(true);
    processedButton_.setToggleState(callbacks_.isProcessedAudition(), juce::dontSendNotification);
    processedButton_.onClick = [this] {
        callbacks_.setProcessedAudition(processedButton_.getToggleState());
        processedButton_.setButtonText(processedButton_.getToggleState() ? "PROCESSED" : "DRY BYPASS");
    };
    seek_.setSliderStyle(juce::Slider::LinearHorizontal);
    seek_.setTitle("Audio-file position");
    seek_.setDescription("Seek through the loaded audio file");
    seek_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    seek_.setRange(0.0, 1.0, 0.0);
    seek_.onDragEnd = [this] {
        if (fileFrameCount_ <= 0) return;
        const auto target = static_cast<std::int64_t>(std::llround(seek_.getValue() * fileFrameCount_));
        const auto error = callbacks_.seekAudioFile(target);
        if (error.isNotEmpty()) status_.setText(error, juce::dontSendNotification);
    };
    loopRange_.setSliderStyle(juce::Slider::TwoValueHorizontal);
    loopRange_.setTitle("Audio-file loop range");
    loopRange_.setDescription("Set the loop start and end points");
    loopRange_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    loopRange_.setRange(0.0, 1.0, 0.0);
    loopRange_.setMinAndMaxValues(0.0, 1.0, juce::dontSendNotification);
    loopRange_.onDragEnd = [this] {
        if (!loopButton_.getToggleState() || fileFrameCount_ <= 0) return;
        const auto start = static_cast<std::int64_t>(std::llround(loopRange_.getMinValue() * fileFrameCount_));
        const auto end = static_cast<std::int64_t>(std::llround(loopRange_.getMaxValue() * fileFrameCount_));
        const auto error = callbacks_.setAudioFileLoop(true, start, end);
        if (error.isNotEmpty()) status_.setText(error, juce::dontSendNotification);
    };

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
        .withNativeFunction("startImpulseCapture", [this](const auto& arguments, auto complete) {
            if (arguments.size() != 3) { complete(juce::var()); return; }
            complete(callbacks_.startImpulseCapture(
                static_cast<double>(arguments[0]),
                static_cast<double>(arguments[1]),
                static_cast<bool>(arguments[2])));
        })
        .withNativeFunction("getImpulseCaptureStatus", [this](const auto&, auto complete) {
            complete(callbacks_.impulseCaptureStatusJson());
        })
        .withNativeFunction("getImpulseCapture", [this](const auto&, auto complete) {
            complete(callbacks_.impulseCaptureJson());
        })
        .withNativeFunction("setEnergyTelemetryEnabled", [this](const auto& arguments, auto complete) {
            if (arguments.size() != 1) { complete(false); return; }
            complete(callbacks_.setEnergyTelemetryEnabled(static_cast<bool>(arguments[0])));
        })
        .withNativeFunction("getEnergyTelemetry", [this](const auto&, auto complete) {
            complete(callbacks_.energyTelemetryJson());
        })
        .withNativeFunction("getRuntimeDiagnostics", [this](const auto&, auto complete) {
            complete(callbacks_.runtimeDiagnosticsJson());
        })
        .withNativeFunction("publishGraph", [this](const auto& arguments, auto complete) {
            if (arguments.size() != 1) {
                complete(juce::String(R"({"accepted":false,"revision":0,"error":"expected one patch JSON argument"})"));
                return;
            }
            complete(callbacks_.publishGraphJson(arguments[0].toString()));
        })
        .withNativeFunction("storePatchState", [this](const auto& arguments, auto complete) {
            if (arguments.size() != 1) {
                complete(juce::String(R"({"accepted":false,"error":"expected one patch JSON argument"})"));
                return;
            }
            complete(callbacks_.storePatchStateJson(arguments[0].toString()));
        })
        .withNativeFunction("resetSafety", [this](const auto&, auto complete) {
            callbacks_.resetSafety();
            complete(true);
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
    const auto headerHeight = callbacks_.standaloneAuditionAvailable ? 210.0F : 88.0F;
    auto controls = getLocalBounds().toFloat().removeFromTop(headerHeight);
    graphics.setColour(panel);
    graphics.fillRect(controls);
    graphics.setColour(border);
    graphics.drawLine(0.0F, controls.getBottom() - 1.0F, controls.getRight(), controls.getBottom() - 1.0F);
    if (callbacks_.standaloneAuditionAvailable && !waveformBounds_.isEmpty()) {
        graphics.setColour(background.brighter(0.08F));
        graphics.fillRoundedRectangle(waveformBounds_.toFloat(), 3.0F);
        if (thumbnail_.getTotalLength() > 0.0) {
            if (loopButton_.getToggleState()) {
                auto loopArea = waveformBounds_.toFloat();
                loopArea.setLeft(loopArea.getX() + loopArea.getWidth() * static_cast<float>(loopRange_.getMinValue()));
                loopArea.setRight(waveformBounds_.getX() + waveformBounds_.getWidth()
                    * static_cast<float>(loopRange_.getMaxValue()));
                graphics.setColour(cyan.withAlpha(0.12F));
                graphics.fillRect(loopArea);
            }
            graphics.setColour(cyan.withAlpha(0.8F));
            thumbnail_.drawChannels(graphics, waveformBounds_.reduced(2), 0.0, thumbnail_.getTotalLength(), 0.9F);
            const auto cursorX = waveformBounds_.getX()
                + static_cast<int>(std::round(waveformBounds_.getWidth() * seek_.getValue()));
            graphics.setColour(amber);
            graphics.drawVerticalLine(cursorX, static_cast<float>(waveformBounds_.getY()),
                static_cast<float>(waveformBounds_.getBottom()));
        }
        graphics.setColour(border);
        graphics.drawRoundedRectangle(waveformBounds_.toFloat(), 3.0F, 1.0F);
    }
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
    const auto headerHeight = callbacks_.standaloneAuditionAvailable ? 210 : 88;
    auto controls = bounds.removeFromTop(headerHeight).reduced(14, 10);
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
    if (callbacks_.standaloneAuditionAvailable) {
        auto sourceRow = controls.removeFromTop(30);
        liveSourceButton_.setBounds(sourceRow.removeFromLeft(105));
        fileSourceButton_.setBounds(sourceRow.removeFromLeft(105));
        impulseSourceButton_.setBounds(sourceRow.removeFromLeft(115));
        sourceRow.removeFromLeft(8);
        fileButton_.setBounds(sourceRow.removeFromLeft(110));
        sourceRow.removeFromLeft(8);
        filePlayButton_.setBounds(sourceRow.removeFromLeft(70));
        fileStopButton_.setBounds(sourceRow.removeFromLeft(65));
        loopButton_.setBounds(sourceRow.removeFromLeft(65));
        processedButton_.setBounds(sourceRow.removeFromLeft(110));
        auto waveformRow = controls.removeFromTop(46);
        waveformBounds_ = waveformRow.removeFromLeft(
            std::max(240, static_cast<int>(std::round(waveformRow.getWidth() * 0.68))));
        auto details = waveformRow.reduced(8, 0);
        fileLabel_.setBounds(details.removeFromTop(22));
        transportLabel_.setBounds(details);
        auto transportRow = controls.removeFromTop(20);
        seek_.setBounds(transportRow.removeFromLeft(
            std::max(240, static_cast<int>(std::round(transportRow.getWidth() * 0.68)))));
        loopRange_.setBounds(transportRow.reduced(8, 0));
    } else {
        waveformBounds_ = {};
    }
    if (browser_ != nullptr)
        browser_->setBounds(bounds);
}

void EditorShell::chooseAudioFile()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Choose audio to audition", juce::File {}, "*.wav;*.aif;*.aiff;*.flac");
    fileChooser_->launchAsync(juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser) {
            const auto file = chooser.getResult();
            if (file.existsAsFile()) loadAudioFile(file);
            fileChooser_.reset();
        });
}

void EditorShell::loadAudioFile(const juce::File& file)
{
    const auto error = callbacks_.loadAudioFile(file);
    if (error.isNotEmpty()) {
        status_.setText(error, juce::dontSendNotification);
        return;
    }
    thumbnail_.setSource(new juce::FileInputSource(file));
    fileLabel_.setText(file.getFileName(), juce::dontSendNotification);
    callbacks_.playAudioFile();
    repaint();
}

void EditorShell::seekFromWaveform(const float x)
{
    if (fileFrameCount_ <= 0 || waveformBounds_.isEmpty()) return;
    const auto proportion = juce::jlimit(0.0, 1.0,
        static_cast<double>(x - static_cast<float>(waveformBounds_.getX()))
            / static_cast<double>(waveformBounds_.getWidth()));
    seek_.setValue(proportion, juce::dontSendNotification);
    const auto error = callbacks_.seekAudioFile(
        static_cast<std::int64_t>(std::llround(proportion * fileFrameCount_)));
    if (error.isNotEmpty()) status_.setText(error, juce::dontSendNotification);
}

void EditorShell::mouseDown(const juce::MouseEvent& event)
{
    if (waveformBounds_.contains(event.getPosition())) seekFromWaveform(event.position.x);
}

void EditorShell::mouseDrag(const juce::MouseEvent& event)
{
    if (callbacks_.standaloneAuditionAvailable) seekFromWaveform(event.position.x);
}

bool EditorShell::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (!callbacks_.standaloneAuditionAvailable || files.size() != 1) return false;
    const auto extension = juce::File(files[0]).getFileExtension().toLowerCase();
    return extension == ".wav" || extension == ".aif" || extension == ".aiff" || extension == ".flac";
}

void EditorShell::filesDropped(const juce::StringArray& files, int, int)
{
    if (isInterestedInFileDrag(files)) loadAudioFile(juce::File(files[0]));
}

void EditorShell::updateTransport()
{
    if (!callbacks_.standaloneAuditionAvailable) return;
    const auto parsed = juce::JSON::parse(callbacks_.audioFileTransportJson());
    const auto* object = parsed.getDynamicObject();
    if (object == nullptr) return;
    const auto state = object->getProperty("state").toString();
    const auto sourceMode = object->getProperty("sourceMode").toString();
    fileFrameCount_ = static_cast<std::int64_t>(object->getProperty("frameCount"));
    fileCursorFrame_ = static_cast<std::int64_t>(object->getProperty("cursorSourceFrame"));
    fileSampleRate_ = static_cast<double>(object->getProperty("sourceSampleRate"));
    filePlaying_ = state == "playing";
    filePlayButton_.setButtonText(filePlaying_ ? "PAUSE" : "PLAY");
    filePlayButton_.setEnabled(fileFrameCount_ > 0);
    fileStopButton_.setEnabled(fileFrameCount_ > 0);
    fileSourceButton_.setEnabled(fileFrameCount_ > 0);
    liveSourceButton_.setToggleState(sourceMode == "live-input", juce::dontSendNotification);
    fileSourceButton_.setToggleState(sourceMode == "audio-file", juce::dontSendNotification);
    impulseSourceButton_.setToggleState(sourceMode == "test-impulse", juce::dontSendNotification);
    updatingTransportControls_ = true;
    const auto cursorProportion = fileFrameCount_ > 0
        ? static_cast<double>(fileCursorFrame_) / static_cast<double>(fileFrameCount_) : 0.0;
    if (!seek_.isMouseButtonDown()) seek_.setValue(cursorProportion, juce::dontSendNotification);
    if (const auto* loop = object->getProperty("loop").getDynamicObject()) {
        const auto enabled = static_cast<bool>(loop->getProperty("enabled"));
        loopButton_.setToggleState(enabled, juce::dontSendNotification);
        if (!loopRange_.isMouseButtonDown() && fileFrameCount_ > 0) {
            loopRange_.setMinAndMaxValues(
                static_cast<double>(static_cast<std::int64_t>(loop->getProperty("startSourceFrame"))) / fileFrameCount_,
                static_cast<double>(static_cast<std::int64_t>(loop->getProperty("endSourceFrame"))) / fileFrameCount_,
                juce::dontSendNotification);
        }
    }
    updatingTransportControls_ = false;
    const auto cursorSeconds = fileSampleRate_ > 0.0 ? fileCursorFrame_ / fileSampleRate_ : 0.0;
    const auto durationSeconds = fileSampleRate_ > 0.0 ? fileFrameCount_ / fileSampleRate_ : 0.0;
    const auto underruns = static_cast<std::int64_t>(object->getProperty("underrunEvents"));
    transportLabel_.setText(
        juce::String(cursorSeconds, 2) + " / " + juce::String(durationSeconds, 2) + " s  |  "
            + state.toUpperCase() + "  |  UNDERRUNS " + juce::String(underruns),
        juce::dontSendNotification);
    processedButton_.setToggleState(callbacks_.isProcessedAudition(), juce::dontSendNotification);
    processedButton_.setButtonText(processedButton_.getToggleState() ? "PROCESSED" : "DRY BYPASS");
    repaint(waveformBounds_);
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
    updateTransport();
    status_.setColour(
        juce::Label::textColourId,
        safetyLatched || muteButton_.getToggleState() ? danger : text);
}

} // namespace reverb::ui
