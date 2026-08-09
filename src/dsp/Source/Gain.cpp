#include <reverb/dsp/Gain.h>

#include <algorithm>
#include <cmath>

void reverb::dsp::Gain::prepare(const double sampleRate, const float initialValue, const double smoothingMilliseconds)
{
    rampSamples_ = std::max<std::size_t>(1, static_cast<std::size_t>(
        std::llround(sampleRate * smoothingMilliseconds / 1000.0)));
    setLinear(initialValue);
}

namespace reverb::dsp {

void Gain::setLinear(const float value) noexcept
{
    linear_ = value;
    target_ = value;
    remaining_ = 0;
    step_ = 0.0F;
}

void Gain::setTargetLinear(const float value) noexcept
{
    if (value == target_)
        return;
    target_ = value;
    remaining_ = rampSamples_;
    step_ = (target_ - linear_) / static_cast<float>(remaining_);
}

void Gain::settleTarget() noexcept
{
    linear_ = target_;
    remaining_ = 0;
    step_ = 0.0F;
}

float Gain::getLinear() const noexcept
{
    return linear_;
}

void Gain::process(const std::span<float> samples) noexcept
{
    for (auto& sample : samples) {
        if (remaining_ > 0) {
            linear_ += step_;
            if (--remaining_ == 0)
                linear_ = target_;
        }
        sample *= linear_;
    }
}

} // namespace reverb::dsp
