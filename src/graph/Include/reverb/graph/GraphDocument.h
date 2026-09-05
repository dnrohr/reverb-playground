#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace reverb::graph {

enum class SignalType {
    audio,
    control,
};

enum class PortDirection {
    input,
    output,
};

struct Port final {
    std::string id;
    SignalType signal { SignalType::audio };
    PortDirection direction { PortDirection::input };

    friend bool operator==(const Port&, const Port&) = default;
};

enum class ModulationPolarity {
    unipolar,
    bipolar,
};

struct ParameterModulation final {
    std::string portId;
    double amount { 0.0 };
    ModulationPolarity polarity { ModulationPolarity::bipolar };
    double clampMinimum { 0.0 };
    double clampMaximum { 1.0 };

    friend bool operator==(const ParameterModulation&, const ParameterModulation&) = default;
};

struct Parameter final {
    std::string id;
    double value { 0.0 };
    std::string unit { "unitless" };
    std::optional<ParameterModulation> modulation;

    friend bool operator==(const Parameter&, const Parameter&) = default;
};

struct Node final {
    std::string id;
    std::string type;
    std::vector<Port> ports;
    std::vector<Parameter> parameters;
    std::string name;
    std::string presentation;

    friend bool operator==(const Node&, const Node&) = default;
};

struct PortReference final {
    std::string nodeId;
    std::string portId;

    friend bool operator==(const PortReference&, const PortReference&) = default;
};

struct Connection final {
    std::string id;
    PortReference from;
    PortReference to;

    friend bool operator==(const Connection&, const Connection&) = default;
};

struct NodePosition final {
    std::string nodeId;
    double x { 0.0 };
    double y { 0.0 };
    bool reversed { false };

    friend bool operator==(const NodePosition&, const NodePosition&) = default;
};

struct Viewport final {
    double x { 0.0 };
    double y { 0.0 };
    double zoom { 1.0 };

    friend bool operator==(const Viewport&, const Viewport&) = default;
};

struct LayoutGroup final {
    std::string id;
    std::string name;
    bool collapsed { false };
    std::vector<std::string> nodeIds;

    friend bool operator==(const LayoutGroup&, const LayoutGroup&) = default;
};

struct CableWaypoint final { double x { 0.0 }; double y { 0.0 }; friend bool operator==(const CableWaypoint&, const CableWaypoint&) = default; };
struct LayoutCable final {
    std::string edgeId;
    std::vector<CableWaypoint> waypoints;
    std::optional<std::string> portalName;
    friend bool operator==(const LayoutCable&, const LayoutCable&) = default;
};
struct SubpatchPortBinding final {
    std::string id;
    SignalType signal { SignalType::audio };
    PortDirection direction { PortDirection::input };
    std::string nodeId;
    std::string portId;
    friend bool operator==(const SubpatchPortBinding&, const SubpatchPortBinding&) = default;
};
struct LayoutSubpatchInstance final {
    std::string id;
    std::string definitionId;
    std::uint32_t definitionVersion { 1 };
    std::string definitionName;
    std::vector<std::string> memberNodeIds;
    std::vector<SubpatchPortBinding> ports;
    friend bool operator==(const LayoutSubpatchInstance&, const LayoutSubpatchInstance&) = default;
};

struct HierarchyPortBinding final {
    std::string id;
    std::string name;
    SignalType signal { SignalType::audio };
    PortDirection direction { PortDirection::input };
    std::vector<PortReference> targets;
    friend bool operator==(const HierarchyPortBinding&, const HierarchyPortBinding&) = default;
};

struct LayoutHierarchyPresentation final {
    std::string id;
    std::string kind;
    std::string name;
    bool collapsed { true };
    bool reversed { false };
    std::vector<std::string> memberNodeIds;
    double x { 0.0 };
    double y { 0.0 };
    Viewport nestedViewport;
    std::vector<HierarchyPortBinding> ports;
    std::optional<std::string> parentId;
    friend bool operator==(const LayoutHierarchyPresentation&, const LayoutHierarchyPresentation&) = default;
};

struct Layout final {
    std::vector<NodePosition> nodes;
    Viewport viewport;
    std::vector<LayoutGroup> groups;
    std::vector<LayoutCable> cables;
    std::vector<LayoutSubpatchInstance> subpatches;
    std::vector<LayoutHierarchyPresentation> hierarchies;

    friend bool operator==(const Layout&, const Layout&) = default;
};

enum class QualityPolicy : std::uint8_t { draft, normal, high };

class GraphDocument final {
public:
    static constexpr std::uint32_t schemaVersion = 2;
    static constexpr std::uint32_t oldestReadableSchemaVersion = 1;

    std::string engineVersion { "0.1" };
    QualityPolicy qualityPolicy { QualityPolicy::normal };
    std::vector<Node> nodes;
    std::vector<Connection> connections;
    Layout layout;
    std::vector<std::string> migrationWarnings;

    friend bool operator==(const GraphDocument&, const GraphDocument&) = default;
};

struct ValidationResult final {
    std::vector<std::string> errors;

    [[nodiscard]] bool valid() const noexcept { return errors.empty(); }
};

[[nodiscard]] ValidationResult validate(const GraphDocument& document);

} // namespace reverb::graph
