#pragma once

#include <span>

namespace reverb::dsp {

class OnePoleLowPass final {
public:
    void prepare(double sampleRate, double cutoffHertz);
    void reset() noexcept;
    void process(std::span<float> samples) noexcept;

private:
    float feed_ { 1.0F };
    float feedback_ {};
    float state_ {};
};

} // namespace reverb::dsp
