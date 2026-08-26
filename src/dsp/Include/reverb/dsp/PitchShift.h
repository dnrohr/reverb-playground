#pragma once

#include <reverb/dsp/PitchShiftContract.h>

#include <cstddef>
#include <span>
#include <vector>

namespace reverb::dsp {

struct PitchShiftParameters final {
    double semitones { pitch_shift::defaultSemitones };
    double grainMilliseconds { pitch_shift::defaultGrainMilliseconds };
    double overlap { pitch_shift::defaultOverlap };
    pitch_shift::GrainDirection direction { pitch_shift::GrainDirection::forward };
    double phaseCycles { pitch_shift::defaultPhaseCycles };
};

class PitchShift final {
public:
    void prepare(double sampleRate, const PitchShiftParameters& parameters = {});
    void prepare(double sampleRate, const PitchShiftParameters& parameters,
        std::span<float> preparedStorage);
    void reset() noexcept;
    void process(std::span<float> samples) noexcept;
    void setParameters(const PitchShiftParameters& parameters) noexcept;
    void settleParameters() noexcept;

    [[nodiscard]] PitchShiftParameters parameters() const noexcept;
    [[nodiscard]] std::size_t latencySamples() const noexcept;
    [[nodiscard]] std::size_t storageSamples() const noexcept;
    [[nodiscard]] bool isPrepared() const noexcept;

private:
    struct GrainState final {
        double phase {};
        double grainMilliseconds { pitch_shift::defaultGrainMilliseconds };
        double overlap { pitch_shift::defaultOverlap };
        pitch_shift::GrainDirection direction { pitch_shift::GrainDirection::forward };
        double resetPhaseCycles { pitch_shift::defaultPhaseCycles };
        double grainSamples { 1.0 };
        double phaseIncrement {};
        double windowCos { 1.0 };
        double windowSin {};
        double rotationCos { 1.0 };
        double rotationSin {};
    };

    std::vector<float> ownedStorage_;
    std::span<float> storage_;
    std::size_t writeIndex_ {};
    std::size_t latencySamples_ {};
    std::size_t transitionSamples_ { 1 };
    std::size_t transitionRemaining_ {};
    std::size_t semitoneRampRemaining_ {};
    double sampleRate_ {};
    double currentSemitones_ { pitch_shift::defaultSemitones };
    double targetSemitones_ { pitch_shift::defaultSemitones };
    double semitoneStep_ {};
    double currentRatio_ { 1.0 };
    double targetRatio_ { 1.0 };
    double ratioMultiplier_ { 1.0 };
    GrainState currentState_;
    GrainState targetState_;

    [[nodiscard]] static PitchShiftParameters sanitize(const PitchShiftParameters& parameters) noexcept;
    [[nodiscard]] float renderState(const GrainState& state, double ratio) const noexcept;
    [[nodiscard]] float readFractional(double delaySamples) const noexcept;
    void configureState(GrainState& state) const noexcept;
    static void advance(GrainState& state) noexcept;
};

} // namespace reverb::dsp
