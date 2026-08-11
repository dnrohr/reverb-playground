#include <reverb/dsp/HoldGate.h>

#include <algorithm>
#include <cmath>

namespace reverb::dsp {
namespace {

std::size_t millisecondsToSamples(const double sampleRate, const double milliseconds) noexcept
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0
        || !std::isfinite(milliseconds) || milliseconds <= 0.0)
        return 0;
    return static_cast<std::size_t>(std::llround(sampleRate * milliseconds / 1'000.0));
}

} // namespace

void HoldGate::prepare(
    const double sampleRate, const double threshold, const double attackMilliseconds,
    const double holdMilliseconds, const double releaseMilliseconds) noexcept
{
    threshold_ = std::isfinite(threshold) ? std::clamp(static_cast<float>(threshold), 0.0F, 1.0F) : 0.5F;
    const auto attackSamples = millisecondsToSamples(sampleRate, attackMilliseconds);
    const auto releaseSamples = millisecondsToSamples(sampleRate, releaseMilliseconds);
    attackIncrement_ = attackSamples == 0 ? 1.0F : 1.0F / static_cast<float>(attackSamples);
    releaseIncrement_ = releaseSamples == 0 ? 1.0F : 1.0F / static_cast<float>(releaseSamples);
    holdSamples_ = millisecondsToSamples(sampleRate, holdMilliseconds);
    reset();
}

void HoldGate::reset() noexcept
{
    gain_ = 0.0F;
    holdRemaining_ = 0;
}

float HoldGate::processSample(const float input, const float control) noexcept
{
    const auto boundedControl = std::isfinite(control) ? std::clamp(control, 0.0F, 1.0F) : 0.0F;
    if (boundedControl > threshold_) {
        holdRemaining_ = holdSamples_;
        gain_ = std::min(1.0F, gain_ + attackIncrement_);
    } else if (holdRemaining_ > 0) {
        --holdRemaining_;
    } else {
        gain_ = std::max(0.0F, gain_ - releaseIncrement_);
    }
    return input * gain_;
}

void HoldGate::process(
    const std::span<const float> input, const std::span<const float> control,
    const std::span<float> output) noexcept
{
    const auto count = std::min({ input.size(), control.size(), output.size() });
    for (std::size_t index = 0; index < count; ++index)
        output[index] = processSample(input[index], control[index]);
    std::ranges::fill(output.subspan(count), 0.0F);
}

} // namespace reverb::dsp
