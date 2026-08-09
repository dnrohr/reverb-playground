#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <vector>

namespace reverb::dsp {

struct ImpulseCaptureConfig final {
    double maximumLengthMilliseconds { 2'000.0 };
    double stopThresholdDb { -80.0 };
    bool muteLiveInput { true };
    float impulseLevel { 0.1F };
};

struct ImpulseCaptureResult final {
    std::uint64_t generation {};
    double sampleRate {};
    ImpulseCaptureConfig config;
    bool stoppedAtThreshold {};
    std::vector<float> left;
    std::vector<float> right;
};

enum class ImpulseCaptureState : std::uint8_t { idle, armed, capturing, complete };

class ImpulseCapture final {
public:
    static constexpr double minimumLengthMilliseconds = 100.0;
    static constexpr double maximumLengthMilliseconds = 10'000.0;
    static constexpr double minimumStopThresholdDb = -120.0;
    static constexpr double maximumStopThresholdDb = -24.0;
    static constexpr float maximumImpulseLevel = 0.25F;
    static constexpr double maximumSupportedSampleRate = 192'000.0;

    void prepare(double sampleRate);
    [[nodiscard]] ImpulseCaptureConfig request(ImpulseCaptureConfig config) noexcept;
    [[nodiscard]] bool beginIfRequested() noexcept;
    void append(std::span<const float> left, std::span<const float> right) noexcept;

    [[nodiscard]] ImpulseCaptureState state() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] std::size_t capturedFrames() const noexcept;
    [[nodiscard]] ImpulseCaptureConfig activeConfig() const noexcept;
    [[nodiscard]] ImpulseCaptureResult copyLatest() const;

private:
    struct Slot final {
        std::uint64_t generation {};
        double sampleRate {};
        ImpulseCaptureConfig config;
        bool stoppedAtThreshold {};
        std::size_t frames {};
        std::vector<float> left;
        std::vector<float> right;
    };

    void finish(bool stoppedAtThreshold) noexcept;

    std::array<Slot, 3> slots_;
    std::atomic<double> sampleRate_ {};
    std::atomic<double> requestedLengthMs_ { 2'000.0 };
    std::atomic<double> requestedThresholdDb_ { -80.0 };
    std::atomic<float> requestedThresholdLinear_ { 0.0001F };
    std::atomic<bool> requestedMuteLiveInput_ { true };
    std::atomic<float> requestedImpulseLevel_ { 0.1F };
    std::atomic<std::uint64_t> requestedGeneration_ {};
    std::atomic<std::uint64_t> activeGeneration_ {};
    std::atomic<std::uint64_t> publishedGeneration_ {};
    std::atomic<int> publishedSlot_ { -1 };
    mutable std::atomic<int> readerSlot_ { -1 };
    std::atomic<ImpulseCaptureState> state_ { ImpulseCaptureState::idle };
    std::atomic<std::size_t> visibleFrames_ {};
    int writerSlot_ {};
    std::size_t maximumFrames_ {};
    std::size_t quietFrames_ {};
    std::size_t quietFramesRequired_ {};
    bool signalSeen_ {};
    float thresholdLinear_ {};
    ImpulseCaptureConfig activeConfig_;
};

} // namespace reverb::dsp
