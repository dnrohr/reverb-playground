#pragma once

#include <reverb/graph/GraphDocument.h>

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace reverb::render {

// Retained as a source-compatible request shorthand; the product UI now uses explicit gains.
enum class FileExportMode : std::uint8_t { wetOnly, auditionMix };

enum class FileExportState : std::uint8_t {
    idle,
    rendering,
    complete,
    cancelled,
    failed,
};

struct ProcessedFileExportRequest final {
    juce::File source;
    juce::File destination;
    reverb::graph::GraphDocument patch;
    FileExportMode mode { FileExportMode::wetOnly };
    double outputSampleRate { 48'000.0 };
    double auditionGain { 1.0 };
    double maximumTailSeconds { 10.0 };
    double silenceThresholdDb { -80.0 };
    bool overwriteConfirmed {};
    double wetGain { -1.0 };
    double dryGain { -1.0 };
};

struct ProcessedFileExportSnapshot final {
    FileExportState state { FileExportState::idle };
    double progress {};
    std::uint64_t generation {};
    std::uint64_t renderedFrames {};
    std::uint64_t sourceFrames {};
    std::uint64_t tailFrames {};
    std::string destinationName;
    std::string error;
};

class ProcessedFileExporter final {
public:
    ProcessedFileExporter() = default;
    ~ProcessedFileExporter();

    ProcessedFileExporter(const ProcessedFileExporter&) = delete;
    ProcessedFileExporter& operator=(const ProcessedFileExporter&) = delete;

    bool start(ProcessedFileExportRequest request, std::string& error);
    void cancel() noexcept;
    void wait();
    [[nodiscard]] ProcessedFileExportSnapshot snapshot() const;

private:
    void render(ProcessedFileExportRequest request, std::uint64_t generation);

    mutable std::mutex mutex_;
    std::thread worker_;
    std::atomic<bool> cancelRequested_ {};
    std::atomic<FileExportState> state_ { FileExportState::idle };
    std::atomic<double> progress_ {};
    std::atomic<std::uint64_t> generation_ {};
    std::atomic<std::uint64_t> renderedFrames_ {};
    std::atomic<std::uint64_t> sourceFrames_ {};
    std::atomic<std::uint64_t> tailFrames_ {};
    std::string destinationName_;
    std::string error_;
};

[[nodiscard]] const char* fileExportStateName(FileExportState state) noexcept;

} // namespace reverb::render
