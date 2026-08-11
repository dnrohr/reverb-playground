#pragma once

#include <cstddef>
#include <span>

namespace reverb::dsp {

class EnvelopeFollower final {
public:
    void prepare(double sampleRate, double attackMilliseconds, double releaseMilliseconds) noexcept;
    void reset() noexcept;
    [[nodiscard]] float processSample(float input) noexcept;
    void process(std::span<const float> input, std::span<float> output) noexcept;

private:
    float attackCoefficient_ {};
    float releaseCoefficient_ {};
    float envelope_ {};
};

} // namespace reverb::dsp
