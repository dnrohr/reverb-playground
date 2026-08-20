#include <reverb/dsp/PitchShift.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace reverb::dsp {
namespace {

double wrappedPhase(const double phase) noexcept
{
    const auto wrapped = phase - std::floor(phase);
    return wrapped < 0.0 ? wrapped + 1.0 : wrapped;
}

} // namespace

void PitchShift::prepare(const double sampleRate, const PitchShiftParameters& parameters)
{
    const auto required = pitch_shift::preparedStorageSamples(sampleRate);
    if (required == 0
        || sampleRate < pitch_shift::minimumPreparationSampleRate
        || sampleRate > pitch_shift::maximumPreparationSampleRate) {
        throw std::invalid_argument("pitch shift preparation requires a supported finite sample rate");
    }
    ownedStorage_.assign(required, 0.0F);
    prepare(sampleRate, parameters, ownedStorage_);
}

void PitchShift::prepare(
    const double sampleRate,
    const PitchShiftParameters& parameters,
    const std::span<float> preparedStorage)
{
    const auto required = pitch_shift::preparedStorageSamples(sampleRate);
    if (required == 0
        || sampleRate < pitch_shift::minimumPreparationSampleRate
        || sampleRate > pitch_shift::maximumPreparationSampleRate) {
        throw std::invalid_argument("pitch shift preparation requires a supported finite sample rate");
    }
    if (preparedStorage.size() != required)
        throw std::invalid_argument("prepared pitch shift storage does not match the causal history budget");

    if (preparedStorage.data() != ownedStorage_.data()) ownedStorage_.clear();
    storage_ = preparedStorage;
    sampleRate_ = sampleRate;
    latencySamples_ = pitch_shift::reportedLatencySamples(sampleRate);
    transitionSamples_ = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(
        sampleRate * pitch_shift::parameterTransitionMilliseconds / 1'000.0)));

    const auto safe = sanitize(parameters);
    currentSemitones_ = safe.semitones;
    targetSemitones_ = safe.semitones;
    currentState_ = { safe.phaseCycles, safe.grainMilliseconds, safe.overlap,
        safe.direction, safe.phaseCycles };
    targetState_ = currentState_;
    reset();
}

void PitchShift::reset() noexcept
{
    std::ranges::fill(storage_, 0.0F);
    writeIndex_ = 0;
    transitionRemaining_ = 0;
    semitoneRampRemaining_ = 0;
    semitoneStep_ = 0.0;
    currentSemitones_ = targetSemitones_;
    currentState_ = targetState_;
    currentState_.phase = currentState_.resetPhaseCycles;
    targetState_.phase = targetState_.resetPhaseCycles;
}

void PitchShift::process(const std::span<float> samples) noexcept
{
    if (storage_.empty()) {
        std::ranges::fill(samples, 0.0F);
        return;
    }

    for (auto& sample : samples) {
        const auto input = std::isfinite(sample) ? sample : 0.0F;
        storage_[writeIndex_] = input;

        if (semitoneRampRemaining_ > 0) {
            currentSemitones_ += semitoneStep_;
            if (--semitoneRampRemaining_ == 0) currentSemitones_ = targetSemitones_;
        }
        const auto ratio = pitch_shift::ratioForSemitones(currentSemitones_);
        auto output = renderState(currentState_, ratio);
        if (transitionRemaining_ > 0) {
            const auto progress = 1.0 - static_cast<double>(transitionRemaining_)
                / static_cast<double>(transitionSamples_);
            output = std::lerp(output, renderState(targetState_, ratio), static_cast<float>(progress));
        }

        sample = std::clamp(output,
            -static_cast<float>(pitch_shift::maximumEqualPowerOutputMagnitude),
            static_cast<float>(pitch_shift::maximumEqualPowerOutputMagnitude));
        writeIndex_ = (writeIndex_ + 1) % storage_.size();
        advance(currentState_, sampleRate_);
        if (transitionRemaining_ > 0) {
            advance(targetState_, sampleRate_);
            if (--transitionRemaining_ == 0) currentState_ = targetState_;
        }
    }
}

void PitchShift::setParameters(const PitchShiftParameters& parameters) noexcept
{
    const auto safe = sanitize(parameters);
    if (safe.semitones != targetSemitones_) {
        targetSemitones_ = safe.semitones;
        semitoneRampRemaining_ = transitionSamples_;
        semitoneStep_ = (targetSemitones_ - currentSemitones_)
            / static_cast<double>(transitionSamples_);
    }

    if (safe.grainMilliseconds == targetState_.grainMilliseconds
        && safe.overlap == targetState_.overlap
        && safe.direction == targetState_.direction
        && safe.phaseCycles == targetState_.resetPhaseCycles) {
        return;
    }
    if (transitionRemaining_ > 0) currentState_ = targetState_;
    targetState_ = currentState_;
    targetState_.grainMilliseconds = safe.grainMilliseconds;
    targetState_.overlap = safe.overlap;
    targetState_.direction = safe.direction;
    targetState_.phase = safe.phaseCycles;
    targetState_.resetPhaseCycles = safe.phaseCycles;
    transitionRemaining_ = transitionSamples_;
}

void PitchShift::settleParameters() noexcept
{
    currentSemitones_ = targetSemitones_;
    semitoneRampRemaining_ = 0;
    semitoneStep_ = 0.0;
    currentState_ = targetState_;
    transitionRemaining_ = 0;
}

PitchShiftParameters PitchShift::parameters() const noexcept
{
    return { targetSemitones_, targetState_.grainMilliseconds,
        targetState_.overlap, targetState_.direction, targetState_.resetPhaseCycles };
}

std::size_t PitchShift::latencySamples() const noexcept { return latencySamples_; }
std::size_t PitchShift::storageSamples() const noexcept { return storage_.size(); }
bool PitchShift::isPrepared() const noexcept { return !storage_.empty(); }

PitchShiftParameters PitchShift::sanitize(const PitchShiftParameters& parameters) noexcept
{
    const auto semitones = std::isfinite(parameters.semitones)
        ? std::clamp(parameters.semitones,
            pitch_shift::minimumSemitones, pitch_shift::maximumSemitones)
        : pitch_shift::defaultSemitones;
    const auto grain = std::isfinite(parameters.grainMilliseconds)
        ? std::clamp(parameters.grainMilliseconds,
            pitch_shift::minimumGrainMilliseconds, pitch_shift::maximumGrainMilliseconds)
        : pitch_shift::defaultGrainMilliseconds;
    const auto overlap = std::isfinite(parameters.overlap)
        ? std::clamp(parameters.overlap, pitch_shift::minimumOverlap, pitch_shift::maximumOverlap)
        : pitch_shift::defaultOverlap;
    const auto phase = std::isfinite(parameters.phaseCycles)
        ? std::clamp(parameters.phaseCycles,
            pitch_shift::minimumPhaseCycles, pitch_shift::maximumPhaseCycles)
        : pitch_shift::defaultPhaseCycles;
    return { semitones, grain, overlap, parameters.direction, phase };
}

float PitchShift::renderState(const GrainState& state, const double ratio) const noexcept
{
    const auto grainSamples = state.grainMilliseconds * sampleRate_ / 1'000.0;
    const auto head0 = wrappedPhase(state.phase);
    const auto head1 = wrappedPhase(state.phase + 0.5);
    const auto delayFor = [&](const double phase) {
        if (state.direction == pitch_shift::GrainDirection::reverse)
            return static_cast<double>(latencySamples_) + (1.0 + ratio) * grainSamples * phase;
        const auto slope = (1.0 - ratio) * grainSamples;
        return static_cast<double>(latencySamples_)
            + (slope >= 0.0 ? slope * phase : -slope * (1.0 - phase));
    };

    const auto raw = std::sin(std::numbers::pi * head0);
    const auto dominance = raw * raw;
    const auto halfWidth = state.overlap * 0.5;
    const auto blend = std::clamp(
        (dominance - (0.5 - halfWidth)) / (2.0 * halfWidth), 0.0, 1.0);
    const auto angle = blend * std::numbers::pi * 0.5;
    const auto gain0 = static_cast<float>(std::sin(angle));
    const auto gain1 = static_cast<float>(std::cos(angle));
    constexpr auto equalPowerHeadroom = static_cast<float>(std::numbers::sqrt2 / 2.0);
    return equalPowerHeadroom * (gain0 * readFractional(delayFor(head0))
        + gain1 * readFractional(delayFor(head1)));
}

float PitchShift::readFractional(const double delaySamples) const noexcept
{
    const auto bounded = std::clamp(delaySamples, 1.0, static_cast<double>(storage_.size() - 2));
    const auto lower = static_cast<std::size_t>(std::floor(bounded));
    const auto fraction = static_cast<float>(bounded - static_cast<double>(lower));
    const auto first = storage_[(writeIndex_ + storage_.size() - lower) % storage_.size()];
    const auto second = storage_[(writeIndex_ + storage_.size() - lower - 1) % storage_.size()];
    return std::lerp(first, second, fraction);
}

void PitchShift::advance(GrainState& state, const double sampleRate) noexcept
{
    const auto grainSamples = std::max(1.0, state.grainMilliseconds * sampleRate / 1'000.0);
    state.phase = wrappedPhase(state.phase + 1.0 / grainSamples);
}

} // namespace reverb::dsp
