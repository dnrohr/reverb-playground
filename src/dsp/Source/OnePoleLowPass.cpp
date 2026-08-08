#include <reverb/dsp/OnePoleLowPass.h>

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace reverb::dsp {

void OnePoleLowPass::prepare(const double sampleRate, const double cutoffHertz)
{
    if (sampleRate <= 0.0 || cutoffHertz <= 0.0 || cutoffHertz >= sampleRate * 0.5)
        throw std::invalid_argument("low-pass cutoff must lie between zero and Nyquist");

    feedback_ = static_cast<float>(std::exp(-2.0 * std::numbers::pi * cutoffHertz / sampleRate));
    feed_ = 1.0F - feedback_;
    state_ = 0.0F;
}

void OnePoleLowPass::reset() noexcept
{
    state_ = 0.0F;
}

void OnePoleLowPass::process(const std::span<float> samples) noexcept
{
    for (auto& sample : samples) {
        state_ = feed_ * sample + feedback_ * state_;
        sample = state_;
    }
}

} // namespace reverb::dsp
