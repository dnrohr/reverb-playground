#include <reverb/render/ProcessedFileExporter.h>

#include <reverb/graph/AcyclicRuntime.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

namespace reverb::render {
namespace {

constexpr std::size_t blockSize = 256;
constexpr double minimumTailObservationSeconds = 2.0;
constexpr double silenceHoldSeconds = 0.5;
constexpr float hardSafetyCeiling = 16.0F;

std::unique_ptr<juce::AudioFormatReader> openReader(const juce::File& file)
{
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    return std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(file));
}

std::string compileError(const reverb::graph::AcyclicCompileResult& result)
{
    auto message = std::string { "The current graph could not be prepared for export." };
    for (const auto& error : result.errors) message += " " + error;
    return message;
}

} // namespace

ProcessedFileExporter::~ProcessedFileExporter()
{
    cancel();
    wait();
}

bool ProcessedFileExporter::start(ProcessedFileExportRequest request, std::string& error)
{
    error.clear();
    if (state_.load(std::memory_order_acquire) == FileExportState::rendering) {
        error = "An audio-file export is already running.";
        return false;
    }
    wait();
    if (!request.source.existsAsFile()) {
        error = "The source audio file is missing or unavailable.";
        return false;
    }
    const auto extension = request.source.getFileExtension().toLowerCase();
    if (extension != ".wav" && extension != ".aif" && extension != ".aiff" && extension != ".flac") {
        error = "The source must be a WAV, AIFF, or FLAC file.";
        return false;
    }
    if (request.destination.exists() && !request.overwriteConfirmed) {
        error = "The export destination already exists.";
        return false;
    }
    if (request.destination == request.source) {
        error = "Choose a destination different from the source file.";
        return false;
    }
    const auto explicitGains = request.wetGain >= 0.0 && request.dryGain >= 0.0;
    if (!explicitGains) {
        request.wetGain = request.mode == FileExportMode::wetOnly ? request.auditionGain : 0.5 * request.auditionGain;
        request.dryGain = request.mode == FileExportMode::auditionMix ? 0.5 * request.auditionGain : 0.0;
    }
    if (!std::isfinite(request.maximumTailSeconds) || request.maximumTailSeconds < 0.5
        || request.maximumTailSeconds > 60.0 || !std::isfinite(request.silenceThresholdDb)
        || request.silenceThresholdDb < -120.0 || request.silenceThresholdDb > -40.0
        || !std::isfinite(request.wetGain) || request.wetGain < 0.0 || request.wetGain > 1.0
        || !std::isfinite(request.dryGain) || request.dryGain < 0.0 || request.dryGain > 1.0
        || !std::isfinite(request.outputSampleRate)
        || request.outputSampleRate < 8'000.0 || request.outputSampleRate > 192'000.0) {
        error = "Export tail, silence threshold, or wet/dry gain is outside the supported range.";
        return false;
    }
    auto reader = openReader(request.source);
    if (reader == nullptr || reader->numChannels == 0 || reader->numChannels > 2
        || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0) {
        error = "The source must be a readable mono or stereo WAV, AIFF, or FLAC file.";
        return false;
    }
    if (request.range == FileExportRange::entireFile) {
        request.sourceStartFrame = 0;
        request.sourceEndFrame = reader->lengthInSamples;
    } else if (request.sourceStartFrame < 0
        || request.sourceEndFrame <= request.sourceStartFrame
        || request.sourceEndFrame > reader->lengthInSamples) {
        error = "Selected Loop requires a valid non-empty range inside the source file.";
        return false;
    }

    const auto generation = generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
    {
        const std::scoped_lock lock(mutex_);
        destinationName_ = request.destination.getFileName().toStdString();
        error_.clear();
    }
    cancelRequested_.store(false, std::memory_order_release);
    progress_.store(0.0, std::memory_order_release);
    renderedFrames_.store(0, std::memory_order_release);
    sourceFrames_.store(static_cast<std::uint64_t>(
        request.sourceEndFrame - request.sourceStartFrame), std::memory_order_release);
    tailFrames_.store(0, std::memory_order_release);
    state_.store(FileExportState::rendering, std::memory_order_release);
    worker_ = std::thread([this, request = std::move(request), generation] () mutable {
        render(std::move(request), generation);
    });
    return true;
}

void ProcessedFileExporter::cancel() noexcept
{
    cancelRequested_.store(true, std::memory_order_release);
}

void ProcessedFileExporter::wait()
{
    if (worker_.joinable()) worker_.join();
}

ProcessedFileExportSnapshot ProcessedFileExporter::snapshot() const
{
    const std::scoped_lock lock(mutex_);
    return {
        .state = state_.load(std::memory_order_acquire),
        .progress = progress_.load(std::memory_order_acquire),
        .generation = generation_.load(std::memory_order_acquire),
        .renderedFrames = renderedFrames_.load(std::memory_order_acquire),
        .sourceFrames = sourceFrames_.load(std::memory_order_acquire),
        .tailFrames = tailFrames_.load(std::memory_order_acquire),
        .destinationName = destinationName_,
        .error = error_,
    };
}

void ProcessedFileExporter::render(
    ProcessedFileExportRequest request, const std::uint64_t generation)
{
    const auto temporary = request.destination.getSiblingFile(
        request.destination.getFileNameWithoutExtension() + ".partial-"
        + juce::String(generation) + ".wav");
    static_cast<void>(temporary.deleteFile());
    try {
        auto reader = openReader(request.source);
        if (reader == nullptr) throw std::runtime_error("The source file could not be reopened for export.");
        const auto sourceSampleRate = reader->sampleRate;
        const auto selectedSourceFrames = static_cast<std::uint64_t>(
            request.sourceEndFrame - request.sourceStartFrame);
        const auto sampleRate = request.outputSampleRate;
        const auto totalSourceFrames = static_cast<std::uint64_t>(
            std::ceil(static_cast<double>(selectedSourceFrames) * sampleRate / sourceSampleRate));
        sourceFrames_.store(totalSourceFrames, std::memory_order_release);
        const auto maximumTailFrames = static_cast<std::uint64_t>(
            std::llround(request.maximumTailSeconds * sampleRate));
        const auto minimumObservationFrames = static_cast<std::uint64_t>(
            std::llround(std::min(request.maximumTailSeconds, minimumTailObservationSeconds) * sampleRate));
        const auto silenceHoldFrames = static_cast<std::uint64_t>(std::llround(silenceHoldSeconds * sampleRate));
        const auto silenceThreshold = static_cast<float>(std::pow(10.0, request.silenceThresholdDb / 20.0));

        auto compiled = reverb::graph::compileFeedbackGraph(request.patch, sampleRate, blockSize);
        if (!compiled.valid()) throw std::runtime_error(compileError(compiled));

        auto readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
        juce::ResamplingAudioSource resampler(readerSource.get(), false, 2);
        resampler.setResamplingRatio(sourceSampleRate / sampleRate);
        resampler.prepareToPlay(static_cast<int>(blockSize), sampleRate);
        readerSource->setNextReadPosition(request.sourceStartFrame);

        std::unique_ptr<juce::OutputStream> outputStream = temporary.createOutputStream();
        if (outputStream == nullptr) throw std::runtime_error("The temporary export file could not be created.");
        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor(outputStream, juce::AudioFormatWriterOptions {}
            .withSampleRate(sampleRate).withNumChannels(2).withBitsPerSample(24));
        if (writer == nullptr) throw std::runtime_error("The 24-bit stereo WAV writer could not be created.");

        juce::AudioBuffer<float> input(2, static_cast<int>(blockSize));
        juce::AudioBuffer<float> output(2, static_cast<int>(blockSize));
        auto sourcePosition = std::uint64_t {};
        auto tailPosition = std::uint64_t {};
        auto silentTailFrames = std::uint64_t {};
        while (sourcePosition < totalSourceFrames || tailPosition < maximumTailFrames) {
            if (cancelRequested_.load(std::memory_order_acquire)) {
                writer.reset();
                static_cast<void>(temporary.deleteFile());
                state_.store(FileExportState::cancelled, std::memory_order_release);
                return;
            }
            const auto inSource = sourcePosition < totalSourceFrames;
            const auto remaining = inSource ? totalSourceFrames - sourcePosition : maximumTailFrames - tailPosition;
            const auto count = static_cast<int>(std::min<std::uint64_t>(blockSize, remaining));
            input.clear();
            if (inSource) {
                const juce::AudioSourceChannelInfo sourceInfo(&input, 0, count);
                resampler.getNextAudioBlock(sourceInfo);
            }
            output.clear();
            compiled.runtime->process(
                { input.getReadPointer(0), static_cast<std::size_t>(count) },
                { input.getReadPointer(1), static_cast<std::size_t>(count) },
                { output.getWritePointer(0), static_cast<std::size_t>(count) },
                { output.getWritePointer(1), static_cast<std::size_t>(count) });

            auto peak = 0.0F;
            for (auto channel = 0; channel < 2; ++channel) {
                auto* wet = output.getWritePointer(channel);
                const auto* dry = input.getReadPointer(channel);
                for (auto frame = 0; frame < count; ++frame) {
                    const auto sample = wet[frame] * static_cast<float>(request.wetGain)
                        + dry[frame] * static_cast<float>(request.dryGain);
                    if (!std::isfinite(sample) || std::abs(sample) > hardSafetyCeiling)
                        throw std::runtime_error("Export stopped because the graph produced unsafe output.");
                    wet[frame] = sample;
                    peak = std::max(peak, std::abs(sample));
                }
            }
            if (!writer->writeFromAudioSampleBuffer(output, 0, count))
                throw std::runtime_error("The temporary WAV failed while writing.");

            if (inSource) sourcePosition += static_cast<std::uint64_t>(count);
            else {
                tailPosition += static_cast<std::uint64_t>(count);
                silentTailFrames = peak < silenceThreshold ? silentTailFrames + static_cast<std::uint64_t>(count) : 0;
            }
            const auto rendered = sourcePosition + tailPosition;
            renderedFrames_.store(rendered, std::memory_order_release);
            tailFrames_.store(tailPosition, std::memory_order_release);
            progress_.store(std::min(1.0, static_cast<double>(rendered)
                / static_cast<double>(totalSourceFrames + maximumTailFrames)), std::memory_order_release);
            if (!inSource && tailPosition >= minimumObservationFrames
                && silentTailFrames >= silenceHoldFrames)
                break;
        }
        writer.reset();
        if (cancelRequested_.load(std::memory_order_acquire)) {
            static_cast<void>(temporary.deleteFile());
            state_.store(FileExportState::cancelled, std::memory_order_release);
            return;
        }
        const auto finalized = request.destination.exists()
            ? temporary.replaceFileIn(request.destination)
            : temporary.moveFileTo(request.destination);
        if (!finalized)
            throw std::runtime_error("The completed temporary WAV could not be finalized at the destination.");
        progress_.store(1.0, std::memory_order_release);
        state_.store(FileExportState::complete, std::memory_order_release);
    } catch (const std::exception& exception) {
        static_cast<void>(temporary.deleteFile());
        {
            const std::scoped_lock lock(mutex_);
            error_ = exception.what();
        }
        state_.store(FileExportState::failed, std::memory_order_release);
    }
}

const char* fileExportStateName(const FileExportState state) noexcept
{
    switch (state) {
        case FileExportState::idle: return "idle";
        case FileExportState::rendering: return "rendering";
        case FileExportState::complete: return "complete";
        case FileExportState::cancelled: return "cancelled";
        case FileExportState::failed: return "failed";
    }
    return "failed";
}

} // namespace reverb::render
