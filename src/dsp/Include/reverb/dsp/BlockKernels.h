#pragma once

#include <span>

namespace reverb::dsp::block {

[[nodiscard]] bool usesSimd() noexcept;
void copy(std::span<const float> input, std::span<float> output) noexcept;
void gain(std::span<float> samples, float linear) noexcept;
void copyGain(std::span<const float> input, std::span<float> output, float linear) noexcept;
void sumScaled(
    std::span<const float> left, std::span<const float> right,
    std::span<float> output, float linear = 1.0F) noexcept;
void weightedSum(
    std::span<const float> left, std::span<const float> right,
    std::span<float> output, float leftScale, float rightScale,
    float outputScale = 1.0F) noexcept;

} // namespace reverb::dsp::block
