#include <reverb/graph/ControlModulation.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace reverb::graph {
namespace {

double wrapPhase(const double phase) noexcept
{
    if (!std::isfinite(phase))
        return 0.0;
    const auto wrapped = phase - std::floor(phase);
    return wrapped < 0.0 ? wrapped + 1.0 : wrapped;
}

} // namespace

void ControlLfo::prepare(const double controlSampleRate) noexcept
{
    controlSampleRate_ = std::isfinite(controlSampleRate) && controlSampleRate > 0.0
        ? controlSampleRate
        : 1'000.0;
}

void ControlLfo::configure(
    const double frequencyHz,
    const double phaseCycles,
    const LfoWaveform waveform,
    const LfoRunMode runMode) noexcept
{
    frequencyHz_ = std::isfinite(frequencyHz)
        ? std::clamp(frequencyHz, 0.0, controlSampleRate_ * 0.5)
        : 0.0;
    phaseOffset_ = wrapPhase(phaseCycles);
    waveform_ = waveform;
    runMode_ = runMode;
}

void ControlLfo::reset() noexcept
{
    if (runMode_ == LfoRunMode::restart)
        restart();
}

void ControlLfo::restart() noexcept
{
    phase_ = phaseOffset_;
}

double ControlLfo::next() noexcept
{
    const auto value = waveform_ == LfoWaveform::sine
        ? std::sin(phase_ * 2.0 * std::numbers::pi)
        : 1.0 - 4.0 * std::abs(phase_ - 0.5);
    phase_ = wrapPhase(phase_ + frequencyHz_ / controlSampleRate_);
    return value;
}

double mapControlValue(
    const double input,
    const double scale,
    const double offset,
    const ModulationPolarity inputPolarity) noexcept
{
    const auto finiteInput = std::isfinite(input) ? input : 0.0;
    const auto finiteScale = std::isfinite(scale) ? scale : 0.0;
    const auto finiteOffset = std::isfinite(offset) ? offset : 0.0;
    const auto normalized = inputPolarity == ModulationPolarity::bipolar
        ? std::clamp(finiteInput, -1.0, 1.0)
        : std::clamp(finiteInput, 0.0, 1.0);
    return std::clamp(normalized * finiteScale + finiteOffset, -1.0, 1.0);
}

ControlMappingRange mappedControlRange(
    const double scale, const double offset, const ModulationPolarity inputPolarity) noexcept
{
    const auto lowerInput = inputPolarity == ModulationPolarity::bipolar ? -1.0 : 0.0;
    const auto first = mapControlValue(lowerInput, scale, offset, inputPolarity);
    const auto second = mapControlValue(1.0, scale, offset, inputPolarity);
    return { std::min(first, second), std::max(first, second) };
}

} // namespace reverb::graph
