#include <reverb/dsp/Allpass.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace reverb::dsp {

void Allpass::prepare(const double sampleRate, const double delayMilliseconds, const float coefficient)
{
    if (sampleRate <= 0.0 || delayMilliseconds <= 0.0)
        throw std::invalid_argument("allpass preparation requires positive sample rate and time");
    if (std::abs(coefficient) >= 1.0F)
        throw std::invalid_argument("allpass coefficient magnitude must be below one");

    const auto samples = static_cast<std::size_t>(std::llround(sampleRate * delayMilliseconds / 1000.0));
    buffer_.assign(std::max<std::size_t>(1, samples), 0.0F);
    index_ = 0;
    coefficient_ = coefficient;
}

void Allpass::reset() noexcept
{
    std::ranges::fill(buffer_, 0.0F);
    index_ = 0;
}

void Allpass::process(const std::span<float> samples) noexcept
{
    if (buffer_.empty()) {
        std::ranges::fill(samples, 0.0F);
        return;
    }

    for (auto& sample : samples) {
        const auto delayed = buffer_[index_];
        const auto output = delayed - coefficient_ * sample;
        buffer_[index_] = sample + coefficient_ * output;
        sample = output;
        index_ = (index_ + 1) % buffer_.size();
    }
}

float Allpass::coefficient() const noexcept
{
    return coefficient_;
}

} // namespace reverb::dsp
