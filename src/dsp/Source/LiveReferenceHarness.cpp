#include <reverb/dsp/LiveReferenceHarness.h>

#include <algorithm>

namespace reverb::dsp {

static_assert(std::atomic<double>::is_always_lock_free);

LiveReferenceHarness::LiveReferenceHarness()
{
    for (std::size_t index = 0; index < parameterTargets_.size(); ++index) {
        const auto id = static_cast<BarrParameterId>(index);
        parameterTargets_[index].store(
            barrReferenceParameterDefinition(id).value, std::memory_order_relaxed);
    }
}

void LiveReferenceHarness::prepare(const double sampleRate)
{
    reference_.prepare(sampleRate);
    leftGuard_.reset();
    rightGuard_.reset();
    safetyLatched_.store(false, std::memory_order_release);
    sampleRate_.store(sampleRate, std::memory_order_release);
}

void LiveReferenceHarness::reset() noexcept
{
    reference_.reset();
    leftGuard_.reset();
    rightGuard_.reset();
    safetyLatched_.store(false, std::memory_order_release);
}

void LiveReferenceHarness::process(
    const std::span<const float> inputLeft,
    const std::span<const float> inputRight,
    const std::span<float> outputLeft,
    const std::span<float> outputRight) noexcept
{
    if (safetyResetPending_.exchange(false, std::memory_order_acq_rel))
        reset();

    if (sampleRate_.load(std::memory_order_acquire) <= 0.0
        || emergencyMuted_.load(std::memory_order_acquire)
        || safetyLatched_.load(std::memory_order_acquire)) {
        std::ranges::fill(outputLeft, 0.0F);
        std::ranges::fill(outputRight, 0.0F);
        return;
    }

    for (std::size_t index = 0; index < parameterTargets_.size(); ++index) {
        reference_.setParameterTarget(
            static_cast<BarrParameterId>(index),
            parameterTargets_[index].load(std::memory_order_relaxed));
    }

    const auto impulse = impulsePending_.exchange(false, std::memory_order_acq_rel) ? 1.0F : 0.0F;
    reference_.process(inputLeft, inputRight, outputLeft, outputRight, impulse);
    const auto gain = masterGain_.load(std::memory_order_relaxed);
    for (auto& sample : outputLeft)
        sample *= gain;
    for (auto& sample : outputRight)
        sample *= gain;

    const auto leftStatus = leftGuard_.inspectAndMute(outputLeft);
    const auto rightStatus = rightGuard_.inspectAndMute(outputRight);
    if (leftStatus.violation != SafetyViolation::none || rightStatus.violation != SafetyViolation::none) {
        safetyLatched_.store(true, std::memory_order_release);
        std::ranges::fill(outputLeft, 0.0F);
        std::ranges::fill(outputRight, 0.0F);
    }
}

void LiveReferenceHarness::triggerImpulse() noexcept
{
    impulsePending_.store(true, std::memory_order_release);
}

void LiveReferenceHarness::setMasterGain(const float linearGain) noexcept
{
    masterGain_.store(std::clamp(linearGain, 0.0F, 1.0F), std::memory_order_release);
}

void LiveReferenceHarness::setEmergencyMuted(const bool muted) noexcept
{
    emergencyMuted_.store(muted, std::memory_order_release);
}

void LiveReferenceHarness::requestSafetyReset() noexcept
{
    safetyResetPending_.store(true, std::memory_order_release);
}

void LiveReferenceHarness::setRuntimeParameter(const BarrParameterId id, const double value) noexcept
{
    const auto& definition = barrReferenceParameterDefinition(id);
    parameterTargets_[static_cast<std::size_t>(id)].store(
        std::clamp(value, definition.minimum, definition.maximum),
        std::memory_order_release);
}

float LiveReferenceHarness::masterGain() const noexcept { return masterGain_.load(std::memory_order_acquire); }
bool LiveReferenceHarness::isEmergencyMuted() const noexcept { return emergencyMuted_.load(std::memory_order_acquire); }
bool LiveReferenceHarness::isSafetyLatched() const noexcept { return safetyLatched_.load(std::memory_order_acquire); }
double LiveReferenceHarness::sampleRate() const noexcept { return sampleRate_.load(std::memory_order_acquire); }
double LiveReferenceHarness::runtimeParameter(const BarrParameterId id) const noexcept
{
    return parameterTargets_[static_cast<std::size_t>(id)].load(std::memory_order_acquire);
}

} // namespace reverb::dsp
