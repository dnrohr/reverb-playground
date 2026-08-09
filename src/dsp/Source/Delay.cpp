#include <reverb/dsp/Delay.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace reverb::dsp {

void Delay::prepare(const double sampleRate, const double delayMilliseconds)
{
    if (sampleRate <= 0.0 || delayMilliseconds <= 0.0)
        throw std::invalid_argument("delay preparation requires positive sample rate and time");

    const auto samples = static_cast<std::size_t>(std::llround(sampleRate * delayMilliseconds / 1000.0));
    buffer_.assign(std::max<std::size_t>(1, samples), 0.0F);
    writeIndex_ = 0;
}

void Delay::reset() noexcept
{
    std::ranges::fill(buffer_, 0.0F);
    writeIndex_ = 0;
}

void Delay::process(const std::span<float> samples) noexcept
{
    if (buffer_.empty()) {
        std::ranges::fill(samples, 0.0F);
        return;
    }

    for (auto& sample : samples) {
        const auto input = sample;
        sample = readSample();
        writeSample(input);
    }
}

float Delay::readSample() const noexcept
{
    return buffer_.empty() ? 0.0F : buffer_[writeIndex_];
}

void Delay::writeSample(const float sample) noexcept
{
    if (buffer_.empty()) return;
    buffer_[writeIndex_] = sample;
    writeIndex_ = (writeIndex_ + 1) % buffer_.size();
}

std::size_t Delay::delaySamples() const noexcept
{
    return buffer_.size();
}

} // namespace reverb::dsp
