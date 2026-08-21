#include <reverb/audio/PreparedAudioFileSource.h>

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace reverb::audio {
namespace {

constexpr auto noEofPosition = std::numeric_limits<std::uint64_t>::max();
constexpr auto maximumFileDurationSeconds = 24.0 * 60.0 * 60.0;

class ReaderRangeSource final : public juce::PositionableAudioSource {
public:
    explicit ReaderRangeSource(std::unique_ptr<juce::AudioFormatReader> reader)
        : reader_(std::move(reader))
    {
        loopEnd_ = reader_->lengthInSamples;
    }

    void prepareToPlay(const int samplesPerBlockExpected, double) override
    {
        crossfade_.setSize(2, std::max(1, samplesPerBlockExpected), false, false, true);
    }

    void releaseResources() override
    {
        crossfade_.setSize(0, 0);
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        info.clearActiveBufferRegion();
        if (info.buffer == nullptr || info.numSamples <= 0 || readFailed_)
            return;

        auto destinationOffset = info.startSample;
        auto remaining = info.numSamples;
        while (remaining > 0) {
            if (!loopEnabled_ && position_ >= reader_->lengthInSamples)
                break;
            if (loopEnabled_ && position_ >= loopEnd_)
                position_ = loopStart_;

            const auto boundary = loopEnabled_ ? loopEnd_ : reader_->lengthInSamples;
            const auto available = std::max<std::int64_t>(0, boundary - position_);
            if (available == 0)
                break;
            const auto count = static_cast<int>(std::min<std::int64_t>(remaining, available));
            if (!reader_->read(info.buffer, destinationOffset, count, position_, true, true)) {
                readFailed_ = true;
                info.buffer->clear(destinationOffset, remaining);
                return;
            }

            applyLoopCrossfade(*info.buffer, destinationOffset, count, position_);
            sanitize(*info.buffer, destinationOffset, count);
            position_ += count;
            destinationOffset += count;
            remaining -= count;
        }
    }

    void setNextReadPosition(const juce::int64 newPosition) override
    {
        position_ = std::clamp<std::int64_t>(newPosition, 0, reader_->lengthInSamples);
        readFailed_ = false;
    }

    [[nodiscard]] juce::int64 getNextReadPosition() const override { return position_; }
    [[nodiscard]] juce::int64 getTotalLength() const override { return reader_->lengthInSamples; }
    [[nodiscard]] bool isLooping() const override { return loopEnabled_; }

    void setLoop(const bool enabled, const std::int64_t start, const std::int64_t end)
    {
        loopEnabled_ = enabled;
        loopStart_ = start;
        loopEnd_ = end;
        loopCrossfadeFrames_ = enabled
            ? std::max<std::int64_t>(1, static_cast<std::int64_t>(std::llround(reader_->sampleRate * 0.005)))
            : 0;
        loopCrossfadeFrames_ = std::min(loopCrossfadeFrames_, std::max<std::int64_t>(1, (loopEnd_ - loopStart_) / 2));
        if (loopEnabled_ && (position_ < loopStart_ || position_ >= loopEnd_))
            position_ = loopStart_;
    }

    [[nodiscard]] bool readFailed() const noexcept { return readFailed_; }
    [[nodiscard]] std::uint64_t sanitizedSamples() const noexcept { return sanitizedSamples_; }
    [[nodiscard]] juce::AudioFormatReader& reader() noexcept { return *reader_; }

private:
    void applyLoopCrossfade(
        juce::AudioBuffer<float>& destination,
        const int destinationOffset,
        const int count,
        const std::int64_t sourceStart)
    {
        if (!loopEnabled_ || loopCrossfadeFrames_ <= 0)
            return;
        const auto fadeStart = loopEnd_ - loopCrossfadeFrames_;
        const auto overlapStart = std::max(sourceStart, fadeStart);
        const auto overlapEnd = std::min(sourceStart + count, loopEnd_);
        if (overlapStart >= overlapEnd)
            return;

        const auto overlapCount = static_cast<int>(overlapEnd - overlapStart);
        if (crossfade_.getNumSamples() < overlapCount)
            return;
        crossfade_.clear();
        const auto startReadPosition = loopStart_ + (overlapStart - fadeStart);
        if (!reader_->read(&crossfade_, 0, overlapCount, startReadPosition, true, true)) {
            readFailed_ = true;
            return;
        }

        const auto destinationStart = destinationOffset + static_cast<int>(overlapStart - sourceStart);
        for (auto index = 0; index < overlapCount; ++index) {
            const auto phase = static_cast<double>(overlapStart - fadeStart + index)
                / static_cast<double>(loopCrossfadeFrames_);
            const auto oldGain = static_cast<float>(std::cos(phase * juce::MathConstants<double>::halfPi));
            const auto newGain = static_cast<float>(std::sin(phase * juce::MathConstants<double>::halfPi));
            for (auto channel = 0; channel < 2; ++channel) {
                const auto oldSample = destination.getSample(channel, destinationStart + index);
                const auto newSample = crossfade_.getSample(channel, index);
                destination.setSample(channel, destinationStart + index, oldSample * oldGain + newSample * newGain);
            }
        }
    }

    void sanitize(juce::AudioBuffer<float>& buffer, const int offset, const int count) noexcept
    {
        for (auto channel = 0; channel < std::min(2, buffer.getNumChannels()); ++channel) {
            auto* samples = buffer.getWritePointer(channel, offset);
            for (auto index = 0; index < count; ++index) {
                if (!std::isfinite(samples[index])) {
                    samples[index] = 0.0F;
                    ++sanitizedSamples_;
                }
            }
        }
    }

    std::unique_ptr<juce::AudioFormatReader> reader_;
    juce::AudioBuffer<float> crossfade_;
    std::int64_t position_ {};
    std::int64_t loopStart_ {};
    std::int64_t loopEnd_ {};
    std::int64_t loopCrossfadeFrames_ {};
    std::uint64_t sanitizedSamples_ {};
    bool loopEnabled_ {};
    bool readFailed_ {};
};

std::unique_ptr<juce::AudioFormatReader> openSupportedReader(const juce::File& file)
{
    juce::AudioFormatManager manager;
    manager.registerFormat(new juce::WavAudioFormat(), true);
    manager.registerFormat(new juce::AiffAudioFormat(), false);
    manager.registerFormat(new juce::FlacAudioFormat(), false);
    return std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(file));
}

} // namespace

struct PreparedAudioFileSource::Impl final {
    Impl()
        : worker([this] { workerLoop(); })
    {
    }

    ~Impl()
    {
        stopWorker.store(true, std::memory_order_release);
        wake.notify_all();
        if (worker.joinable())
            worker.join();
    }

    template <typename Function>
    void reconfigure(Function&& function)
    {
        reconfiguring.store(true, std::memory_order_release);
        while (processing.load(std::memory_order_acquire))
            std::this_thread::yield();
        {
            const std::scoped_lock lock(workerMutex);
            std::forward<Function>(function)();
        }
        reconfiguring.store(false, std::memory_order_release);
        wake.notify_all();
    }

    void allocateRingLocked(const double rate, const std::size_t blockSize)
    {
        outputSampleRate = rate;
        maximumBlockSize = blockSize;
        const auto requested = static_cast<std::size_t>(std::ceil(rate * PreparedAudioFileSource::readAheadSeconds));
        ringCapacity = std::max<std::size_t>(blockSize * 4, requested);
        ringCapacity = std::min<std::size_t>(ringCapacity, static_cast<std::size_t>(PreparedAudioFileSource::maximumOutputSampleRate * PreparedAudioFileSource::readAheadSeconds));
        ringLeft.assign(ringCapacity, 0.0F);
        ringRight.assign(ringCapacity, 0.0F);
        ringSourceFrame.assign(ringCapacity, 0);
        workerBuffer.setSize(2, static_cast<int>(blockSize), false, false, true);
        preparedBytes = ringLeft.capacity() * sizeof(float)
            + ringRight.capacity() * sizeof(float)
            + ringSourceFrame.capacity() * sizeof(std::int64_t)
            + static_cast<std::size_t>(workerBuffer.getNumChannels() * workerBuffer.getNumSamples()) * sizeof(float);
        flushRingLocked();
        prepared.store(rate > 0.0 && blockSize > 0 && preparedBytes <= PreparedAudioFileSource::maximumPreparedBytes,
            std::memory_order_release);
    }

    void flushRingLocked() noexcept
    {
        readPosition.store(0, std::memory_order_release);
        writePosition.store(0, std::memory_order_release);
        eofPosition.store(noEofPosition, std::memory_order_release);
    }

    void rebuildPipelineLocked(const std::int64_t sourcePosition)
    {
        resampler.reset();
        if (readerSource == nullptr || outputSampleRate <= 0.0 || maximumBlockSize == 0)
            return;
        readerSource->setLoop(loopEnabled, loopStart, loopEnd);
        readerSource->setNextReadPosition(sourcePosition);
        resampler = std::make_unique<juce::ResamplingAudioSource>(readerSource.get(), false, 2);
        sourcePerOutput = sourceSampleRate / outputSampleRate;
        resampler->setResamplingRatio(sourcePerOutput);
        resampler->prepareToPlay(static_cast<int>(maximumBlockSize), outputSampleRate);
        phaseSourceFrame = static_cast<double>(sourcePosition);
        cursorSourceFrame.store(sourcePosition, std::memory_order_release);
        flushRingLocked();
    }

    void workerLoop()
    {
        for (;;) {
            std::unique_lock lock(workerMutex);
            wake.wait_for(lock, std::chrono::milliseconds(2), [this] {
                return stopWorker.load(std::memory_order_acquire);
            });
            if (stopWorker.load(std::memory_order_acquire))
                return;
            while (fillOneChunkLocked()) {
            }
        }
    }

    bool fillOneChunkLocked()
    {
        if (state.load(std::memory_order_acquire) != AudioFileTransportState::playing
            || workerStarved.load(std::memory_order_acquire)
            || resampler == nullptr || ringCapacity == 0)
            return false;

        const auto read = readPosition.load(std::memory_order_acquire);
        const auto write = writePosition.load(std::memory_order_relaxed);
        const auto occupied = write - read;
        if (occupied >= ringCapacity)
            return false;
        auto count = std::min<std::size_t>(maximumBlockSize, ringCapacity - static_cast<std::size_t>(occupied));
        if (!loopEnabled) {
            const auto remainingSource = static_cast<double>(frameCount) - phaseSourceFrame;
            if (remainingSource <= 0.0) {
                eofPosition.store(write, std::memory_order_release);
                return false;
            }
            const auto remainingOutput = static_cast<std::size_t>(std::ceil(remainingSource / sourcePerOutput));
            count = std::min(count, remainingOutput);
        }
        if (count == 0)
            return false;

        workerBuffer.clear();
        const juce::AudioSourceChannelInfo info(&workerBuffer, 0, static_cast<int>(count));
        resampler->getNextAudioBlock(info);
        if (readerSource->readFailed()) {
            error = "The audio file could not be decoded while reading.";
            state.store(AudioFileTransportState::error, std::memory_order_release);
            eofPosition.store(write, std::memory_order_release);
            return false;
        }

        for (std::size_t index = 0; index < count; ++index) {
            const auto ringIndex = static_cast<std::size_t>((write + index) % ringCapacity);
            const auto left = workerBuffer.getSample(0, static_cast<int>(index));
            const auto right = workerBuffer.getSample(1, static_cast<int>(index));
            ringLeft[ringIndex] = std::isfinite(left) ? left : 0.0F;
            ringRight[ringIndex] = std::isfinite(right) ? right : 0.0F;
            ringSourceFrame[ringIndex] = static_cast<std::int64_t>(std::floor(phaseSourceFrame));
            phaseSourceFrame += sourcePerOutput;
            if (loopEnabled && phaseSourceFrame >= static_cast<double>(loopEnd)) {
                const auto length = static_cast<double>(loopEnd - loopStart);
                phaseSourceFrame = static_cast<double>(loopStart)
                    + std::fmod(phaseSourceFrame - static_cast<double>(loopStart), length);
            }
        }
        writePosition.store(write + count, std::memory_order_release);
        if (!loopEnabled && phaseSourceFrame >= static_cast<double>(frameCount))
            eofPosition.store(write + count, std::memory_order_release);
        return write + count - read < ringCapacity
            && eofPosition.load(std::memory_order_acquire) == noEofPosition;
    }

    void prefill()
    {
        wake.notify_all();
        const auto required = std::min<std::size_t>(
            ringCapacity,
            std::max<std::size_t>(maximumBlockSize * 4, static_cast<std::size_t>(outputSampleRate / 4.0)));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (std::chrono::steady_clock::now() < deadline) {
            const auto available = writePosition.load(std::memory_order_acquire)
                - readPosition.load(std::memory_order_acquire);
            if (available >= required || eofPosition.load(std::memory_order_acquire) != noEofPosition
                || state.load(std::memory_order_acquire) == AudioFileTransportState::error)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    mutable std::mutex workerMutex;
    std::condition_variable wake;
    std::thread worker;
    std::atomic<bool> stopWorker {};
    std::atomic<bool> workerStarved {};
    std::atomic<bool> reconfiguring {};
    std::atomic<bool> processing {};
    std::atomic<bool> prepared {};
    std::atomic<AudioFileTransportState> state { AudioFileTransportState::empty };
    std::atomic<std::uint64_t> readPosition {};
    std::atomic<std::uint64_t> writePosition {};
    std::atomic<std::uint64_t> eofPosition { noEofPosition };
    std::atomic<std::int64_t> cursorSourceFrame {};
    std::atomic<std::uint64_t> generation {};
    std::atomic<std::uint64_t> underrunEvents {};
    std::atomic<std::uint64_t> underrunFrames {};
    std::unique_ptr<ReaderRangeSource> readerSource;
    std::unique_ptr<juce::ResamplingAudioSource> resampler;
    juce::AudioBuffer<float> workerBuffer;
    std::vector<float> ringLeft;
    std::vector<float> ringRight;
    std::vector<std::int64_t> ringSourceFrame;
    std::string fileName;
    std::string format;
    std::string error;
    double sourceSampleRate {};
    double outputSampleRate {};
    double sourcePerOutput { 1.0 };
    double phaseSourceFrame {};
    std::int64_t frameCount {};
    std::int64_t loopStart {};
    std::int64_t loopEnd {};
    std::uint32_t channels {};
    std::size_t maximumBlockSize {};
    std::size_t ringCapacity {};
    std::size_t preparedBytes {};
    bool loopEnabled {};
};

PreparedAudioFileSource::PreparedAudioFileSource()
    : impl_(std::make_unique<Impl>())
{
}

PreparedAudioFileSource::~PreparedAudioFileSource() = default;

void PreparedAudioFileSource::prepare(
    const double outputSampleRate,
    const std::size_t requestedMaximumBlockSize)
{
    if (!std::isfinite(outputSampleRate) || outputSampleRate <= 0.0
        || outputSampleRate > maximumOutputSampleRate || requestedMaximumBlockSize == 0
        || requestedMaximumBlockSize > PreparedAudioFileSource::maximumBlockSize)
        throw std::invalid_argument("Audio-file transport preparation is outside supported bounds.");
    const auto cursor = impl_->cursorSourceFrame.load(std::memory_order_acquire);
    impl_->reconfigure([&] {
        impl_->allocateRingLocked(outputSampleRate, requestedMaximumBlockSize);
        if (impl_->readerSource != nullptr) {
            impl_->rebuildPipelineLocked(std::clamp<std::int64_t>(cursor, 0, impl_->frameCount));
            impl_->state.store(AudioFileTransportState::paused, std::memory_order_release);
        }
    });
}

void PreparedAudioFileSource::reset()
{
    impl_->reconfigure([&] {
        if (impl_->readerSource != nullptr) {
            impl_->rebuildPipelineLocked(0);
            impl_->state.store(AudioFileTransportState::ready, std::memory_order_release);
        } else {
            impl_->flushRingLocked();
            impl_->state.store(AudioFileTransportState::empty, std::memory_order_release);
        }
    });
}

bool PreparedAudioFileSource::loadFile(const juce::File& file, std::string& error)
{
    error.clear();
    if (!file.existsAsFile()) {
        error = "The selected audio file does not exist or is no longer available.";
        return false;
    }
    auto reader = openSupportedReader(file);
    if (reader == nullptr) {
        error = "Unsupported or corrupt audio file. Choose a WAV, AIFF, or FLAC file.";
        return false;
    }
    if (reader->numChannels == 0 || reader->numChannels > 2) {
        error = "Audio files must contain one or two channels; this file contains "
            + std::to_string(reader->numChannels) + ".";
        return false;
    }
    if (!std::isfinite(reader->sampleRate) || reader->sampleRate <= 0.0
        || reader->lengthInSamples <= 0
        || static_cast<double>(reader->lengthInSamples) / reader->sampleRate > maximumFileDurationSeconds) {
        error = "The audio file has invalid or unsupported duration/sample-rate metadata.";
        return false;
    }

    const auto name = file.getFileName().toStdString();
    const auto format = reader->getFormatName().toStdString();
    const auto rate = reader->sampleRate;
    const auto frames = reader->lengthInSamples;
    const auto channelCount = reader->numChannels;
    impl_->reconfigure([&] {
        impl_->readerSource = std::make_unique<ReaderRangeSource>(std::move(reader));
        impl_->fileName = name;
        impl_->format = format;
        impl_->error.clear();
        impl_->sourceSampleRate = rate;
        impl_->frameCount = frames;
        impl_->channels = channelCount;
        impl_->loopEnabled = false;
        impl_->loopStart = 0;
        impl_->loopEnd = frames;
        impl_->generation.fetch_add(1, std::memory_order_acq_rel);
        impl_->underrunEvents.store(0, std::memory_order_release);
        impl_->underrunFrames.store(0, std::memory_order_release);
        impl_->rebuildPipelineLocked(0);
        impl_->state.store(AudioFileTransportState::ready, std::memory_order_release);
    });
    return true;
}

void PreparedAudioFileSource::play()
{
    bool canPlay = false;
    impl_->reconfigure([&] {
        if (impl_->readerSource == nullptr || !impl_->prepared.load(std::memory_order_acquire))
            return;
        auto cursor = impl_->cursorSourceFrame.load(std::memory_order_acquire);
        if (impl_->state.load(std::memory_order_acquire) == AudioFileTransportState::tail
            || cursor >= impl_->frameCount)
            cursor = impl_->loopEnabled ? impl_->loopStart : 0;
        impl_->rebuildPipelineLocked(cursor);
        impl_->state.store(AudioFileTransportState::playing, std::memory_order_release);
        canPlay = true;
    });
    if (canPlay)
        impl_->prefill();
}

void PreparedAudioFileSource::pause()
{
    const auto cursor = impl_->cursorSourceFrame.load(std::memory_order_acquire);
    impl_->reconfigure([&] {
        if (impl_->readerSource == nullptr)
            return;
        impl_->rebuildPipelineLocked(cursor);
        impl_->state.store(AudioFileTransportState::paused, std::memory_order_release);
    });
}

void PreparedAudioFileSource::stop()
{
    impl_->reconfigure([&] {
        if (impl_->readerSource == nullptr)
            return;
        impl_->rebuildPipelineLocked(0);
        impl_->state.store(AudioFileTransportState::ready, std::memory_order_release);
    });
}

bool PreparedAudioFileSource::seek(const std::int64_t sourceFrame, std::string& error)
{
    error.clear();
    if (impl_->readerSource == nullptr) {
        error = "Load an audio file before seeking.";
        return false;
    }
    const auto target = std::clamp<std::int64_t>(sourceFrame, 0, impl_->frameCount);
    impl_->reconfigure([&] {
        impl_->rebuildPipelineLocked(target);
        impl_->state.store(AudioFileTransportState::paused, std::memory_order_release);
    });
    return true;
}

bool PreparedAudioFileSource::setLoop(
    const bool enabled,
    const std::int64_t startSourceFrame,
    const std::int64_t endSourceFrame,
    std::string& error)
{
    error.clear();
    if (impl_->readerSource == nullptr) {
        error = "Load an audio file before setting a loop.";
        return false;
    }
    const auto minimumFrames = std::max<std::int64_t>(
        static_cast<std::int64_t>(std::ceil(impl_->sourceSampleRate * 0.020)),
        static_cast<std::int64_t>(std::ceil(
            static_cast<double>(impl_->maximumBlockSize) * impl_->sourceSampleRate / impl_->outputSampleRate)));
    if (enabled && (startSourceFrame < 0 || endSourceFrame > impl_->frameCount
        || endSourceFrame - startSourceFrame < minimumFrames)) {
        error = "Loop range must be inside the file and at least 20 ms or one output block long.";
        return false;
    }
    const auto cursor = impl_->cursorSourceFrame.load(std::memory_order_acquire);
    const auto wasPlaying = impl_->state.load(std::memory_order_acquire) == AudioFileTransportState::playing;
    impl_->reconfigure([&] {
        impl_->loopEnabled = enabled;
        impl_->loopStart = enabled ? startSourceFrame : 0;
        impl_->loopEnd = enabled ? endSourceFrame : impl_->frameCount;
        const auto boundedCursor = enabled
            ? std::clamp(cursor, impl_->loopStart, impl_->loopEnd - 1)
            : std::clamp<std::int64_t>(cursor, 0, impl_->frameCount);
        impl_->rebuildPipelineLocked(boundedCursor);
        impl_->state.store(
            wasPlaying ? AudioFileTransportState::playing : AudioFileTransportState::paused,
            std::memory_order_release);
    });
    if (wasPlaying)
        impl_->prefill();
    return true;
}

void PreparedAudioFileSource::process(
    const std::span<float> outputLeft,
    const std::span<float> outputRight) noexcept
{
    std::ranges::fill(outputLeft, 0.0F);
    std::ranges::fill(outputRight, 0.0F);
    const auto frameCount = std::min(outputLeft.size(), outputRight.size());
    if (frameCount == 0 || impl_->reconfiguring.load(std::memory_order_acquire))
        return;

    impl_->processing.store(true, std::memory_order_release);
    if (impl_->reconfiguring.load(std::memory_order_acquire)) {
        impl_->processing.store(false, std::memory_order_release);
        return;
    }

    auto read = impl_->readPosition.load(std::memory_order_relaxed);
    const auto write = impl_->writePosition.load(std::memory_order_acquire);
    auto rendered = std::size_t {};
    if (impl_->state.load(std::memory_order_acquire) == AudioFileTransportState::playing) {
        const auto available = static_cast<std::size_t>(std::min<std::uint64_t>(write - read, frameCount));
        for (; rendered < available; ++rendered) {
            const auto ringIndex = static_cast<std::size_t>(read % impl_->ringCapacity);
            outputLeft[rendered] = impl_->ringLeft[ringIndex];
            outputRight[rendered] = impl_->ringRight[ringIndex];
            impl_->cursorSourceFrame.store(impl_->ringSourceFrame[ringIndex], std::memory_order_relaxed);
            ++read;
        }
        impl_->readPosition.store(read, std::memory_order_release);

        const auto eof = impl_->eofPosition.load(std::memory_order_acquire);
        if (eof != noEofPosition && read >= eof) {
            impl_->state.store(AudioFileTransportState::tail, std::memory_order_release);
            impl_->cursorSourceFrame.store(impl_->frameCount, std::memory_order_release);
        } else if (rendered < frameCount) {
            impl_->underrunEvents.fetch_add(1, std::memory_order_relaxed);
            impl_->underrunFrames.fetch_add(frameCount - rendered, std::memory_order_relaxed);
        }
    }
    impl_->processing.store(false, std::memory_order_release);
}

AudioFileTransportSnapshot PreparedAudioFileSource::snapshot() const
{
    const std::scoped_lock lock(impl_->workerMutex);
    return {
        .state = impl_->state.load(std::memory_order_acquire),
        .fileName = impl_->fileName,
        .format = impl_->format,
        .error = impl_->error,
        .sourceSampleRate = impl_->sourceSampleRate,
        .outputSampleRate = impl_->outputSampleRate,
        .frameCount = impl_->frameCount,
        .cursorSourceFrame = impl_->cursorSourceFrame.load(std::memory_order_acquire),
        .loopStartSourceFrame = impl_->loopStart,
        .loopEndSourceFrame = impl_->loopEnd,
        .channels = impl_->channels,
        .generation = impl_->generation.load(std::memory_order_acquire),
        .underrunEvents = impl_->underrunEvents.load(std::memory_order_acquire),
        .underrunFrames = impl_->underrunFrames.load(std::memory_order_acquire),
        .sanitizedSourceSamples = impl_->readerSource != nullptr
            ? impl_->readerSource->sanitizedSamples() : 0,
        .ringCapacityFrames = impl_->ringCapacity,
        .preparedBytes = impl_->preparedBytes,
        .loopEnabled = impl_->loopEnabled,
        .prepared = impl_->prepared.load(std::memory_order_acquire),
    };
}

void PreparedAudioFileSource::setWorkerStarvedForTesting(const bool starved) noexcept
{
    impl_->workerStarved.store(starved, std::memory_order_release);
    impl_->wake.notify_all();
}

const char* transportStateName(const AudioFileTransportState state) noexcept
{
    switch (state) {
        case AudioFileTransportState::empty: return "empty";
        case AudioFileTransportState::ready: return "ready";
        case AudioFileTransportState::playing: return "playing";
        case AudioFileTransportState::paused: return "paused";
        case AudioFileTransportState::tail: return "tail";
        case AudioFileTransportState::error: return "error";
    }
    return "error";
}

const char* auditionSourceModeName(const AuditionSourceMode mode) noexcept
{
    switch (mode) {
        case AuditionSourceMode::liveInput: return "live-input";
        case AuditionSourceMode::audioFile: return "audio-file";
        case AuditionSourceMode::testImpulse: return "test-impulse";
    }
    return "live-input";
}

} // namespace reverb::audio
