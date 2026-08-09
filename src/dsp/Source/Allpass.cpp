#include <reverb/dsp/Allpass.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace reverb::dsp {

void Allpass::prepare(
    const double sampleRate,
    const double delayMilliseconds,
    const float coefficient,
    const double maximumDelayMilliseconds)
{
    if (sampleRate <= 0.0 || delayMilliseconds <= 0.0)
        throw std::invalid_argument("allpass preparation requires positive sample rate and time");
    if (std::abs(coefficient) >= 1.0F)
        throw std::invalid_argument("allpass coefficient magnitude must be below one");

    sampleRate_ = sampleRate;
    const auto capacity = static_cast<std::size_t>(
        std::ceil(sampleRate * maximumDelayMilliseconds / 1000.0)) + 1;
    ownedBuffer_.assign(std::max<std::size_t>(2, capacity), 0.0F);
    buffer_ = ownedBuffer_;
    prepare(sampleRate, delayMilliseconds, coefficient, maximumDelayMilliseconds, buffer_);
}

void Allpass::prepare(
    const double sampleRate,
    const double delayMilliseconds,
    const float coefficient,
    const double maximumDelayMilliseconds,
    const std::span<float> preparedStorage)
{
    if (sampleRate <= 0.0 || delayMilliseconds <= 0.0)
        throw std::invalid_argument("allpass preparation requires positive sample rate and time");
    if (std::abs(coefficient) >= 1.0F)
        throw std::invalid_argument("allpass coefficient magnitude must be below one");
    const auto capacity = std::max<std::size_t>(2,
        static_cast<std::size_t>(std::ceil(sampleRate * maximumDelayMilliseconds / 1000.0)) + 1);
    if (preparedStorage.size() != capacity)
        throw std::invalid_argument("prepared allpass storage does not match the declared maximum time");
    if (preparedStorage.data() != buffer_.data()) ownedBuffer_.clear();
    buffer_ = preparedStorage;
    std::ranges::fill(buffer_, 0.0F);
    writeIndex_ = 0;
    delayMilliseconds_ = delayMilliseconds;
    delaySamples_ = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::llround(sampleRate * delayMilliseconds / 1000.0)), 1, buffer_.size() - 1);
    oldDelaySamples_ = delaySamples_;
    targetDelaySamples_ = delaySamples_;
    crossfadeSamples_ = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(sampleRate * 0.02)));
    crossfadeRemaining_ = 0;
    coefficient_ = coefficient;
    targetCoefficient_ = coefficient;
    coefficientSmoothing_ = 1.0F - static_cast<float>(std::exp(-1.0 / (sampleRate * 0.02)));
}

void Allpass::reset() noexcept
{
    std::ranges::fill(buffer_, 0.0F);
    writeIndex_ = 0;
}

void Allpass::settleParameters() noexcept
{
    coefficient_ = targetCoefficient_;
    delaySamples_ = targetDelaySamples_;
    oldDelaySamples_ = targetDelaySamples_;
    crossfadeRemaining_ = 0;
}

void Allpass::process(const std::span<float> samples) noexcept
{
    if (buffer_.empty()) {
        std::ranges::fill(samples, 0.0F);
        return;
    }

    for (auto& sample : samples) {
        coefficient_ += (targetCoefficient_ - coefficient_) * coefficientSmoothing_;
        const auto read = [this](const std::size_t delay) {
            return buffer_[(writeIndex_ + buffer_.size() - delay) % buffer_.size()];
        };
        auto delayed = read(delaySamples_);
        if (crossfadeRemaining_ > 0) {
            const auto progress = 1.0F - static_cast<float>(crossfadeRemaining_)
                / static_cast<float>(crossfadeSamples_);
            delayed = std::lerp(read(oldDelaySamples_), read(targetDelaySamples_), progress);
        }
        const auto output = delayed - coefficient_ * sample;
        buffer_[writeIndex_] = sample + coefficient_ * output;
        sample = output;
        writeIndex_ = (writeIndex_ + 1) % buffer_.size();
        if (crossfadeRemaining_ > 0 && --crossfadeRemaining_ == 0)
            delaySamples_ = targetDelaySamples_;
    }
}

double Allpass::delayMilliseconds() const noexcept { return delayMilliseconds_; }
std::size_t Allpass::storageSamples() const noexcept { return buffer_.size(); }

void Allpass::setCoefficient(const float coefficient) noexcept
{
    targetCoefficient_ = std::clamp(coefficient, -0.95F, 0.95F);
}

void Allpass::setDelayMilliseconds(const double delayMilliseconds) noexcept
{
    delayMilliseconds_ = std::clamp(delayMilliseconds, 1000.0 / sampleRate_,
        static_cast<double>(buffer_.size() - 1) * 1000.0 / sampleRate_);
    const auto samples = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::llround(sampleRate_ * delayMilliseconds_ / 1000.0)),
        1,
        buffer_.size() - 1);
    if (samples == targetDelaySamples_)
        return;
    oldDelaySamples_ = crossfadeRemaining_ == 0 ? delaySamples_ : targetDelaySamples_;
    targetDelaySamples_ = samples;
    crossfadeRemaining_ = crossfadeSamples_;
}

float Allpass::coefficient() const noexcept
{
    return coefficient_;
}

} // namespace reverb::dsp
