#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace reverb::dsp {

class Delay final {
public:
    void prepare(double sampleRate, double delayMilliseconds);
    void prepare(double sampleRate, double delayMilliseconds, std::span<float> preparedStorage);
    void reset() noexcept;
    void process(std::span<float> samples) noexcept;
    [[nodiscard]] float readSample() const noexcept;
    void writeSample(float sample) noexcept;

    [[nodiscard]] std::size_t delaySamples() const noexcept;

private:
    std::vector<float> ownedBuffer_;
    std::span<float> buffer_;
    std::size_t writeIndex_ {};
};

} // namespace reverb::dsp
