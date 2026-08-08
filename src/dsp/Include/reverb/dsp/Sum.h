#pragma once

#include <span>

namespace reverb::dsp {

class Sum final {
public:
    static void process(std::span<const float> left, std::span<const float> right, std::span<float> output) noexcept;
};

} // namespace reverb::dsp
