#include <reverb/dsp/NumericalSafetyGuard.h>

#include <algorithm>
#include <cmath>

namespace reverb::dsp {

NumericalSafetyGuard::NumericalSafetyGuard(
    const float maximumAbsoluteSample,
    const float sustainedAbsoluteSample,
    const double sustainedMilliseconds) noexcept
    : maximumAbsoluteSample_(std::max(0.0F, maximumAbsoluteSample)),
      sustainedAbsoluteSample_(std::clamp(sustainedAbsoluteSample, 0.0F, maximumAbsoluteSample_)),
      sustainedMilliseconds_(std::max(0.0, sustainedMilliseconds))
{
}

void NumericalSafetyGuard::prepare(const double sampleRate) noexcept
{
    const auto samples = sampleRate > 0.0
        ? std::ceil(sampleRate * sustainedMilliseconds_ / 1'000.0)
        : 1.0;
    sustainedSampleLimit_ = static_cast<std::size_t>(std::max(1.0, samples));
    reset();
}

SafetyStatus NumericalSafetyGuard::inspectAndMute(const std::span<float> samples) noexcept
{
    if (muted_) {
        std::ranges::fill(samples, 0.0F);
        return {};
    }

    SafetyStatus status;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto sample = samples[index];
        const auto absolute = std::abs(sample);
        if (std::isfinite(absolute)) {
            status.peakAbsoluteSample = std::max(status.peakAbsoluteSample, absolute);
            if (absolute > 1.0F)
                ++status.clippedSamples;
        }
        auto violation = SafetyViolation::none;
        if (!std::isfinite(sample)) {
            violation = SafetyViolation::nonFinite;
        } else if (absolute > maximumAbsoluteSample_) {
            violation = SafetyViolation::runawayLevel;
        } else {
            consecutiveOverThreshold_ = absolute > sustainedAbsoluteSample_
                ? consecutiveOverThreshold_ + 1
                : 0;
            if (consecutiveOverThreshold_ >= sustainedSampleLimit_)
                violation = SafetyViolation::runawayLevel;
        }

        if (violation != SafetyViolation::none) {
            muted_ = true;
            std::ranges::fill(samples, 0.0F);
            status.violation = violation;
            status.sampleIndex = index;
            return status;
        }
    }

    return status;
}

bool NumericalSafetyGuard::isMuted() const noexcept
{
    return muted_;
}

void NumericalSafetyGuard::reset() noexcept
{
    muted_ = false;
    consecutiveOverThreshold_ = 0;
}

} // namespace reverb::dsp
