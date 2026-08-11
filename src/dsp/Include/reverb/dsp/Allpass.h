#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace reverb::dsp {

class Allpass final {
public:
    void prepare(double sampleRate, double delayMilliseconds, float coefficient, double maximumDelayMilliseconds = 100.0);
    void prepare(double sampleRate, double delayMilliseconds, float coefficient,
        double maximumDelayMilliseconds, std::span<float> preparedStorage);
    void reset() noexcept;
    void settleParameters() noexcept;
    void process(std::span<float> samples) noexcept;
    void processModulated(std::span<float> samples,
        std::span<const double> delayMilliseconds,
        std::span<const double> coefficients) noexcept;
    [[nodiscard]] float processSampleModulated(
        float sample, double delayMilliseconds, double coefficient) noexcept;

    [[nodiscard]] float coefficient() const noexcept;
    [[nodiscard]] double delayMilliseconds() const noexcept;
    [[nodiscard]] std::size_t storageSamples() const noexcept;
    void setCoefficient(float coefficient) noexcept;
    void setDelayMilliseconds(double delayMilliseconds) noexcept;

private:
    std::vector<float> ownedBuffer_;
    std::span<float> buffer_;
    std::size_t writeIndex_ {};
    std::size_t delaySamples_ { 1 };
    std::size_t oldDelaySamples_ { 1 };
    std::size_t targetDelaySamples_ { 1 };
    std::size_t crossfadeSamples_ { 1 };
    std::size_t crossfadeRemaining_ {};
    double sampleRate_ {};
    double delayMilliseconds_ { 1.0 };
    float coefficient_ { 0.5F };
    float targetCoefficient_ { 0.5F };
    float coefficientSmoothing_ { 1.0F };

    [[nodiscard]] float readFractional(double delaySamples) const noexcept;
};

} // namespace reverb::dsp
