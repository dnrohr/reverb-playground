#include <reverb/graph/AcyclicRuntime.h>

#include <reverb/dsp/Allpass.h>
#include <reverb/dsp/Delay.h>
#include <reverb/dsp/Gain.h>
#include <reverb/dsp/OnePoleLowPass.h>
#include <reverb/dsp/Sum.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace reverb::graph {
namespace {

enum class OperationKind { input, output, gain, sum, delay, allpass, lowpass };
using Processor = std::variant<std::monostate, reverb::dsp::Gain, reverb::dsp::Delay,
    reverb::dsp::Allpass, reverb::dsp::OnePoleLowPass>;

struct Operation final {
    std::string id;
    OperationKind kind {};
    std::vector<std::size_t> inputs;
    std::vector<std::size_t> outputs;
    Processor processor;
    float sumGain { 1.0F };
};

std::string portKey(const std::string& node, const std::string& port) { return node + "\n" + port; }

const Parameter* parameter(const Node& node, const std::string_view id)
{
    const auto found = std::ranges::find(node.parameters, id, &Parameter::id);
    return found == node.parameters.end() ? nullptr : &*found;
}

bool hasParameter(const Node& node, const std::string_view id, const std::string_view unit)
{
    const auto* value = parameter(node, id);
    return value != nullptr && value->unit == unit && std::isfinite(value->value);
}

bool hasPorts(const Node& node, const std::vector<std::pair<std::string, PortDirection>>& expected)
{
    if (node.ports.size() != expected.size())
        return false;
    for (const auto& [id, direction] : expected) {
        const auto found = std::ranges::find(node.ports, id, &Port::id);
        if (found == node.ports.end() || found->direction != direction || found->signal != SignalType::audio)
            return false;
    }
    return true;
}

void addNodeContractErrors(const Node& node, std::vector<std::string>& errors)
{
    const auto unary = hasPorts(node, { { "in", PortDirection::input }, { "out", PortDirection::output } });
    if (node.type == "stereo-input") {
        if (!hasPorts(node, { { "out-l", PortDirection::output }, { "out-r", PortDirection::output } }))
            errors.push_back("node '" + node.id + "' must expose audio outputs out-l and out-r");
    } else if (node.type == "stereo-output") {
        if (!hasPorts(node, { { "in-l", PortDirection::input }, { "in-r", PortDirection::input } }))
            errors.push_back("node '" + node.id + "' must expose audio inputs in-l and in-r");
    } else if (node.type == "sum") {
        const auto generic = hasPorts(node, { { "in-a", PortDirection::input }, { "in-b", PortDirection::input }, { "out", PortDirection::output } });
        const auto reference = hasPorts(node, { { "in-l", PortDirection::input }, { "in-r", PortDirection::input }, { "out", PortDirection::output } });
        if (!generic && !reference)
            errors.push_back("sum node '" + node.id + "' must expose two audio inputs and out");
        if (parameter(node, "gain") != nullptr && !hasParameter(node, "gain", "linear"))
            errors.push_back("sum node '" + node.id + "' gain must be finite linear units");
    } else if (node.type == "gain") {
        if (!unary || !hasParameter(node, "gain", "linear"))
            errors.push_back("gain node '" + node.id + "' requires in, out, and gain");
    } else if (node.type == "delay") {
        if (!unary || !hasParameter(node, "delay", "milliseconds"))
            errors.push_back("delay node '" + node.id + "' requires in, out, and delay");
    } else if (node.type == "allpass") {
        if (!unary || !hasParameter(node, "delay", "milliseconds") || !hasParameter(node, "coefficient", "unitless"))
            errors.push_back("allpass node '" + node.id + "' requires in, out, delay, and coefficient");
    } else if (node.type == "lowpass") {
        if (!unary || !hasParameter(node, "cutoff", "hertz"))
            errors.push_back("lowpass node '" + node.id + "' requires in, out, and cutoff");
    } else {
        errors.push_back("unsupported node type '" + node.type + "' on '" + node.id + "'");
    }
}

OperationKind kindFor(const std::string& type)
{
    if (type == "stereo-input") return OperationKind::input;
    if (type == "stereo-output") return OperationKind::output;
    if (type == "gain") return OperationKind::gain;
    if (type == "sum") return OperationKind::sum;
    if (type == "delay") return OperationKind::delay;
    if (type == "allpass") return OperationKind::allpass;
    return OperationKind::lowpass;
}

} // namespace

struct PreparedAcyclicRuntime::Impl final {
    std::size_t maximumBlockSize {};
    std::size_t silenceBuffer {};
    std::size_t inputLeftBuffer {};
    std::size_t inputRightBuffer {};
    std::size_t outputLeftInput {};
    std::size_t outputRightInput {};
    std::vector<std::string> schedule;
    std::vector<std::vector<float>> buffers;
    std::vector<Operation> operations;
};

PreparedAcyclicRuntime::PreparedAcyclicRuntime(std::unique_ptr<Impl> implementation) noexcept
    : implementation_(std::move(implementation)) {}
PreparedAcyclicRuntime::~PreparedAcyclicRuntime() = default;
PreparedAcyclicRuntime::PreparedAcyclicRuntime(PreparedAcyclicRuntime&&) noexcept = default;
PreparedAcyclicRuntime& PreparedAcyclicRuntime::operator=(PreparedAcyclicRuntime&&) noexcept = default;

const std::vector<std::string>& PreparedAcyclicRuntime::schedule() const noexcept { return implementation_->schedule; }
std::size_t PreparedAcyclicRuntime::maximumBlockSize() const noexcept { return implementation_->maximumBlockSize; }
std::size_t PreparedAcyclicRuntime::preparedStorageBytes() const noexcept
{
    return implementation_->buffers.size() * implementation_->maximumBlockSize * sizeof(float);
}

void PreparedAcyclicRuntime::process(
    const std::span<const float> inputLeft, const std::span<const float> inputRight,
    const std::span<float> outputLeft, const std::span<float> outputRight) noexcept
{
    const auto count = outputLeft.size();
    if (outputRight.size() != count || inputLeft.size() < count || inputRight.size() < count
        || count > implementation_->maximumBlockSize) {
        std::ranges::fill(outputLeft, 0.0F);
        std::ranges::fill(outputRight, 0.0F);
        return;
    }
    auto buffer = [this, count](const std::size_t index) { return std::span<float>(implementation_->buffers[index]).first(count); };
    std::ranges::fill(buffer(implementation_->silenceBuffer), 0.0F);
    std::ranges::copy(inputLeft.first(count), buffer(implementation_->inputLeftBuffer).begin());
    std::ranges::copy(inputRight.first(count), buffer(implementation_->inputRightBuffer).begin());

    for (auto& operation : implementation_->operations) {
        if (operation.kind == OperationKind::input || operation.kind == OperationKind::output)
            continue;
        auto destination = buffer(operation.outputs.front());
        std::ranges::copy(buffer(operation.inputs.front()), destination.begin());
        if (operation.kind == OperationKind::sum) {
            reverb::dsp::Sum::process(destination, buffer(operation.inputs[1]), destination);
            if (operation.sumGain != 1.0F)
                for (auto& sample : destination) sample *= operation.sumGain;
        } else if (operation.kind == OperationKind::gain) {
            std::get<reverb::dsp::Gain>(operation.processor).process(destination);
        } else if (operation.kind == OperationKind::delay) {
            std::get<reverb::dsp::Delay>(operation.processor).process(destination);
        } else if (operation.kind == OperationKind::allpass) {
            std::get<reverb::dsp::Allpass>(operation.processor).process(destination);
        } else if (operation.kind == OperationKind::lowpass) {
            std::get<reverb::dsp::OnePoleLowPass>(operation.processor).process(destination);
        }
    }
    std::ranges::copy(buffer(implementation_->outputLeftInput), outputLeft.begin());
    std::ranges::copy(buffer(implementation_->outputRightInput), outputRight.begin());
}

AcyclicCompileResult compileAcyclicGraph(
    const GraphDocument& document, const double sampleRate, const std::size_t maximumBlockSize)
{
    AcyclicCompileResult result;
    if (sampleRate <= 0.0) result.errors.push_back("sample rate must be positive");
    if (sampleRate > 192'000.0) result.errors.push_back("sample rate exceeds the 192 kHz runtime limit");
    if (maximumBlockSize == 0) result.errors.push_back("maximum block size must be positive");
    if (document.nodes.size() > 256) result.errors.push_back("patch exceeds the 256-node runtime limit");
    if (document.connections.size() > 512) result.errors.push_back("patch exceeds the 512-connection runtime limit");
    const auto validation = validate(document);
    result.errors.insert(result.errors.end(), validation.errors.begin(), validation.errors.end());

    std::map<std::string, const Node*> nodes;
    std::unordered_map<std::string, std::vector<std::string>> adjacency;
    std::unordered_map<std::string, std::vector<std::string>> reverse;
    std::unordered_map<std::string, std::size_t> indegree;
    std::unordered_map<std::string, const Connection*> incoming;
    std::unordered_set<std::string> incidentNodes;
    std::size_t inputs = 0; std::size_t outputs = 0;
    std::size_t delayLines = 0;
    for (const auto& node : document.nodes) {
        nodes.emplace(node.id, &node); indegree[node.id] = 0; addNodeContractErrors(node, result.errors);
        for (const auto& value : node.parameters) if (!std::isfinite(value.value))
            result.errors.push_back("parameter '" + node.id + "." + value.id + "' must be finite");
        inputs += node.type == "stereo-input"; outputs += node.type == "stereo-output";
        if (node.type == "delay" || node.type == "allpass") {
            ++delayLines;
            if (const auto* delay = parameter(node, "delay"); delay != nullptr && delay->value > 10'000.0)
                result.errors.push_back("node '" + node.id + "' exceeds the 10-second delay limit");
        }
    }
    if (delayLines > 64) result.errors.push_back("patch exceeds the 64-delay-line runtime limit");
    if (inputs != 1) result.errors.push_back("acyclic runtime requires exactly one stereo-input");
    if (outputs != 1) result.errors.push_back("acyclic runtime requires exactly one stereo-output");
    for (const auto& connection : document.connections) {
        if (!nodes.contains(connection.from.nodeId) || !nodes.contains(connection.to.nodeId)) continue;
        const auto key = portKey(connection.to.nodeId, connection.to.portId);
        if (!incoming.emplace(key, &connection).second)
            result.errors.push_back("input '" + connection.to.nodeId + "." + connection.to.portId + "' has more than one cable");
        adjacency[connection.from.nodeId].push_back(connection.to.nodeId);
        reverse[connection.to.nodeId].push_back(connection.from.nodeId);
        incidentNodes.insert(connection.from.nodeId); incidentNodes.insert(connection.to.nodeId);
        ++indegree[connection.to.nodeId];
    }
    if (!result.errors.empty()) return result;

    std::priority_queue<std::string, std::vector<std::string>, std::greater<>> ready;
    for (const auto& [id, degree] : indegree) if (degree == 0) ready.push(id);
    while (!ready.empty()) {
        auto id = ready.top(); ready.pop(); result.schedule.push_back(id);
        auto targets = adjacency[id]; std::ranges::sort(targets);
        for (const auto& target : targets) if (--indegree[target] == 0) ready.push(target);
    }
    if (result.schedule.size() != nodes.size()) {
        result.errors.push_back("acyclic compiler rejected a directed cycle; feedback compilation is provided by M3.4");
        result.schedule.clear(); return result;
    }

    const auto inputId = std::ranges::find_if(document.nodes, [](const Node& node) { return node.type == "stereo-input"; })->id;
    const auto outputId = std::ranges::find_if(document.nodes, [](const Node& node) { return node.type == "stereo-output"; })->id;
    std::unordered_set<std::string> fromInput { inputId }, toOutput { outputId };
    std::vector<std::string> frontier { inputId };
    while (!frontier.empty()) { auto id = frontier.back(); frontier.pop_back(); for (const auto& next : adjacency[id]) if (fromInput.insert(next).second) frontier.push_back(next); }
    frontier = { outputId };
    while (!frontier.empty()) { auto id = frontier.back(); frontier.pop_back(); for (const auto& prior : reverse[id]) if (toOutput.insert(prior).second) frontier.push_back(prior); }
    for (const auto& [id, node] : nodes) {
        const auto incident = incidentNodes.contains(id);
        if (!incident && node->type != "stereo-input" && node->type != "stereo-output")
            result.warnings.push_back("disconnected node '" + id + "' processes silence and its output is discarded");
        else if (!fromInput.contains(id)) result.warnings.push_back("node '" + id + "' is unreachable from stereo input");
        else if (!toOutput.contains(id)) result.warnings.push_back("node '" + id + "' cannot reach stereo output and is discarded");
    }

    try {
        auto implementation = std::make_unique<PreparedAcyclicRuntime::Impl>();
        implementation->maximumBlockSize = maximumBlockSize; implementation->schedule = result.schedule;
        implementation->buffers.emplace_back(maximumBlockSize, 0.0F); implementation->silenceBuffer = 0;
        std::unordered_map<std::string, std::size_t> outputBuffers;
        for (const auto& id : result.schedule) {
            for (const auto& port : nodes.at(id)->ports) if (port.direction == PortDirection::output) {
                outputBuffers[portKey(id, port.id)] = implementation->buffers.size();
                implementation->buffers.emplace_back(maximumBlockSize, 0.0F);
            }
        }
        implementation->inputLeftBuffer = outputBuffers.at(portKey(inputId, "out-l"));
        implementation->inputRightBuffer = outputBuffers.at(portKey(inputId, "out-r"));
        const auto inputBuffer = [&](const std::string& node, const std::string& port) {
            const auto found = incoming.find(portKey(node, port));
            return found == incoming.end() ? implementation->silenceBuffer
                : outputBuffers.at(portKey(found->second->from.nodeId, found->second->from.portId));
        };
        implementation->outputLeftInput = inputBuffer(outputId, "in-l");
        implementation->outputRightInput = inputBuffer(outputId, "in-r");

        for (const auto& id : result.schedule) {
            const auto& node = *nodes.at(id); Operation operation { .id = id, .kind = kindFor(node.type) };
            for (const auto& port : node.ports) {
                if (port.signal != SignalType::audio) continue;
                if (port.direction == PortDirection::input) operation.inputs.push_back(inputBuffer(id, port.id));
                else operation.outputs.push_back(outputBuffers.at(portKey(id, port.id)));
            }
            if (operation.kind == OperationKind::gain) {
                reverb::dsp::Gain processor; processor.prepare(sampleRate, static_cast<float>(parameter(node, "gain")->value), 0.0); operation.processor = std::move(processor);
            } else if (operation.kind == OperationKind::delay) {
                reverb::dsp::Delay processor; processor.prepare(sampleRate, parameter(node, "delay")->value); operation.processor = std::move(processor);
            } else if (operation.kind == OperationKind::allpass) {
                reverb::dsp::Allpass processor; const auto delay = parameter(node, "delay")->value; processor.prepare(sampleRate, delay, static_cast<float>(parameter(node, "coefficient")->value), std::max(100.0, delay)); operation.processor = std::move(processor);
            } else if (operation.kind == OperationKind::lowpass) {
                reverb::dsp::OnePoleLowPass processor; processor.prepare(sampleRate, parameter(node, "cutoff")->value); operation.processor = std::move(processor);
            } else if (operation.kind == OperationKind::sum) {
                if (const auto* gain = parameter(node, "gain")) operation.sumGain = static_cast<float>(gain->value);
            }
            implementation->operations.push_back(std::move(operation));
        }
        result.runtime = std::unique_ptr<PreparedAcyclicRuntime>(new PreparedAcyclicRuntime(std::move(implementation)));
    } catch (const std::exception& exception) {
        result.errors.push_back(std::string("runtime preparation failed: ") + exception.what());
        result.runtime.reset();
    }
    return result;
}

AcyclicPublishResult AcyclicRuntimeHost::compileAndPublish(
    const GraphDocument& document, const double sampleRate, const std::size_t maximumBlockSize)
{
    auto result = compileAcyclicGraph(document, sampleRate, maximumBlockSize);
    AcyclicPublishResult publication { result.schedule, result.warnings, result.errors };
    if (!result.valid()) return publication;
    std::scoped_lock lock(publicationMutex_);
    auto next = std::move(result.runtime);
    auto previous = std::move(ownedRuntime_);
    ownedRuntime_ = std::move(next);
    activeRuntime_.store(ownedRuntime_.get());
    while (processing_.load()) std::this_thread::yield();
    previous.reset();
    return publication;
}

void AcyclicRuntimeHost::process(
    const std::span<const float> inputLeft, const std::span<const float> inputRight,
    const std::span<float> outputLeft, const std::span<float> outputRight) noexcept
{
    processing_.store(true);
    if (auto* runtime = activeRuntime_.load()) runtime->process(inputLeft, inputRight, outputLeft, outputRight);
    else { std::ranges::fill(outputLeft, 0.0F); std::ranges::fill(outputRight, 0.0F); }
    processing_.store(false);
}

bool AcyclicRuntimeHost::hasRuntime() const noexcept { return activeRuntime_.load() != nullptr; }

} // namespace reverb::graph
