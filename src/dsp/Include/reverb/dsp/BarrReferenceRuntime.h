#pragma once

#include <span>
#include <string_view>
#include <optional>

namespace reverb::dsp {

struct RuntimePortDefinition final {
    std::string_view id;
    std::string_view signal;
    std::string_view direction;
};

enum class BarrParameterId {
    sumGain,
    filterCutoff,
    diffuserOneDelay,
    diffuserOneCoefficient,
    diffuserTwoDelay,
    diffuserTwoCoefficient,
    tankOneDelay,
    tankOneCoefficient,
    tankTwoDelay,
    tankTwoCoefficient,
    leftTapDelay,
    leftTapCoefficient,
    rightTapDelay,
    rightTapCoefficient,
    count,
};

struct RuntimeParameterDefinition final {
    BarrParameterId runtimeId;
    std::string_view id;
    double value;
    std::string_view unit;
    double minimum;
    double maximum;
    double step;
};

struct RuntimeNodeDefinition final {
    std::string_view id;
    std::string_view type;
    std::string_view label;
    std::string_view role;
    std::span<const RuntimePortDefinition> ports;
    std::span<const RuntimeParameterDefinition> parameters;
};

struct RuntimeConnectionDefinition final {
    std::string_view id;
    std::string_view sourceNode;
    std::string_view sourcePort;
    std::string_view targetNode;
    std::string_view targetPort;
};

[[nodiscard]] std::span<const RuntimeNodeDefinition> barrReferenceRuntimeNodes() noexcept;
[[nodiscard]] std::span<const RuntimeConnectionDefinition> barrReferenceRuntimeConnections() noexcept;
[[nodiscard]] double barrReferenceParameter(std::string_view nodeId, std::string_view parameterId);
[[nodiscard]] const RuntimeParameterDefinition& barrReferenceParameterDefinition(BarrParameterId id);
[[nodiscard]] std::optional<BarrParameterId> findBarrReferenceParameter(
    std::string_view nodeId, std::string_view parameterId) noexcept;

} // namespace reverb::dsp
