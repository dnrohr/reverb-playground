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

// Minimax-style low-order approximations on [0, pi/2]. The overlap gains do
// not need libm accuracy, but preserving the endpoints is important for
// click-free dual-grain hand-offs.
void fastSinCosQuarterTurn(const double angle, float& sine, float& cosine) noexcept
{
    const auto x2 = angle * angle;
    const auto sinValue = angle * (1.0 + x2 * (-1.666665710e-1
        + x2 * (8.333017292e-3 + x2 * -1.980661520e-4)));
    const auto mirrored = std::numbers::pi * 0.5 - angle;
    const auto y2 = mirrored * mirrored;
    const auto cosValue = mirrored * (1.0 + y2 * (-1.666665710e-1
        + y2 * (8.333017292e-3 + y2 * -1.980661520e-4)));
    sine = static_cast<float>(sinValue);
    cosine = static_cast<float>(cosValue);
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
    currentRatio_ = pitch_shift::ratioForSemitones(safe.semitones);
    targetRatio_ = currentRatio_;
    ratioMultiplier_ = 1.0;
    currentState_ = { safe.phaseCycles, safe.grainMilliseconds, safe.overlap,
        safe.direction, safe.phaseCycles };
    targetState_ = currentState_;
    configureState(currentState_);
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
    currentRatio_ = targetRatio_;
    ratioMultiplier_ = 1.0;
    currentState_ = targetState_;
    currentState_.phase = currentState_.resetPhaseCycles;
    targetState_.phase = targetState_.resetPhaseCycles;
    configureState(currentState_);
    configureState(targetState_);
}

void PitchShift::process(const std::span<float> samples) noexcept
{
    if (storage_.empty()) {
        std::ranges::fill(samples, 0.0F);
        return;
    }

    for (auto& sample : samples) sample = processSample(sample);
}

void PitchShift::processModulated(
    const std::span<float> samples,
    const std::span<const double> semitones,
    const std::span<const double> grainMilliseconds,
    const std::span<const double> overlap,
    const PitchShiftParameters& constants) noexcept
{
    if (storage_.empty()
        || (!semitones.empty() && semitones.size() < samples.size())
        || (!grainMilliseconds.empty() && grainMilliseconds.size() < samples.size())
        || (!overlap.empty() && overlap.size() < samples.size())) {
        std::ranges::fill(samples, 0.0F);
        return;
    }
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = processSampleModulated(samples[index], {
            semitones.empty() ? constants.semitones : semitones[index],
            grainMilliseconds.empty() ? constants.grainMilliseconds : grainMilliseconds[index],
            overlap.empty() ? constants.overlap : overlap[index],
            constants.direction,
            constants.phaseCycles,
        });
    }
}

float PitchShift::processSampleModulated(
    const float sample, const PitchShiftParameters& parameters) noexcept
{
    if (storage_.empty()) return 0.0F;
    setParameters(parameters);
    return processSample(sample);
}

float PitchShift::processSample(const float sample) noexcept
{
    const auto input = std::isfinite(sample) ? sample : 0.0F;
    storage_[writeIndex_] = input;
    if (semitoneRampRemaining_ > 0) {
        currentSemitones_ += semitoneStep_;
        currentRatio_ *= ratioMultiplier_;
        if (--semitoneRampRemaining_ == 0) {
            currentSemitones_ = targetSemitones_;
            currentRatio_ = targetRatio_;
        }
    }
    auto output = renderState(currentState_, currentRatio_);
    if (transitionRemaining_ > 0) {
        const auto progress = 1.0 - static_cast<double>(transitionRemaining_)
            / static_cast<double>(transitionSamples_);
        output = std::lerp(output, renderState(targetState_, currentRatio_), static_cast<float>(progress));
    }
    if (++writeIndex_ == storage_.size()) writeIndex_ = 0;
    advance(currentState_);
    if (transitionRemaining_ > 0) {
        advance(targetState_);
        if (--transitionRemaining_ == 0) currentState_ = targetState_;
    }
    return std::clamp(output,
        -static_cast<float>(pitch_shift::maximumEqualPowerOutputMagnitude),
        static_cast<float>(pitch_shift::maximumEqualPowerOutputMagnitude));
}

void PitchShift::setParameters(const PitchShiftParameters& parameters) noexcept
{
    const auto safe = sanitize(parameters);
    if (safe.semitones != targetSemitones_) {
        targetSemitones_ = safe.semitones;
        targetRatio_ = pitch_shift::ratioForSemitones(targetSemitones_);
        semitoneRampRemaining_ = transitionSamples_;
        semitoneStep_ = (targetSemitones_ - currentSemitones_)
            / static_cast<double>(transitionSamples_);
        ratioMultiplier_ = std::pow(targetRatio_ / currentRatio_,
            1.0 / static_cast<double>(transitionSamples_));
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
    configureState(targetState_);
    transitionRemaining_ = transitionSamples_;
}

void PitchShift::settleParameters() noexcept
{
    currentSemitones_ = targetSemitones_;
    currentRatio_ = targetRatio_;
    ratioMultiplier_ = 1.0;
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
    const auto grainSamples = state.grainSamples;
    const auto head0 = state.phase;
    const auto head1 = state.phase < 0.5 ? state.phase + 0.5 : state.phase - 0.5;
    const auto delayFor = [&](const double phase) {
        if (state.direction == pitch_shift::GrainDirection::reverse)
            return static_cast<double>(latencySamples_) + (1.0 + ratio) * grainSamples * phase;
        const auto slope = (1.0 - ratio) * grainSamples;
        return static_cast<double>(latencySamples_)
            + (slope >= 0.0 ? slope * phase : -slope * (1.0 - phase));
    };

    const auto dominance = 0.5 - 0.5 * state.windowCos;
    const auto halfWidth = state.overlap * 0.5;
    const auto blend = std::clamp(
        (dominance - (0.5 - halfWidth)) / (2.0 * halfWidth), 0.0, 1.0);
    const auto angle = blend * std::numbers::pi * 0.5;
    float gain0 {};
    float gain1 {};
    fastSinCosQuarterTurn(angle, gain0, gain1);
    constexpr auto equalPowerHeadroom = static_cast<float>(std::numbers::sqrt2 / 2.0);
    return equalPowerHeadroom * (gain0 * readFractional(delayFor(head0))
        + gain1 * readFractional(delayFor(head1)));
}

float PitchShift::readFractional(const double delaySamples) const noexcept
{
    const auto bounded = std::clamp(delaySamples, 1.0, static_cast<double>(storage_.size() - 2));
    const auto lower = static_cast<std::size_t>(bounded);
    const auto fraction = static_cast<float>(bounded - static_cast<double>(lower));
    auto firstIndex = writeIndex_ + storage_.size() - lower;
    if (firstIndex >= storage_.size()) firstIndex -= storage_.size();
    auto secondIndex = firstIndex == 0 ? storage_.size() - 1 : firstIndex - 1;
    const auto first = storage_[firstIndex];
    const auto second = storage_[secondIndex];
    return std::lerp(first, second, fraction);
}

void PitchShift::configureState(GrainState& state) const noexcept
{
    state.phase = wrappedPhase(state.phase);
    state.grainSamples = std::max(1.0, state.grainMilliseconds * sampleRate_ / 1'000.0);
    state.phaseIncrement = 1.0 / state.grainSamples;
    const auto phaseAngle = 2.0 * std::numbers::pi * state.phase;
    const auto rotationAngle = 2.0 * std::numbers::pi * state.phaseIncrement;
    state.windowCos = std::cos(phaseAngle);
    state.windowSin = std::sin(phaseAngle);
    state.rotationCos = std::cos(rotationAngle);
    state.rotationSin = std::sin(rotationAngle);
}

void PitchShift::advance(GrainState& state) noexcept
{
    state.phase += state.phaseIncrement;
    if (state.phase >= 1.0) state.phase -= 1.0;
    const auto cosine = state.windowCos * state.rotationCos - state.windowSin * state.rotationSin;
    state.windowSin = state.windowSin * state.rotationCos + state.windowCos * state.rotationSin;
    state.windowCos = cosine;
}

} // namespace reverb::dsp
