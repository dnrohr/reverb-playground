#include <reverb/dsp/BlockKernels.h>

#include <algorithm>
#include <cstring>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define REVERB_BLOCK_KERNELS_SSE2 1
#else
#define REVERB_BLOCK_KERNELS_SSE2 0
#endif

namespace reverb::dsp::block {
namespace {

std::size_t commonSize(
    const std::span<const float> left, const std::span<const float> right,
    const std::span<float> output) noexcept
{
    return std::min({ left.size(), right.size(), output.size() });
}

} // namespace

bool usesSimd() noexcept { return REVERB_BLOCK_KERNELS_SSE2 != 0; }

void copy(const std::span<const float> input, const std::span<float> output) noexcept
{
    const auto count = std::min(input.size(), output.size());
    if (count > 0 && input.data() != output.data())
        std::memmove(output.data(), input.data(), count * sizeof(float));
    std::fill(output.begin() + static_cast<std::ptrdiff_t>(count), output.end(), 0.0F);
}

void gain(const std::span<float> samples, const float linear) noexcept
{
    std::size_t index = 0;
#if REVERB_BLOCK_KERNELS_SSE2
    const auto scale = _mm_set1_ps(linear);
    for (; index + 4 <= samples.size(); index += 4) {
        const auto value = _mm_loadu_ps(samples.data() + index);
        _mm_storeu_ps(samples.data() + index, _mm_mul_ps(value, scale));
    }
#endif
    for (; index < samples.size(); ++index) samples[index] *= linear;
}

void copyGain(
    const std::span<const float> input, const std::span<float> output,
    const float linear) noexcept
{
    const auto count = std::min(input.size(), output.size());
    std::size_t index = 0;
#if REVERB_BLOCK_KERNELS_SSE2
    const auto scale = _mm_set1_ps(linear);
    for (; index + 4 <= count; index += 4) {
        const auto value = _mm_loadu_ps(input.data() + index);
        _mm_storeu_ps(output.data() + index, _mm_mul_ps(value, scale));
    }
#endif
    for (; index < count; ++index) output[index] = input[index] * linear;
    std::fill(output.begin() + static_cast<std::ptrdiff_t>(count), output.end(), 0.0F);
}

void sumScaled(
    const std::span<const float> left, const std::span<const float> right,
    const std::span<float> output, const float linear) noexcept
{
    const auto count = commonSize(left, right, output);
    std::size_t index = 0;
#if REVERB_BLOCK_KERNELS_SSE2
    const auto scale = _mm_set1_ps(linear);
    for (; index + 4 <= count; index += 4) {
        const auto a = _mm_loadu_ps(left.data() + index);
        const auto b = _mm_loadu_ps(right.data() + index);
        _mm_storeu_ps(output.data() + index, _mm_mul_ps(_mm_add_ps(a, b), scale));
    }
#endif
    for (; index < count; ++index) output[index] = (left[index] + right[index]) * linear;
    std::fill(output.begin() + static_cast<std::ptrdiff_t>(count), output.end(), 0.0F);
}

void weightedSum(
    const std::span<const float> left, const std::span<const float> right,
    const std::span<float> output, const float leftScale, const float rightScale,
    const float outputScale) noexcept
{
    const auto count = commonSize(left, right, output);
    std::size_t index = 0;
#if REVERB_BLOCK_KERNELS_SSE2
    const auto leftGain = _mm_set1_ps(leftScale);
    const auto rightGain = _mm_set1_ps(rightScale);
    const auto finalGain = _mm_set1_ps(outputScale);
    for (; index + 4 <= count; index += 4) {
        const auto a = _mm_mul_ps(_mm_loadu_ps(left.data() + index), leftGain);
        const auto b = _mm_mul_ps(_mm_loadu_ps(right.data() + index), rightGain);
        _mm_storeu_ps(output.data() + index, _mm_mul_ps(_mm_add_ps(a, b), finalGain));
    }
#endif
    for (; index < count; ++index)
        output[index] = (left[index] * leftScale + right[index] * rightScale) * outputScale;
    std::fill(output.begin() + static_cast<std::ptrdiff_t>(count), output.end(), 0.0F);
}

} // namespace reverb::dsp::block
