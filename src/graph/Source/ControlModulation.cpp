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

double shapeControl(
    const double input, const ControlCurveFamily family, const double amount,
    const double exponent, const ModulationPolarity polarity) noexcept
{
    if (family == ControlCurveFamily::linear)
        return input;
    if (family == ControlCurveFamily::power) {
        const auto finiteExponent = std::isfinite(exponent) ? std::clamp(exponent, 0.1, 8.0) : 1.0;
        return polarity == ModulationPolarity::bipolar
            ? std::copysign(std::pow(std::abs(input), finiteExponent), input)
            : std::pow(input, finiteExponent);
    }

    const auto finiteAmount = std::isfinite(amount) ? std::clamp(amount, -8.0, 8.0) : 0.0;
    const auto unitInput = polarity == ModulationPolarity::bipolar ? (input + 1.0) * 0.5 : input;
    const auto shaped = std::abs(finiteAmount) < 1.0e-9
        ? unitInput
        : std::expm1(finiteAmount * unitInput) / std::expm1(finiteAmount);
    return polarity == ModulationPolarity::bipolar ? shaped * 2.0 - 1.0 : shaped;
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

double mapControlValue(
    const double input, const ControlCurveFamily curveFamily, const double curveAmount,
    const double exponent, const double scale, const double offset,
    const ModulationPolarity inputPolarity, const double clampMinimum,
    const double clampMaximum) noexcept
{
    const auto finiteInput = std::isfinite(input) ? input : 0.0;
    const auto normalized = inputPolarity == ModulationPolarity::bipolar
        ? std::clamp(finiteInput, -1.0, 1.0)
        : std::clamp(finiteInput, 0.0, 1.0);
    const auto shaped = shapeControl(normalized, curveFamily, curveAmount, exponent, inputPolarity);
    const auto finiteScale = std::isfinite(scale) ? scale : 0.0;
    const auto finiteOffset = std::isfinite(offset) ? offset : 0.0;
    const auto finiteMinimum = std::isfinite(clampMinimum) ? clampMinimum : -1.0;
    const auto finiteMaximum = std::isfinite(clampMaximum) ? clampMaximum : 1.0;
    if (finiteMinimum >= finiteMaximum)
        return std::clamp(finiteOffset, -1.0, 1.0);
    return std::clamp(shaped * finiteScale + finiteOffset, finiteMinimum, finiteMaximum);
}

ControlMappingRange mappedControlRange(
    const double scale, const double offset, const ModulationPolarity inputPolarity) noexcept
{
    const auto lowerInput = inputPolarity == ModulationPolarity::bipolar ? -1.0 : 0.0;
    const auto first = mapControlValue(lowerInput, scale, offset, inputPolarity);
    const auto second = mapControlValue(1.0, scale, offset, inputPolarity);
    return { std::min(first, second), std::max(first, second) };
}

ControlMappingRange mappedControlRange(
    const ControlCurveFamily curveFamily, const double curveAmount, const double exponent,
    const double scale, const double offset, const ModulationPolarity inputPolarity,
    const double clampMinimum, const double clampMaximum) noexcept
{
    const auto lowerInput = inputPolarity == ModulationPolarity::bipolar ? -1.0 : 0.0;
    const auto first = mapControlValue(
        lowerInput, curveFamily, curveAmount, exponent, scale, offset,
        inputPolarity, clampMinimum, clampMaximum);
    const auto second = mapControlValue(
        1.0, curveFamily, curveAmount, exponent, scale, offset,
        inputPolarity, clampMinimum, clampMaximum);
    return { std::min(first, second), std::max(first, second) };
}

} // namespace reverb::graph
