#include <reverb/dsp/BarrReference.h>

#include <reverb/dsp/Sum.h>

#include <algorithm>

namespace reverb::dsp {

void BarrReference::prepare(const double sampleRate)
{
    inputGain_.setLinear(0.5F);
    inputFilter_.prepare(sampleRate, 7'000.0);
    diffuserOne_.prepare(sampleRate, 4.31, 0.5F);
    diffuserTwo_.prepare(sampleRate, 7.13, 0.5F);
    tankOne_.prepare(sampleRate, 13.73, 0.5F);
    tankTwo_.prepare(sampleRate, 19.91, -0.5F);
    leftTap_.prepare(sampleRate, 29.71, 0.5F);
    rightTap_.prepare(sampleRate, 37.11, 0.5F);
}

void BarrReference::reset() noexcept
{
    inputFilter_.reset();
    diffuserOne_.reset();
    diffuserTwo_.reset();
    tankOne_.reset();
    tankTwo_.reset();
    leftTap_.reset();
    rightTap_.reset();
}

void BarrReference::process(
    const std::span<const float> inputLeft,
    const std::span<const float> inputRight,
    const std::span<float> outputLeft,
    const std::span<float> outputRight,
    const float impulse) noexcept
{
    const auto count = std::min({ inputLeft.size(), inputRight.size(), outputLeft.size(), outputRight.size() });
    const auto left = outputLeft.first(count);
    const auto right = outputRight.first(count);

    Sum::process(inputLeft.first(count), inputRight.first(count), left);
    if (!left.empty())
        left.front() += impulse;
    inputGain_.process(left);
    inputFilter_.process(left);
    diffuserOne_.process(left);
    diffuserTwo_.process(left);
    tankOne_.process(left);
    tankTwo_.process(left);

    std::ranges::copy(left, right.begin());
    leftTap_.process(left);
    rightTap_.process(right);
    std::fill(outputLeft.begin() + static_cast<std::ptrdiff_t>(count), outputLeft.end(), 0.0F);
    std::fill(outputRight.begin() + static_cast<std::ptrdiff_t>(count), outputRight.end(), 0.0F);
}

} // namespace reverb::dsp
