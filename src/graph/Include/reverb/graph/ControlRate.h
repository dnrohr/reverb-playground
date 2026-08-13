#pragma once

#include <reverb/graph/GraphDocument.h>
#include <reverb/graph/ControlModulation.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace reverb::graph {

inline constexpr double controlRateHz = 1'000.0;
inline constexpr std::size_t maximumControlNodes = 64;
inline constexpr std::size_t maximumControlMappings = 128;
inline constexpr std::size_t maximumMacroControls = 64;
inline constexpr std::size_t macroSmoothingTicks = 20;

[[nodiscard]] std::uint64_t macroControlKey(std::string_view nodeId) noexcept;
[[nodiscard]] std::size_t macroControlSlot(std::uint64_t key) noexcept;

struct CompiledControlMapping final {
    std::string sourceNodeId;
    std::string sourcePortId;
    std::string targetNodeId;
    std::string parameterId;
    double baseValue { 0.0 };
    double amount { 0.0 };
    ModulationPolarity polarity { ModulationPolarity::bipolar };
    double clampMinimum { 0.0 };
    double clampMaximum { 1.0 };
};

struct ControlRatePlan final {
    double sampleRate { 0.0 };
    std::size_t maximumBlockSize { 0 };
    std::size_t quantumSamples { 0 };
    std::size_t maximumTicksPerBlock { 0 };
    std::size_t maximumMappingEvaluationsPerBlock { 0 };
    std::vector<CompiledControlMapping> mappings;
    struct LfoNode final {
        std::string nodeId;
        double frequencyHz { 1.0 };
        double phaseCycles { 0.0 };
        LfoWaveform waveform { LfoWaveform::sine };
        LfoRunMode runMode { LfoRunMode::freeRun };
    };
    struct MapperNode final {
        std::string nodeId;
        std::string sourceNodeId;
        std::string sourcePortId;
        double scale { 1.0 };
        double offset { 0.0 };
        ModulationPolarity inputPolarity { ModulationPolarity::bipolar };
        ControlCurveFamily curveFamily { ControlCurveFamily::linear };
        double curveAmount { 0.0 };
        double exponent { 1.0 };
        double clampMinimum { -1.0 };
        double clampMaximum { 1.0 };
        ControlMappingRange predictedRange;
    };
    struct MacroNode final {
        std::string nodeId;
        std::uint64_t key {};
        std::size_t slot {};
        double value {};
        double defaultValue {};
        bool centerDetent {};
    };
    std::vector<LfoNode> lfos;
    std::vector<MapperNode> mappers;
    std::vector<MacroNode> macros;
    std::vector<std::string> errors;

    [[nodiscard]] bool valid() const noexcept { return errors.empty(); }
};

[[nodiscard]] ControlRatePlan compileControlRatePlan(
    const GraphDocument& document, double sampleRate, std::size_t maximumBlockSize);

[[nodiscard]] double mappedParameterValue(
    const CompiledControlMapping& mapping, double controlValue) noexcept;

class ControlRamp final {
public:
    void reset(double value) noexcept;
    void setTarget(double value, std::size_t samples) noexcept;
    [[nodiscard]] double next() noexcept;
    [[nodiscard]] double current() const noexcept { return current_; }

private:
    double current_ { 0.0 };
    double increment_ { 0.0 };
    double target_ { 0.0 };
    std::size_t remaining_ { 0 };
};

} // namespace reverb::graph
