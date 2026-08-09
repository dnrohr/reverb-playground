#pragma once

#include <reverb/dsp/Allpass.h>
#include <reverb/dsp/Gain.h>
#include <reverb/dsp/OnePoleLowPass.h>
#include <reverb/dsp/BarrReferenceRuntime.h>
#include <reverb/dsp/EnergyTelemetry.h>

#include <span>

namespace reverb::dsp {

class BarrReference final {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void resetForMeasurement() noexcept;
    void setParameterTarget(BarrParameterId id, double value) noexcept;
    void process(
        std::span<const float> inputLeft,
        std::span<const float> inputRight,
        std::span<float> outputLeft,
        std::span<float> outputRight,
        float impulse = 0.0F,
        bool muteLiveInput = false,
        EnergyTelemetry* telemetry = nullptr) noexcept;

private:
    Gain inputGain_;
    OnePoleLowPass inputFilter_;
    Allpass diffuserOne_;
    Allpass diffuserTwo_;
    Allpass tankOne_;
    Allpass tankTwo_;
    Allpass leftTap_;
    Allpass rightTap_;
};

} // namespace reverb::dsp
