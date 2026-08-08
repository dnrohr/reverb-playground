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

    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto sample = samples[index];
        const auto violation = !std::isfinite(sample)
            ? SafetyViolation::nonFinite
            : (std::abs(sample) > maximumAbsoluteSample_ ? SafetyViolation::runawayLevel : SafetyViolation::none);

        if (violation != SafetyViolation::none) {
            muted_ = true;
            std::ranges::fill(samples, 0.0F);
            return { violation, index };
        }
    }

    return {};
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
