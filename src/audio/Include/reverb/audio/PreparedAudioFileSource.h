#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace reverb::audio {

enum class AuditionSourceMode : std::uint8_t {
    liveInput,
    audioFile,
    testImpulse,
};

enum class AudioFileTransportState : std::uint8_t {
    empty,
    ready,
    playing,
    paused,
    tail,
    error,
};

struct AudioFileTransportSnapshot {
    AudioFileTransportState state { AudioFileTransportState::empty };
    std::string fileName;
    std::string format;
    std::string error;
    double sourceSampleRate {};
    double outputSampleRate {};
    std::int64_t frameCount {};
    std::int64_t cursorSourceFrame {};
    std::int64_t loopStartSourceFrame {};
    std::int64_t loopEndSourceFrame {};
    std::uint32_t channels {};
    std::uint64_t generation {};
    std::uint64_t underrunEvents {};
    std::uint64_t underrunFrames {};
    std::uint64_t sanitizedSourceSamples {};
    std::size_t ringCapacityFrames {};
    std::size_t preparedBytes {};
    bool loopEnabled {};
    bool prepared {};
};

class PreparedAudioFileSource final {
public:
    // Read-ahead memory is sized no larger than two seconds at this rate. Hosts
    // may probe higher rates; preparation remains bounded instead of throwing.
    static constexpr double maximumOutputSampleRate = 192'000.0;
    static constexpr double readAheadSeconds = 2.0;
    static constexpr std::size_t maximumBlockSize = 8'192;
    static constexpr std::size_t maximumPreparedBytes = 8U * 1024U * 1024U;

    PreparedAudioFileSource();
    ~PreparedAudioFileSource();

    PreparedAudioFileSource(const PreparedAudioFileSource&) = delete;
    PreparedAudioFileSource& operator=(const PreparedAudioFileSource&) = delete;

    void prepare(double outputSampleRate, std::size_t requestedMaximumBlockSize);
    void reset();

    bool loadFile(const juce::File& file, std::string& error);
    void play();
    void pause();
    [[nodiscard]] bool pauseFromAudioThread() noexcept;
    void stop();
    bool seek(std::int64_t sourceFrame, std::string& error);
    bool setLoop(
        bool enabled,
        std::int64_t startSourceFrame,
        std::int64_t endSourceFrame,
        std::string& error);

    void process(
        std::span<float> outputLeft,
        std::span<float> outputRight) noexcept;

    [[nodiscard]] AudioFileTransportSnapshot snapshot() const;

    // Deterministic fault injection for the native underrun contract test.
    void setWorkerStarvedForTesting(bool starved) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] const char* transportStateName(AudioFileTransportState state) noexcept;
[[nodiscard]] const char* auditionSourceModeName(AuditionSourceMode mode) noexcept;

} // namespace reverb::audio
