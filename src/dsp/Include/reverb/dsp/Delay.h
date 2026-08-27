#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace reverb::dsp {

class Delay final {
public:
    void prepare(double sampleRate, double delayMilliseconds);
    void prepare(double sampleRate, double delayMilliseconds, std::span<float> preparedStorage);
    void prepareModulated(double sampleRate, double delayMilliseconds,
        double maximumDelayMilliseconds, std::span<float> preparedStorage);
    void reset() noexcept;
    void process(std::span<float> samples) noexcept;
    void processModulated(std::span<float> samples, std::span<const double> delayMilliseconds) noexcept;
    [[nodiscard]] float readSample() const noexcept;
    [[nodiscard]] float readSampleModulated(double delayMilliseconds) noexcept;
    void writeSample(float sample) noexcept;
    void setDelayMilliseconds(double delayMilliseconds) noexcept;

    [[nodiscard]] std::size_t delaySamples() const noexcept;
    [[nodiscard]] double delayMilliseconds() const noexcept;

private:
    std::vector<float> ownedBuffer_;
    std::span<float> buffer_;
    std::size_t writeIndex_ {};
    double sampleRate_ {};
    double delaySamples_ { 1.0 };
    bool fractional_ {};
};

} // namespace reverb::dsp
