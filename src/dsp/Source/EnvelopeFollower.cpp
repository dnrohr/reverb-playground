#include <reverb/dsp/EnvelopeFollower.h>

#include <algorithm>
#include <cmath>

namespace reverb::dsp {
namespace {

float coefficient(const double sampleRate, const double milliseconds) noexcept
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0
        || !std::isfinite(milliseconds) || milliseconds <= 0.0)
        return 0.0F;
    return static_cast<float>(std::exp(-1.0 / (sampleRate * milliseconds / 1'000.0)));
}

} // namespace

void EnvelopeFollower::prepare(
    const double sampleRate, const double attackMilliseconds, const double releaseMilliseconds) noexcept
{
    attackCoefficient_ = coefficient(sampleRate, attackMilliseconds);
    releaseCoefficient_ = coefficient(sampleRate, releaseMilliseconds);
    reset();
}

void EnvelopeFollower::reset() noexcept
{
    envelope_ = 0.0F;
}

float EnvelopeFollower::processSample(const float input) noexcept
{
    const auto level = std::isfinite(input) ? std::clamp(std::abs(input), 0.0F, 1.0F) : 0.0F;
    const auto coefficientValue = level > envelope_ ? attackCoefficient_ : releaseCoefficient_;
    envelope_ = level + coefficientValue * (envelope_ - level);
    if (!std::isfinite(envelope_)) envelope_ = 0.0F;
    return std::clamp(envelope_, 0.0F, 1.0F);
}

void EnvelopeFollower::process(const std::span<const float> input, const std::span<float> output) noexcept
{
    const auto count = std::min(input.size(), output.size());
    for (std::size_t index = 0; index < count; ++index)
        output[index] = processSample(input[index]);
    std::ranges::fill(output.subspan(count), 0.0F);
}

} // namespace reverb::dsp
