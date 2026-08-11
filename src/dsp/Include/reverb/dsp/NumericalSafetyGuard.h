#pragma once

#include <cstddef>
#include <span>

namespace reverb::dsp {

enum class SafetyViolation {
    none,
    nonFinite,
    runawayLevel
};

struct SafetyStatus {
    SafetyViolation violation { SafetyViolation::none };
    std::size_t sampleIndex {};
    std::size_t clippedSamples {};
    float peakAbsoluteSample {};
};

class NumericalSafetyGuard final {
public:
    static constexpr float defaultMaximumAbsoluteSample = 16.0F;
    static constexpr float defaultSustainedAbsoluteSample = 4.0F;
    static constexpr double defaultSustainedMilliseconds = 50.0;

    explicit NumericalSafetyGuard(
        float maximumAbsoluteSample = defaultMaximumAbsoluteSample,
        float sustainedAbsoluteSample = defaultSustainedAbsoluteSample,
        double sustainedMilliseconds = defaultSustainedMilliseconds) noexcept;

    void prepare(double sampleRate) noexcept;
    [[nodiscard]] SafetyStatus inspectAndMute(std::span<float> samples) noexcept;
    [[nodiscard]] bool isMuted() const noexcept;
    void reset() noexcept;

private:
    float maximumAbsoluteSample_;
    float sustainedAbsoluteSample_;
    double sustainedMilliseconds_;
    std::size_t sustainedSampleLimit_ { 1 };
    std::size_t consecutiveOverThreshold_ {};
    bool muted_ {};
};

} // namespace reverb::dsp
