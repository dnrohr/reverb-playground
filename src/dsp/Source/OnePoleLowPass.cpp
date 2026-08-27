#include <reverb/dsp/OnePoleLowPass.h>

#include <cmath>
#include <algorithm>
#include <numbers>
#include <stdexcept>

namespace reverb::dsp {

void OnePoleLowPass::prepare(const double sampleRate, const double cutoffHertz)
{
    if (sampleRate <= 0.0 || cutoffHertz <= 0.0 || cutoffHertz >= sampleRate * 0.5)
        throw std::invalid_argument("low-pass cutoff must lie between zero and Nyquist");

    sampleRate_ = sampleRate;
    cutoffHertz_ = cutoffHertz;
    feedback_ = static_cast<float>(std::exp(-2.0 * std::numbers::pi * cutoffHertz / sampleRate));
    feed_ = 1.0F - feedback_;
    targetFeedback_ = feedback_;
    targetFeed_ = feed_;
    smoothing_ = 1.0F - static_cast<float>(std::exp(-1.0 / (sampleRate * 0.02)));
    state_ = 0.0F;
}

void OnePoleLowPass::reset() noexcept
{
    state_ = 0.0F;
}

void OnePoleLowPass::settleParameters() noexcept
{
    feedback_ = targetFeedback_;
    feed_ = targetFeed_;
}

void OnePoleLowPass::process(const std::span<float> samples) noexcept
{
    for (auto& sample : samples) {
        feedback_ += (targetFeedback_ - feedback_) * smoothing_;
        feed_ += (targetFeed_ - feed_) * smoothing_;
        state_ = feed_ * sample + feedback_ * state_;
        sample = state_;
    }
}

void OnePoleLowPass::processScaled(
    const std::span<const float> input, const std::span<float> output,
    const float scale) noexcept
{
    const auto count = std::min(input.size(), output.size());
    for (std::size_t index = 0; index < count; ++index) {
        feedback_ += (targetFeedback_ - feedback_) * smoothing_;
        feed_ += (targetFeed_ - feed_) * smoothing_;
        state_ = feed_ * (input[index] * scale) + feedback_ * state_;
        output[index] = state_;
    }
    std::fill(output.begin() + static_cast<std::ptrdiff_t>(count), output.end(), 0.0F);
}

void OnePoleLowPass::processSummedScaled(
    const std::span<const float> left, const std::span<const float> right,
    const std::span<float> output, const float scale) noexcept
{
    const auto count = std::min({ left.size(), right.size(), output.size() });
    for (std::size_t index = 0; index < count; ++index) {
        feedback_ += (targetFeedback_ - feedback_) * smoothing_;
        feed_ += (targetFeed_ - feed_) * smoothing_;
        state_ = feed_ * ((left[index] + right[index]) * scale) + feedback_ * state_;
        output[index] = state_;
    }
    std::fill(output.begin() + static_cast<std::ptrdiff_t>(count), output.end(), 0.0F);
}

void OnePoleLowPass::processOutputScaled(
    const std::span<const float> input, const std::span<float> output,
    const float scale) noexcept
{
    const auto count = std::min(input.size(), output.size());
    for (std::size_t index = 0; index < count; ++index) {
        feedback_ += (targetFeedback_ - feedback_) * smoothing_;
        feed_ += (targetFeed_ - feed_) * smoothing_;
        state_ = feed_ * input[index] + feedback_ * state_;
        output[index] = state_ * scale;
    }
    std::fill(output.begin() + static_cast<std::ptrdiff_t>(count), output.end(), 0.0F);
}

void OnePoleLowPass::processModulated(
    const std::span<float> samples,
    const std::span<const double> cutoffHertz) noexcept
{
    if (cutoffHertz.size() < samples.size()) {
        std::ranges::fill(samples, 0.0F);
        return;
    }
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = processSampleModulated(samples[index], cutoffHertz[index]);
    }
}

float OnePoleLowPass::processSampleModulated(
    const float sample, const double cutoffHertz) noexcept
{
    setCutoffHertz(cutoffHertz);
    feedback_ += (targetFeedback_ - feedback_) * smoothing_;
    feed_ += (targetFeed_ - feed_) * smoothing_;
    state_ = feed_ * sample + feedback_ * state_;
    return state_;
}

void OnePoleLowPass::setCutoffHertz(const double cutoffHertz) noexcept
{
    cutoffHertz_ = std::clamp(cutoffHertz, 1.0, sampleRate_ * 0.499);
    targetFeedback_ = static_cast<float>(
        std::exp(-2.0 * std::numbers::pi * cutoffHertz_ / sampleRate_));
    targetFeed_ = 1.0F - targetFeedback_;
}

double OnePoleLowPass::cutoffHertz() const noexcept { return cutoffHertz_; }

} // namespace reverb::dsp
