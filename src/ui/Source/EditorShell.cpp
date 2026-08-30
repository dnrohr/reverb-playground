#include <reverb/ui/EditorShell.h>
#include <reverb/ui/AuditionDeckLayout.h>
#include <reverb/ui/AuditionWaveformStyle.h>
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
    wetGainLabel_.setText("WET", juce::dontSendNotification);
    dryGainLabel_.setText("DRY", juce::dontSendNotification);
    styleLabel(wetGainLabel_, mutedText, 12.0F, juce::Font::bold);
    styleLabel(dryGainLabel_, mutedText, 12.0F, juce::Font::bold);

    const std::array<juce::Component*, 8> components {
        &status_, &wetGainLabel_, &dryGainLabel_,
        &impulseButton_, &muteButton_, &resetButton_, &wetGain_, &dryGain_,
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

    for (auto* slider : { &wetGain_, &dryGain_ }) {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 28);
        slider->setRange(0.0, 1.0, 0.001);
        slider->setColour(juce::Slider::trackColourId, cyan);
        slider->setColour(juce::Slider::thumbColourId, text);
        slider->setColour(juce::Slider::textBoxTextColourId, text);
        slider->setColour(juce::Slider::textBoxBackgroundColourId, panel);
    }
    wetGain_.setTitle("Wet gain");
    wetGain_.setValue(callbacks_.wetGain(), juce::dontSendNotification);
    wetGain_.setDoubleClickReturnValue(true, 0.5);
    wetGain_.onValueChange = [this] { callbacks_.setWetGain(static_cast<float>(wetGain_.getValue())); };
    dryGain_.setTitle("Dry gain");
    dryGain_.setValue(callbacks_.dryGain(), juce::dontSendNotification);
    dryGain_.setDoubleClickReturnValue(true, 0.0);
    dryGain_.onValueChange = [this] { callbacks_.setDryGain(static_cast<float>(dryGain_.getValue())); };
    muteButton_.setToggleState(callbacks_.emergencyMuted(), juce::dontSendNotification);
    resetButton_.setVisible(false);

    const std::array<juce::Component*, 13> auditionComponents {
        &sourceMode_, &fileButton_, &filePlayButton_, &fileStopButton_, &drawerButton_,
        &loopButton_, &exportRange_, &fileLabel_, &transportLabel_, &seek_,
        &exportButton_, &exportProgressBar_, &loopRange_,
    };
    for (auto* component : auditionComponents) {
        addAndMakeVisible(component);
        component->setVisible(callbacks_.standaloneAuditionAvailable);
    }
    fileLabel_.setText("DROP WAV, AIFF, OR FLAC HERE", juce::dontSendNotification);
    fileLabel_.setJustificationType(juce::Justification::centredLeft);
    transportLabel_.setJustificationType(juce::Justification::centredRight);
    styleLabel(fileLabel_, text, 12.0F, juce::Font::bold);
    styleLabel(transportLabel_, mutedText, 11.0F, juce::Font::plain);
    for (auto* button : { &fileButton_, &filePlayButton_, &fileStopButton_, &drawerButton_ }) {
        button->setColour(juce::TextButton::buttonColourId, panel.brighter(0.08F));
        button->setColour(juce::TextButton::buttonOnColourId, cyan.darker(0.35F));
    }
    sourceMode_.addItem("LIVE INPUT", 1);
    sourceMode_.addItem("AUDIO FILE", 2);
    sourceMode_.addItem("TEST IMPULSE", 3);
    sourceMode_.setSelectedId(1, juce::dontSendNotification);
    sourceMode_.setTitle("Audition source");
    sourceMode_.onChange = [this] {
        if (updatingTransportControls_) return;
        const auto mode = juce::jlimit(0, 2, sourceMode_.getSelectedId() - 1);
        callbacks_.setAuditionSourceMode(mode);
        if (mode == 2) callbacks_.triggerImpulse();
    };
    fileButton_.onClick = [this] { chooseAudioFile(); };
    filePlayButton_.onClick = [this] {
        if (filePlaying_) callbacks_.pauseAudioFile(); else callbacks_.playAudioFile();
    };
    fileStopButton_.onClick = [this] { callbacks_.stopAudioFile(); };
    drawerButton_.setTitle("Audio-file details");
    drawerButton_.setDescription("Show or hide waveform, looping, comparison, and export details");
    drawerButton_.onClick = [this] { setAuditionDrawerExpanded(!auditionDrawerExpanded_); };
    loopButton_.onClick = [this] {
        if (updatingTransportControls_ || fileFrameCount_ <= 0) return;
        const auto start = static_cast<std::int64_t>(std::llround(loopRange_.getMinValue() * fileFrameCount_));
        const auto end = static_cast<std::int64_t>(std::llround(loopRange_.getMaxValue() * fileFrameCount_));
        const auto error = callbacks_.setAudioFileLoop(loopButton_.getToggleState(), start, end);
        if (error.isNotEmpty()) status_.setText(error, juce::dontSendNotification);
    };
    exportRange_.addItem("ENTIRE FILE", 1);
    exportRange_.addItem("SELECTED LOOP", 2);
    exportRange_.setSelectedId(1, juce::dontSendNotification);
    exportRange_.setTitle("Export source range");
    exportButton_.setColour(juce::TextButton::buttonColourId, amber.darker(0.55F));
    exportButton_.onClick = [this] {
        const auto status = juce::JSON::parse(callbacks_.processedFileExportJson());
        if (status.getProperty("state", "").toString() == "rendering")
            callbacks_.cancelProcessedFileExport();
        else
            chooseExportFile();
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

    setAuditionDrawerExpanded(false);

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
            if (arguments.size() != 2) { complete(juce::var()); return; }
            complete(callbacks_.startImpulseCapture(
                static_cast<double>(arguments[0]),
                static_cast<double>(arguments[1])));
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
        .withNativeFunction("chooseAudioDevice", [this](const auto&, auto complete) {
            callbacks_.chooseAudioDevice();
            complete(true);
        })
        .withNativeFunction("standaloneAuditionAvailable", [this](const auto&, auto complete) {
            complete(callbacks_.standaloneAuditionAvailable);
        })
        .withNativeFunction("setAuditionDrawerExpanded", [this](const auto& arguments, auto complete) {
            const auto expanded = arguments.size() > 0 && static_cast<bool>(arguments[0]);
            setAuditionDrawerExpanded(expanded);
            complete(auditionDrawerExpanded_);
        })
        .withNativeFunction("auditionDrawerExpanded", [this](const auto&, auto complete) {
            complete(auditionDrawerExpanded_);
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
    const auto headerHeight = callbacks_.standaloneAuditionAvailable
        ? static_cast<float>(calculateAuditionDeckLayout(getWidth(), auditionDrawerExpanded_).headerHeight)
        : static_cast<float>(globalControlHeightForWidth(getWidth()) + 10);
    auto controls = getLocalBounds().toFloat().removeFromTop(headerHeight);
    graphics.setColour(panel);
    graphics.fillRect(controls);
    graphics.setColour(border);
    graphics.drawLine(0.0F, controls.getBottom() - 1.0F, controls.getRight(), controls.getBottom() - 1.0F);
    if (callbacks_.standaloneAuditionAvailable && !waveformBounds_.isEmpty()) {
        graphics.setColour(background.brighter(0.08F));
        graphics.fillRoundedRectangle(waveformBounds_.toFloat(), 3.0F);
        if (thumbnail_.getTotalLength() > 0.0) {
            const auto colours = auditionWaveformColours(loopButton_.getToggleState());
            auto selectedArea = waveformBounds_.toFloat();
            selectedArea.setLeft(selectedArea.getX() + selectedArea.getWidth()
                * static_cast<float>(loopRange_.getMinValue()));
            selectedArea.setRight(waveformBounds_.getX() + waveformBounds_.getWidth()
                * static_cast<float>(loopRange_.getMaxValue()));

            graphics.setColour(juce::Colour(colours.selectionFill));
            graphics.fillRect(selectedArea);
            graphics.setColour(juce::Colour(colours.unselected));
            thumbnail_.drawChannels(graphics, waveformBounds_.reduced(2),
                0.0, thumbnail_.getTotalLength(), 0.9F);

            {
                const juce::Graphics::ScopedSaveState saveState(graphics);
                graphics.reduceClipRegion(selectedArea.toNearestInt());
                graphics.setColour(juce::Colour(colours.selected));
                thumbnail_.drawChannels(graphics, waveformBounds_.reduced(2),
                    0.0, thumbnail_.getTotalLength(), 0.9F);
            }
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
    const auto deck = calculateAuditionDeckLayout(getWidth(), auditionDrawerExpanded_);
    const auto headerHeight = callbacks_.standaloneAuditionAvailable
        ? deck.headerHeight : globalControlHeightForWidth(getWidth()) + 10;
    constexpr int inset = 14;
    const auto contentWidth = juce::jmax(0, getWidth() - inset * 2);
    if (getWidth() < 900) {
        status_.setBounds(inset, 6, contentWidth, 23);
        auto gainRow = juce::Rectangle<int> { inset, 31, contentWidth, 25 };
        const auto half = gainRow.getWidth() / 2;
        auto wet = gainRow.removeFromLeft(half).reduced(0, 0); wet.removeFromRight(5);
        auto dry = gainRow; dry.removeFromLeft(5);
        wetGainLabel_.setBounds(wet.removeFromLeft(34)); wetGain_.setBounds(wet);
        dryGainLabel_.setBounds(dry.removeFromLeft(34)); dryGain_.setBounds(dry);
        auto actions = juce::Rectangle<int> { inset, 59, contentWidth, 28 };
        constexpr int actionGap = 6;
        const auto actionCount = resetButton_.isVisible() ? 3 : 2;
        const auto actionWidth = juce::jmax(0, (actions.getWidth() - actionGap * (actionCount - 1)) / actionCount);
        impulseButton_.setBounds(actions.removeFromLeft(actionWidth)); actions.removeFromLeft(actionGap);
        muteButton_.setBounds(actions.removeFromLeft(actionWidth));
        if (resetButton_.isVisible()) { actions.removeFromLeft(actionGap); resetButton_.setBounds(actions); }
    } else {
        auto row = juce::Rectangle<int> { inset, 6, contentWidth, 29 };
        constexpr int gap = 7;
        if (resetButton_.isVisible()) { resetButton_.setBounds(row.removeFromRight(112)); row.removeFromRight(gap); }
        muteButton_.setBounds(row.removeFromRight(132)); row.removeFromRight(gap);
        impulseButton_.setBounds(row.removeFromRight(122)); row.removeFromRight(gap);
        auto dry = row.removeFromRight(205); row.removeFromRight(gap);
        dryGainLabel_.setBounds(dry.removeFromLeft(34)); dryGain_.setBounds(dry);
        auto wet = row.removeFromRight(205); row.removeFromRight(gap);
        wetGainLabel_.setBounds(wet.removeFromLeft(34)); wetGain_.setBounds(wet);
        status_.setBounds(row);
    }
    if (callbacks_.standaloneAuditionAvailable) {
        const auto convert = [](const LayoutRect& item) {
            return juce::Rectangle<int> { item.x, item.y, item.width, item.height };
        };
        sourceMode_.setBounds(convert(deck.sourceMode));
        fileButton_.setBounds(convert(deck.loadFile));
        filePlayButton_.setBounds(convert(deck.playPause));
        auto summary = convert(deck.summary);
        fileLabel_.setBounds(summary.removeFromLeft(static_cast<int>(summary.getWidth() * 0.48)));
        transportLabel_.setBounds(summary);
        exportButton_.setBounds(convert(deck.exportWav));
        drawerButton_.setBounds(convert(deck.drawerToggle));
        waveformBounds_ = convert(deck.waveform);
        fileStopButton_.setBounds(convert(deck.stop));
        loopButton_.setBounds(convert(deck.loop));
        exportRange_.setBounds(convert(deck.exportMode));
        exportProgressBar_.setBounds(convert(deck.exportProgress));
        seek_.setBounds(convert(deck.seek));
        loopRange_.setBounds(convert(deck.loopRange));
    } else {
        waveformBounds_ = {};
    }
    bounds.removeFromTop(headerHeight);
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

void EditorShell::chooseExportFile()
{
    if (fileFrameCount_ <= 0) return;
    exportChooser_ = std::make_unique<juce::FileChooser>(
        "Export processed audio", juce::File {}, "*.wav");
    exportChooser_->launchAsync(juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& chooser) {
            auto destination = chooser.getResult();
            if (destination != juce::File {}) {
                if (destination.getFileExtension().toLowerCase() != ".wav")
                    destination = destination.withFileExtension(".wav");
                const auto overwriteConfirmed = destination.existsAsFile();
                const auto error = callbacks_.startProcessedFileExport(
                    destination, exportRange_.getSelectedId() - 1, overwriteConfirmed);
                if (error.isNotEmpty()) status_.setText(error, juce::dontSendNotification);
            }
            exportChooser_.reset();
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
    setAuditionDrawerExpanded(true);
    repaint();
}

void EditorShell::setAuditionDrawerExpanded(const bool expanded)
{
    auditionDrawerExpanded_ = callbacks_.standaloneAuditionAvailable && expanded;
    drawerButton_.setButtonText(auditionDrawerExpanded_ ? juce::String::fromUTF8("\xe2\x88\x92") : "+");
    drawerButton_.setTooltip(auditionDrawerExpanded_ ? "Hide audio-file details" : "Show audio-file details");
    const std::array<juce::Component*, 6> drawerComponents {
        &fileStopButton_, &loopButton_, &exportRange_,
        &exportProgressBar_, &seek_, &loopRange_,
    };
    for (auto* component : drawerComponents)
        component->setVisible(auditionDrawerExpanded_);
    waveformBounds_ = {};
    resized();
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
    updatingTransportControls_ = true;
    sourceMode_.setItemEnabled(2, fileFrameCount_ > 0);
    sourceMode_.setSelectedId(sourceMode == "audio-file" ? 2 : sourceMode == "test-impulse" ? 3 : 1,
        juce::dontSendNotification);
    const auto cursorProportion = fileFrameCount_ > 0
        ? static_cast<double>(fileCursorFrame_) / static_cast<double>(fileFrameCount_) : 0.0;
    if (!seek_.isMouseButtonDown()) seek_.setValue(cursorProportion, juce::dontSendNotification);
    if (const auto* loop = object->getProperty("loop").getDynamicObject()) {
        const auto enabled = static_cast<bool>(loop->getProperty("enabled"));
        loopButton_.setToggleState(enabled, juce::dontSendNotification);
        exportRange_.setItemEnabled(2, enabled);
        if (!enabled && exportRange_.getSelectedId() == 2)
            exportRange_.setSelectedId(1, juce::dontSendNotification);
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
    const auto exportStatus = juce::JSON::parse(callbacks_.processedFileExportJson());
    const auto exportState = exportStatus.getProperty("state", "idle").toString();
    exportProgress_ = static_cast<double>(exportStatus.getProperty("progress", 0.0));
    exportButton_.setButtonText(exportState == "rendering" ? "CANCEL EXPORT" : "EXPORT WAV...");
    exportButton_.setEnabled(fileFrameCount_ > 0 || exportState == "rendering");
    exportRange_.setEnabled(exportState != "rendering");
    if (exportState == "failed")
        status_.setText(exportStatus.getProperty("error", "Export failed").toString(), juce::dontSendNotification);
    else if (exportState == "complete")
        exportProgressBar_.setTextToDisplay("EXPORTED "
            + exportStatus.getProperty("destinationName", "").toString());
    else if (exportState == "cancelled")
        exportProgressBar_.setTextToDisplay("EXPORT CANCELLED");
    else
        exportProgressBar_.setTextToDisplay(exportState == "rendering"
            ? "EXPORT " + juce::String(exportProgress_ * 100.0, 0) + "%" : "EXPORT READY");
    repaint(waveformBounds_);
}

void EditorShell::timerCallback()
{
    if (!wetGain_.isMouseButtonDown())
        wetGain_.setValue(callbacks_.wetGain(), juce::dontSendNotification);
    if (!dryGain_.isMouseButtonDown())
        dryGain_.setValue(callbacks_.dryGain(), juce::dontSendNotification);
    muteButton_.setToggleState(callbacks_.emergencyMuted(), juce::dontSendNotification);
    const auto safetyLatched = callbacks_.isSafetyLatched();
    if (resetButton_.isVisible() != safetyLatched) {
        resetButton_.setVisible(safetyLatched);
        resized();
    }
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
