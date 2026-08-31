#include "PluginEditor.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include <reverb/ui/EditorSizing.h>

ReverbPlaygroundEditor::ReverbPlaygroundEditor(ReverbPlaygroundProcessor& processor)
    : AudioProcessorEditor(processor)
    , shell_({
          [&processor] { processor.triggerImpulse(); },
          [&processor](const float gain) { processor.setWetGain(gain); },
          [&processor](const float gain) { processor.setDryGain(gain); },
          [&processor](const bool muted) { processor.setEmergencyMuted(muted); },
          [&processor] { processor.requestSafetyReset(); },
          [] {
              if (auto* holder = juce::StandalonePluginHolder::getInstance()) {
                  holder->showAudioSettingsDialog();
                  return;
              }
              juce::AlertWindow::showMessageBoxAsync(
                  juce::MessageBoxIconType::InfoIcon,
                  "Audio device",
                  "Audio device selection is managed by the plugin host.");
          },
          [&processor] {
              const auto rate = processor.activeSampleRate();
              if (auto* holder = juce::StandalonePluginHolder::getInstance()) {
                  if (const auto* device = holder->deviceManager.getCurrentAudioDevice()) {
                      return juce::String::fromUTF8("\xe2\x97\x8f  AUDIO ONLINE  /  ") + device->getName()
                          + "  /  " + juce::String(rate / 1000.0, 1) + " kHz";
                  }
                  return juce::String::fromUTF8("\xe2\x97\x8b  NO AUDIO DEVICE  /  UI READY");
              }
              return rate > 0.0
                  ? juce::String::fromUTF8("\xe2\x97\x8f  HOST AUDIO  /  ") + juce::String(rate / 1000.0, 1) + " kHz"
                  : juce::String::fromUTF8("\xe2\x97\x8b  WAITING FOR HOST AUDIO");
          },
          [&processor] { return processor.wetGain(); },
          [&processor] { return processor.dryGain(); },
          [&processor] { return processor.isEmergencyMuted(); },
          [&processor] { return processor.isSafetyLatched(); },
          [&processor] { return processor.runtimeSnapshotJson(); },
          [&processor](const auto& node, const auto& parameter, const double value) {
              return processor.setRuntimeParameter(node, parameter, value);
          },
          [&processor](const double length, const double threshold) {
              return processor.startImpulseCapture(length, threshold);
          },
          [&processor] { return processor.impulseCaptureStatusJson(); },
          [&processor] { return processor.impulseCaptureJson(); },
          [&processor](const bool enabled) { return processor.setEnergyTelemetryEnabled(enabled); },
          [&processor] { return processor.energyTelemetryJson(); },
          [&processor] { return processor.runtimeDiagnosticsJson(); },
          [&processor](const auto& patchJson) { return processor.publishGraphJson(patchJson); },
          [&processor](const auto& patchJson) { return processor.previewGraphJson(patchJson); },
          [&processor](const auto& patchJson) { return processor.storePatchStateJson(patchJson); },
          juce::StandalonePluginHolder::getInstance() != nullptr,
          [&processor](const juce::File& file) {
              std::string error;
              static_cast<void>(processor.loadAudioFile(file, error));
              return juce::String::fromUTF8(error.data(), static_cast<int>(error.size()));
          },
          [&processor](const int mode) {
              processor.setAuditionSourceMode(static_cast<reverb::audio::AuditionSourceMode>(
                  juce::jlimit(0, 2, mode)));
          },
          [&processor] { processor.playAudioFile(); },
          [&processor] { processor.pauseAudioFile(); },
          [&processor] { processor.stopAudioFile(); },
          [&processor](const std::int64_t frame) {
              std::string error;
              static_cast<void>(processor.seekAudioFile(frame, error));
              return juce::String::fromUTF8(error.data(), static_cast<int>(error.size()));
          },
          [&processor](const bool enabled, const std::int64_t start, const std::int64_t end) {
              std::string error;
              static_cast<void>(processor.setAudioFileLoop(enabled, start, end, error));
              return juce::String::fromUTF8(error.data(), static_cast<int>(error.size()));
          },
          [&processor] { return processor.audioFileTransportJson(); },
          [&processor](const juce::File& destination, const int range, const bool overwriteConfirmed) {
              std::string error;
              static_cast<void>(processor.startProcessedFileExport(destination,
                  range == 1 ? reverb::render::FileExportRange::selectedLoop
                             : reverb::render::FileExportRange::entireFile,
                  overwriteConfirmed, error));
              return juce::String::fromUTF8(error.data(), static_cast<int>(error.size()));
          },
          [&processor] { processor.cancelProcessedFileExport(); },
          [&processor] { return processor.processedFileExportJson(); },
      })
{
    addAndMakeVisible(shell_);
    setResizable(true, true);
    setResizeLimits(640, 400, 8192, 8192);
    const auto standalone = juce::StandalonePluginHolder::getInstance() != nullptr;
    const auto initial = reverb::ui::preferredEditorSize(
        standalone, 0, 0);
    setSize(initial.width, initial.height);

}

void ReverbPlaygroundEditor::resized()
{
    shell_.setBounds(getLocalBounds());
}
