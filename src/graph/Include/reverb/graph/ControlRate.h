#pragma once

#include <reverb/graph/GraphDocument.h>

#include <cstddef>
#include <string>
#include <vector>

namespace reverb::graph {

inline constexpr double controlRateHz = 1'000.0;
inline constexpr std::size_t maximumControlNodes = 64;
inline constexpr std::size_t maximumControlMappings = 128;

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
