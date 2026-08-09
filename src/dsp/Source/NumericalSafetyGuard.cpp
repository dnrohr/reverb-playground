#include <reverb/dsp/NumericalSafetyGuard.h>

#include <algorithm>
#include <cmath>

namespace reverb::dsp {

NumericalSafetyGuard::NumericalSafetyGuard(const float maximumAbsoluteSample) noexcept
    : maximumAbsoluteSample_(maximumAbsoluteSample)
{
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
        const auto violation = !std::isfinite(sample)
            ? SafetyViolation::nonFinite
            : (absolute > maximumAbsoluteSample_ ? SafetyViolation::runawayLevel : SafetyViolation::none);

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
}

} // namespace reverb::dsp
