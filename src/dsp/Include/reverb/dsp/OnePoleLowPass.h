#pragma once

#include <span>

namespace reverb::dsp {

class OnePoleLowPass final {
public:
    void prepare(double sampleRate, double cutoffHertz);
    void reset() noexcept;
    void settleParameters() noexcept;
    void process(std::span<float> samples) noexcept;
    void setCutoffHertz(double cutoffHertz) noexcept;
    [[nodiscard]] double cutoffHertz() const noexcept;

private:
    double sampleRate_ {};
    double cutoffHertz_ {};
    float feed_ { 1.0F };
    float feedback_ {};
    float targetFeed_ { 1.0F };
    float targetFeedback_ {};
    float smoothing_ { 1.0F };
    float state_ {};
};

} // namespace reverb::dsp
