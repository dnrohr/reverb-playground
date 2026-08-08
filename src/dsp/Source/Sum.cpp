#include <reverb/dsp/Sum.h>

#include <algorithm>

namespace reverb::dsp {

void Sum::process(
    const std::span<const float> left,
    const std::span<const float> right,
    const std::span<float> output) noexcept
{
    const auto count = std::min({ left.size(), right.size(), output.size() });
    for (std::size_t index = 0; index < count; ++index)
        output[index] = left[index] + right[index];
    std::fill(output.begin() + static_cast<std::ptrdiff_t>(count), output.end(), 0.0F);
}

} // namespace reverb::dsp
