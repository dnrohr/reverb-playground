#pragma once

#include <reverb/graph/GraphDocument.h>

#include <cstddef>

namespace reverb::graph {

enum class LfoWaveform {
    sine,
    triangle,
};

enum class LfoRunMode {
    freeRun,
    restart,
};

class ControlLfo final {
public:
    void prepare(double controlSampleRate) noexcept;
    void configure(double frequencyHz, double phaseCycles, LfoWaveform waveform, LfoRunMode runMode) noexcept;
    void reset() noexcept;
    void restart() noexcept;
    [[nodiscard]] double next() noexcept;
    [[nodiscard]] double phase() const noexcept { return phase_; }

private:
    double controlSampleRate_ { 1'000.0 };
    double frequencyHz_ { 1.0 };
    double phaseOffset_ { 0.0 };
    double phase_ { 0.0 };
    LfoWaveform waveform_ { LfoWaveform::sine };
    LfoRunMode runMode_ { LfoRunMode::freeRun };
};

struct ControlMappingRange final {
    double minimum { 0.0 };
    double maximum { 0.0 };
};

[[nodiscard]] double mapControlValue(
    double input, double scale, double offset, ModulationPolarity inputPolarity) noexcept;

[[nodiscard]] ControlMappingRange mappedControlRange(
    double scale, double offset, ModulationPolarity inputPolarity) noexcept;

} // namespace reverb::graph
