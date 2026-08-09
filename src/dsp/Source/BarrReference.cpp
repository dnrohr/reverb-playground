#include <reverb/dsp/BarrReference.h>
#include <reverb/dsp/BarrReferenceRuntime.h>

#include <reverb/dsp/Sum.h>

#include <algorithm>

namespace reverb::dsp {

void BarrReference::prepare(const double sampleRate)
{
    const auto prepareAllpass = [sampleRate](Allpass& allpass, const std::string_view nodeId) {
        allpass.prepare(
            sampleRate,
            barrReferenceParameter(nodeId, "delay"),
            static_cast<float>(barrReferenceParameter(nodeId, "coefficient")));
    };
    inputGain_.prepare(sampleRate, static_cast<float>(barrReferenceParameter("sum", "gain")));
    inputFilter_.prepare(sampleRate, barrReferenceParameter("input-filter", "cutoff"));
    prepareAllpass(diffuserOne_, "diffuser-1");
    prepareAllpass(diffuserTwo_, "diffuser-2");
    prepareAllpass(tankOne_, "tank-1");
    prepareAllpass(tankTwo_, "tank-2");
    prepareAllpass(leftTap_, "left-tap");
    prepareAllpass(rightTap_, "right-tap");
}

void BarrReference::setParameterTarget(const BarrParameterId id, const double value) noexcept
{
    switch (id) {
    case BarrParameterId::sumGain: inputGain_.setTargetLinear(static_cast<float>(value)); break;
    case BarrParameterId::filterCutoff: inputFilter_.setCutoffHertz(value); break;
    case BarrParameterId::diffuserOneDelay: diffuserOne_.setDelayMilliseconds(value); break;
    case BarrParameterId::diffuserOneCoefficient: diffuserOne_.setCoefficient(static_cast<float>(value)); break;
    case BarrParameterId::diffuserTwoDelay: diffuserTwo_.setDelayMilliseconds(value); break;
    case BarrParameterId::diffuserTwoCoefficient: diffuserTwo_.setCoefficient(static_cast<float>(value)); break;
    case BarrParameterId::tankOneDelay: tankOne_.setDelayMilliseconds(value); break;
    case BarrParameterId::tankOneCoefficient: tankOne_.setCoefficient(static_cast<float>(value)); break;
    case BarrParameterId::tankTwoDelay: tankTwo_.setDelayMilliseconds(value); break;
    case BarrParameterId::tankTwoCoefficient: tankTwo_.setCoefficient(static_cast<float>(value)); break;
    case BarrParameterId::leftTapDelay: leftTap_.setDelayMilliseconds(value); break;
    case BarrParameterId::leftTapCoefficient: leftTap_.setCoefficient(static_cast<float>(value)); break;
    case BarrParameterId::rightTapDelay: rightTap_.setDelayMilliseconds(value); break;
    case BarrParameterId::rightTapCoefficient: rightTap_.setCoefficient(static_cast<float>(value)); break;
    case BarrParameterId::count: break;
    }
}

void BarrReference::reset() noexcept
{
    inputFilter_.reset();
    diffuserOne_.reset();
    diffuserTwo_.reset();
    tankOne_.reset();
    tankTwo_.reset();
    leftTap_.reset();
    rightTap_.reset();
}

void BarrReference::resetForMeasurement() noexcept
{
    reset();
    inputGain_.settleTarget();
    inputFilter_.settleParameters();
    diffuserOne_.settleParameters();
    diffuserTwo_.settleParameters();
    tankOne_.settleParameters();
    tankTwo_.settleParameters();
    leftTap_.settleParameters();
    rightTap_.settleParameters();
}

void BarrReference::process(
    const std::span<const float> inputLeft,
    const std::span<const float> inputRight,
    const std::span<float> outputLeft,
    const std::span<float> outputRight,
    const float impulse,
    const bool muteLiveInput,
    EnergyTelemetry* const telemetry) noexcept
{
    const auto count = std::min({ inputLeft.size(), inputRight.size(), outputLeft.size(), outputRight.size() });
    const auto left = outputLeft.first(count);
    const auto right = outputRight.first(count);
    const auto instrument = telemetry != nullptr && telemetry->beginBlock();
    if (instrument)
        telemetry->observeStereo(BarrEnergyLane::input, inputLeft.first(count), inputRight.first(count));

    if (muteLiveInput)
        std::ranges::fill(left, 0.0F);
    else
        Sum::process(inputLeft.first(count), inputRight.first(count), left);
    if (!left.empty())
        left.front() += impulse;
    inputGain_.process(left);
    if (instrument)
        telemetry->observeMono(BarrEnergyLane::sum, left);
    inputFilter_.process(left);
    if (instrument)
        telemetry->observeMono(BarrEnergyLane::inputFilter, left);
    diffuserOne_.process(left);
    if (instrument)
        telemetry->observeMono(BarrEnergyLane::diffuserOne, left);
    diffuserTwo_.process(left);
    if (instrument)
        telemetry->observeMono(BarrEnergyLane::diffuserTwo, left);
    tankOne_.process(left);
    if (instrument)
        telemetry->observeMono(BarrEnergyLane::tankOne, left);
    tankTwo_.process(left);
    if (instrument)
        telemetry->observeMono(BarrEnergyLane::tankTwo, left);

    std::ranges::copy(left, right.begin());
    leftTap_.process(left);
    if (instrument)
        telemetry->observeMono(BarrEnergyLane::leftTap, left);
    rightTap_.process(right);
    if (instrument) {
        telemetry->observeMono(BarrEnergyLane::rightTap, right);
        telemetry->observeStereo(BarrEnergyLane::output, left, right);
        telemetry->endBlock(count);
    }
    std::fill(outputLeft.begin() + static_cast<std::ptrdiff_t>(count), outputLeft.end(), 0.0F);
    std::fill(outputRight.begin() + static_cast<std::ptrdiff_t>(count), outputRight.end(), 0.0F);
}

} // namespace reverb::dsp
