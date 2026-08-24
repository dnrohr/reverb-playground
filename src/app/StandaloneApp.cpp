#include "StartupProgress.h"

#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#if JucePlugin_Build_Standalone

namespace {

class StartupWindow final : public juce::DocumentWindow,
                            private juce::Timer {
public:
    explicit StartupWindow(reverb::app::StartupProgress& progress)
        : DocumentWindow(
              juce::String::fromUTF8(JucePlugin_Name),
              juce::Colour(0xff090d10),
              DocumentWindow::minimiseButton | DocumentWindow::closeButton)
        , progress_(progress)
    {
        setUsingNativeTitleBar(true);
        setResizable(false, false);
        setContentOwned(new Content(progress_), false);
        centreWithSize(680, 250);
        startTimerHz(10);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplicationBase::quit();
    }

private:
    class Content final : public juce::Component {
    public:
        explicit Content(reverb::app::StartupProgress& progress)
            : progress_(progress)
            , startedAtMilliseconds_(juce::Time::getMillisecondCounterHiRes())
        {
            setAccessible(true);
            setTitle("Reverb Playground startup status");
            setDescription("The editor is opening while Windows audio devices connect.");
        }

        void paint(juce::Graphics& graphics) override
        {
            graphics.fillAll(juce::Colour(0xff090d10));

            auto area = getLocalBounds().reduced(42);
            graphics.setColour(juce::Colour(0xfff2b44e));
            graphics.setFont(juce::FontOptions(14.0f, juce::Font::bold));
            graphics.drawText("REVERB PLAYGROUND", area.removeFromTop(24), juce::Justification::left);

            area.removeFromTop(24);
            graphics.setColour(juce::Colour(0xffedf1f2));
            graphics.setFont(juce::FontOptions(26.0f, juce::Font::bold));
            graphics.drawText("Opening the schematic editor", area.removeFromTop(38), juce::Justification::left);

            area.removeFromTop(12);
            graphics.setColour(juce::Colour(0xffaab6bc));
            graphics.setFont(juce::FontOptions(15.0f));
            const auto presentation = currentPresentation();
            graphics.drawFittedText(statusText(presentation),
                area.removeFromTop(48), juce::Justification::topLeft, 2);

            area.removeFromTop(16);
            auto track = area.removeFromTop(3).withWidth(420);
            graphics.setColour(juce::Colour(0xff263239));
            graphics.fillRect(track);
            graphics.setColour(juce::Colour(0xff55c7b0));
            graphics.fillRect(track.withWidth(static_cast<int>(
                std::round(track.getWidth() * presentation.progress))));
        }

    private:
        [[nodiscard]] juce::String statusText(
            const reverb::app::StartupPresentation presentation) const
        {
            if (progress_.phase() == reverb::app::StartupPhase::failed) {
                return "Audio could not start. Close and reopen to try again.";
            }
            return presentation.welcomed ? "Welcome!" : "Loading...";
        }

        [[nodiscard]] reverb::app::StartupPresentation currentPresentation() const noexcept
        {
            return reverb::app::startupPresentation(
                (juce::Time::getMillisecondCounterHiRes() - startedAtMilliseconds_) / 1000.0);
        }

        reverb::app::StartupProgress& progress_;
        double startedAtMilliseconds_ {};
    };

    void timerCallback() override
    {
        if (auto* content = getContentComponent()) {
            content->repaint();
        }
    }

    reverb::app::StartupProgress& progress_;
};

class ReverbStandaloneApp final : public juce::JUCEApplication {
public:
    ReverbStandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = juce::CharPointer_UTF8(JucePlugin_Name);
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName = "";
        appProperties_.setStorageParameters(options);
    }

    ~ReverbStandaloneApp() override
    {
        finishStartupWorker();
    }

    const juce::String getApplicationName() override { return juce::CharPointer_UTF8(JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted(const juce::String&) override {}

    void initialise(const juce::String&) override
    {
        startupWindow_ = std::make_unique<StartupWindow>(progress_);
        startupWindow_->setVisible(true);
        progress_.advanceTo(reverb::app::StartupPhase::connectingAudio);

        startupWorker_ = std::thread([this] {
            try {
                auto holder = createPluginHolder();
                {
                    const std::scoped_lock lock(resultMutex_);
                    completedHolder_ = std::move(holder);
                }
            } catch (...) {
                progress_.advanceTo(reverb::app::StartupPhase::failed);
            }

            juce::MessageManager::callAsync([this] {
                if (juce::JUCEApplicationBase::getInstance() == this) {
                    finishOpeningEditor();
                }
            });
        });
    }

    void shutdown() override
    {
        shuttingDown_.store(true, std::memory_order_release);
        finishStartupWorker();
        if (mainWindow_ != nullptr) {
            mainWindow_->pluginHolder->savePluginState();
        }
        mainWindow_ = nullptr;
        startupWindow_ = nullptr;
        appProperties_.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents()) {
            juce::Timer::callAfterDelay(100, [] {
                if (auto* app = juce::JUCEApplicationBase::getInstance()) {
                    app->systemRequestedQuit();
                }
            });
            return;
        }
        quit();
    }

private:
    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
        return std::make_unique<juce::StandalonePluginHolder>(
            appProperties_.getUserSettings(), false, juce::String {}, nullptr,
            juce::Array<juce::StandalonePluginHolder::PluginInOuts> {}, false);
    }

    void finishOpeningEditor()
    {
        if (shuttingDown_.load(std::memory_order_acquire)) {
            return;
        }

        std::unique_ptr<juce::StandalonePluginHolder> holder;
        {
            const std::scoped_lock lock(resultMutex_);
            holder = std::move(completedHolder_);
        }
        if (holder == nullptr) {
            progress_.advanceTo(reverb::app::StartupPhase::failed);
            return;
        }

        progress_.advanceTo(reverb::app::StartupPhase::openingEditor);
        mainWindow_ = std::make_unique<juce::StandaloneFilterWindow>(
            getApplicationName(), juce::Colour(0xff090d10), std::move(holder));
        mainWindow_->setVisible(true);
        startupWindow_ = nullptr;
        progress_.advanceTo(reverb::app::StartupPhase::ready);
    }

    void finishStartupWorker()
    {
        if (startupWorker_.joinable() && startupWorker_.get_id() != std::this_thread::get_id()) {
            startupWorker_.join();
        }
    }

    juce::ApplicationProperties appProperties_;
    reverb::app::StartupProgress progress_;
    std::unique_ptr<StartupWindow> startupWindow_;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow_;
    std::thread startupWorker_;
    std::mutex resultMutex_;
    std::unique_ptr<juce::StandalonePluginHolder> completedHolder_;
    std::atomic<bool> shuttingDown_ {false};
};

} // namespace

JUCE_CREATE_APPLICATION_DEFINE(ReverbStandaloneApp)

#endif
