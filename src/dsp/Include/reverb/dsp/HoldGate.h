#pragma once

#include <cstddef>
#include <span>

namespace reverb::dsp {

class HoldGate final {
public:
    void prepare(
        double sampleRate, double threshold, double attackMilliseconds,
        double holdMilliseconds, double releaseMilliseconds) noexcept;
    void reset() noexcept;
    [[nodiscard]] float processSample(float input, float control) noexcept;
    void process(
        std::span<const float> input, std::span<const float> control,
        std::span<float> output) noexcept;
    [[nodiscard]] float gain() const noexcept { return gain_; }

private:
    float threshold_ { 0.5F };
    float attackIncrement_ { 1.0F };
    float releaseIncrement_ { 1.0F };
    std::size_t holdSamples_ {};
    std::size_t holdRemaining_ {};
    float gain_ {};
};

} // namespace reverb::dsp
