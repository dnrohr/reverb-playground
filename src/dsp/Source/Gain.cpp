#include <reverb/dsp/Gain.h>

#include <algorithm>

namespace reverb::dsp {

void Gain::setLinear(const float value) noexcept
{
    linear_ = value;
}

float Gain::getLinear() const noexcept
{
    return linear_;
}

void Gain::process(const std::span<float> samples) const noexcept
{
    std::ranges::for_each(samples, [gain = linear_](float& sample) {
        sample *= gain;
    });
}

} // namespace reverb::dsp
