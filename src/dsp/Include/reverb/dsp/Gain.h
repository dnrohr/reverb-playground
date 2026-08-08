#pragma once

#include <span>

namespace reverb::dsp {

class Gain final {
public:
    void prepare(double sampleRate, float initialValue, double smoothingMilliseconds = 20.0);
    void setLinear(float value) noexcept;
    void setTargetLinear(float value) noexcept;
    [[nodiscard]] float getLinear() const noexcept;
    void process(std::span<float> samples) noexcept;

private:
    float linear_ { 1.0F };
    float target_ { 1.0F };
    float step_ {};
    std::size_t rampSamples_ { 1 };
    std::size_t remaining_ {};
};

} // namespace reverb::dsp
