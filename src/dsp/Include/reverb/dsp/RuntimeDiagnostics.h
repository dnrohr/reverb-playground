#pragma once

#include <reverb/dsp/NumericalSafetyGuard.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace reverb::dsp {

enum class SafetyChannel : std::uint8_t { none, left, right };

struct RuntimeDiagnosticsSnapshot final {
    double sampleRate {};
    std::uint64_t processedBlocks {};
    float liveLoadPercent {};
    float peakLoadPercent {};
    std::uint64_t clippedSamples {};
    std::uint64_t clippedBlocks {};
    std::size_t delayLineCount {};
    std::size_t delayMemoryBytes {};
    std::uint64_t activeRevision { 1 };
    std::uint64_t safetyEventGeneration {};
    SafetyViolation lastViolation { SafetyViolation::none };
    SafetyChannel lastViolationChannel { SafetyChannel::none };
    std::size_t lastViolationSampleIndex {};
    std::uint64_t lastViolationRevision {};
    std::uint64_t recoveryCount {};
    bool safetyEventCoherent { true };
};

class RuntimeDiagnostics final {
public:
    static constexpr std::size_t estimatedScalarOperationsPerSample = 48;

    void prepare(double sampleRate, std::size_t delayLineCount, std::size_t delayMemoryBytes) noexcept;
    [[nodiscard]] std::uint64_t beginBlock() const noexcept;
    void endBlock(std::uint64_t startedNanoseconds, std::size_t frameCount, std::size_t clippedSamples) noexcept;
    [[nodiscard]] std::uint64_t advanceRevision() noexcept;
    void setActiveRevision(std::uint64_t revision) noexcept;
    [[nodiscard]] std::uint64_t activeRevision() const noexcept;
    void recordSafety(SafetyStatus status, SafetyChannel channel) noexcept;
    void recordRecovery() noexcept;
    [[nodiscard]] RuntimeDiagnosticsSnapshot snapshot() const noexcept;

private:
    std::atomic<double> sampleRate_ {};
    std::atomic<std::uint64_t> processedBlocks_ {};
    std::atomic<float> liveLoadPercent_ {};
    std::atomic<float> peakLoadPercent_ {};
    std::atomic<std::uint64_t> clippedSamples_ {};
    std::atomic<std::uint64_t> clippedBlocks_ {};
    std::atomic<std::size_t> delayLineCount_ {};
    std::atomic<std::size_t> delayMemoryBytes_ {};
    std::atomic<std::uint64_t> activeRevision_ { 1 };
    std::atomic<std::uint64_t> safetySequence_ {};
    std::atomic<std::uint64_t> safetyEventGeneration_ {};
    std::atomic<SafetyViolation> lastViolation_ { SafetyViolation::none };
    std::atomic<SafetyChannel> lastViolationChannel_ { SafetyChannel::none };
    std::atomic<std::size_t> lastViolationSampleIndex_ {};
    std::atomic<std::uint64_t> lastViolationRevision_ {};
    std::atomic<std::uint64_t> recoveryCount_ {};
    float smoothedLoadPercent_ {};
    float peakLoadPercentAudio_ {};
};

} // namespace reverb::dsp
