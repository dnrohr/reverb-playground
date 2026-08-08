#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace reverb::dsp {

class Allpass final {
public:
    void prepare(double sampleRate, double delayMilliseconds, float coefficient);
    void reset() noexcept;
    void process(std::span<float> samples) noexcept;

    [[nodiscard]] float coefficient() const noexcept;

private:
    std::vector<float> buffer_;
    std::size_t index_ {};
    float coefficient_ { 0.5F };
};

} // namespace reverb::dsp
