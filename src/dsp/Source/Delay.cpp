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
    ownedBuffer_.assign(std::max<std::size_t>(1, samples), 0.0F);
    buffer_ = ownedBuffer_;
    writeIndex_ = 0;
    sampleRate_ = sampleRate;
    delaySamples_ = static_cast<double>(buffer_.size());
    fractional_ = false;
}

void Delay::prepare(
    const double sampleRate, const double delayMilliseconds, const std::span<float> preparedStorage)
{
    if (sampleRate <= 0.0 || delayMilliseconds <= 0.0)
        throw std::invalid_argument("delay preparation requires positive sample rate and time");
    const auto samples = std::max<std::size_t>(1,
        static_cast<std::size_t>(std::llround(sampleRate * delayMilliseconds / 1000.0)));
    if (preparedStorage.size() != samples)
        throw std::invalid_argument("prepared delay storage does not match the requested time");
    ownedBuffer_.clear();
    buffer_ = preparedStorage;
    std::ranges::fill(buffer_, 0.0F);
    writeIndex_ = 0;
    sampleRate_ = sampleRate;
    delaySamples_ = static_cast<double>(buffer_.size());
    fractional_ = false;
}

void Delay::prepareModulated(
    const double sampleRate,
    const double delayMilliseconds,
    const double maximumDelayMilliseconds,
    const std::span<float> preparedStorage)
{
    if (sampleRate <= 0.0 || delayMilliseconds <= 0.0 || maximumDelayMilliseconds < delayMilliseconds)
        throw std::invalid_argument("modulated delay preparation requires positive ordered times");
    const auto capacity = std::max<std::size_t>(2,
        static_cast<std::size_t>(std::ceil(sampleRate * maximumDelayMilliseconds / 1000.0)) + 1);
    if (preparedStorage.size() != capacity)
        throw std::invalid_argument("prepared modulated delay storage does not match the declared maximum time");
    ownedBuffer_.clear();
    buffer_ = preparedStorage;
    std::ranges::fill(buffer_, 0.0F);
    writeIndex_ = 0;
    sampleRate_ = sampleRate;
    fractional_ = true;
    setDelayMilliseconds(delayMilliseconds);
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

void Delay::processModulated(
    const std::span<float> samples, const std::span<const double> delayMilliseconds) noexcept
{
    if (delayMilliseconds.size() < samples.size()) {
        std::ranges::fill(samples, 0.0F);
        return;
    }
    for (std::size_t index = 0; index < samples.size(); ++index) {
        setDelayMilliseconds(delayMilliseconds[index]);
        const auto input = samples[index];
        samples[index] = readSample();
        writeSample(input);
    }
}

float Delay::readSample() const noexcept
{
    if (buffer_.empty()) return 0.0F;
    if (!fractional_) return buffer_[writeIndex_];
    const auto lower = static_cast<std::size_t>(std::floor(delaySamples_));
    const auto fraction = static_cast<float>(delaySamples_ - static_cast<double>(lower));
    const auto first = buffer_[(writeIndex_ + buffer_.size() - lower) % buffer_.size()];
    const auto second = buffer_[(writeIndex_ + buffer_.size() - lower - 1) % buffer_.size()];
    return std::lerp(first, second, fraction);
}

void Delay::writeSample(const float sample) noexcept
{
    if (buffer_.empty()) return;
    buffer_[writeIndex_] = sample;
    writeIndex_ = (writeIndex_ + 1) % buffer_.size();
}

std::size_t Delay::delaySamples() const noexcept
{
    return static_cast<std::size_t>(std::llround(delaySamples_));
}

double Delay::delayMilliseconds() const noexcept
{
    return sampleRate_ > 0.0 ? delaySamples_ * 1000.0 / sampleRate_ : 0.0;
}

void Delay::setDelayMilliseconds(const double delayMilliseconds) noexcept
{
    if (!fractional_ || buffer_.size() < 2 || sampleRate_ <= 0.0) return;
    const auto finite = std::isfinite(delayMilliseconds) ? delayMilliseconds : 1000.0 / sampleRate_;
    delaySamples_ = std::clamp(
        finite * sampleRate_ / 1000.0, 1.0, static_cast<double>(buffer_.size() - 1));
}

} // namespace reverb::dsp
