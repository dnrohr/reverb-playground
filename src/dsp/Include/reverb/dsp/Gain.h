#pragma once

#include <span>

namespace reverb::dsp {

class Gain final {
public:
    void setLinear(float value) noexcept;
    [[nodiscard]] float getLinear() const noexcept;
    void process(std::span<float> samples) const noexcept;

private:
    float linear_ { 1.0F };
};

} // namespace reverb::dsp
