#pragma once

#include <span>
#include <string_view>

namespace reverb::dsp {

struct RuntimePortDefinition final {
    std::string_view id;
    std::string_view signal;
    std::string_view direction;
};

struct RuntimeParameterDefinition final {
    std::string_view id;
    double value;
    std::string_view unit;
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

} // namespace reverb::dsp
