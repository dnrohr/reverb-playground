#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/ControlModulation.h>
#include <reverb/graph/ControlRate.h>

#include <reverb/dsp/Allpass.h>
#include <reverb/dsp/Delay.h>
#include <reverb/dsp/EnvelopeFollower.h>
#include <reverb/dsp/Gain.h>
#include <reverb/dsp/HoldGate.h>
#include <reverb/dsp/OnePoleLowPass.h>
#include <reverb/dsp/Sum.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace reverb::graph {
namespace {

enum class OperationKind { input, output, gain, sum, delay, allpass, lowpass, envelopeFollower, holdGate };
using Processor = std::variant<std::monostate, reverb::dsp::Gain, reverb::dsp::Delay,
    reverb::dsp::Allpass, reverb::dsp::OnePoleLowPass,
    reverb::dsp::EnvelopeFollower, reverb::dsp::HoldGate>;

struct Operation final {
    std::string id;
    OperationKind kind {};
    std::vector<std::size_t> inputs;
    std::vector<std::size_t> outputs;
    Processor processor;
    float sumGain { 1.0F };
    double baseDelayMilliseconds {};
    double baseCoefficient {};
    std::size_t delayModulation { std::numeric_limits<std::size_t>::max() };
    std::size_t coefficientModulation { std::numeric_limits<std::size_t>::max() };
    std::size_t controlInput { std::numeric_limits<std::size_t>::max() };
    double controlScale { 1.0 };
    double controlOffset {};
    ModulationPolarity controlPolarity { ModulationPolarity::unipolar };
    ControlCurveFamily controlCurveFamily { ControlCurveFamily::linear };
    double controlCurveAmount {};
    double controlExponent { 1.0 };
    double controlClampMinimum { -1.0 };
    double controlClampMaximum { 1.0 };
};

enum class ControlOperationKind { macro, lfo, mapper };

struct ControlOperation final {
    std::string id;
    ControlOperationKind kind {};
    reverb::graph::ControlLfo lfo;
    std::size_t source { std::numeric_limits<std::size_t>::max() };
    double scale { 1.0 };
    double offset {};
    ModulationPolarity polarity { ModulationPolarity::bipolar };
    ControlCurveFamily curveFamily { ControlCurveFamily::linear };
    double curveAmount {};
    double exponent { 1.0 };
    double clampMinimum { -1.0 };
    double clampMaximum { 1.0 };
    double value {};
    std::uint64_t macroKey {};
    std::size_t macroSlot {};
    double macroTarget {};
    double macroDefault {};
    double observedMacroTarget {};
    ControlRamp macroRamp;
};

struct RuntimeModulation final {
    CompiledControlMapping mapping;
    ControlRamp ramp;
    std::size_t source { std::numeric_limits<std::size_t>::max() };
    std::vector<double> values;
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

bool parameterInRange(
    const Node& node, const std::string_view id, const std::string_view unit,
    const double minimum, const double maximum)
{
    const auto* value = parameter(node, id);
    return value != nullptr && value->unit == unit && std::isfinite(value->value)
        && value->value >= minimum && value->value <= maximum;
}

bool hasPorts(const Node& node, const std::vector<std::pair<std::string, PortDirection>>& expected)
{
    if (static_cast<std::size_t>(std::ranges::count(node.ports, SignalType::audio, &Port::signal)) != expected.size())
        return false;
    for (const auto& [id, direction] : expected) {
        const auto found = std::ranges::find(node.ports, id, &Port::id);
        if (found == node.ports.end() || found->direction != direction || found->signal != SignalType::audio)
            return false;
    }
    return true;
}

bool hasPort(const Node& node, const std::string_view id, const SignalType signal, const PortDirection direction)
{
    const auto found = std::ranges::find(node.ports, id, &Port::id);
    return found != node.ports.end() && found->signal == signal && found->direction == direction;
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
    } else if (node.type == "macro") {
        if (!hasPort(node, "out", SignalType::control, PortDirection::output)
            || !parameterInRange(node, "value", "normalized", -1.0, 1.0)
            || !parameterInRange(node, "default-value", "normalized", -1.0, 1.0)
            || !parameterInRange(node, "center-detent", "boolean", 0.0, 1.0)
            || node.name.empty() || node.name.size() > 64)
            errors.push_back("macro node '" + node.id + "' requires name, normalized value/default, center detent, and control out");
    } else if (node.type == "lfo") {
        if (!hasPort(node, "out", SignalType::control, PortDirection::output)
            || !hasParameter(node, "frequency", "hertz") || !hasParameter(node, "phase", "cycles")
            || !hasParameter(node, "waveform", "waveform") || !hasParameter(node, "run-mode", "run-mode"))
            errors.push_back("lfo node '" + node.id + "' requires frequency, phase, waveform, and run-mode");
    } else if (node.type == "control-map") {
        const auto hasNoCurveFields = parameter(node, "curve-family") == nullptr
            && parameter(node, "curve-amount") == nullptr && parameter(node, "exponent") == nullptr
            && parameter(node, "clamp-min") == nullptr && parameter(node, "clamp-max") == nullptr;
        const auto hasAllCurveFields = hasParameter(node, "curve-family", "curve-family")
            && hasParameter(node, "curve-amount", "unitless")
            && hasParameter(node, "exponent", "unitless")
            && hasParameter(node, "clamp-min", "unitless")
            && hasParameter(node, "clamp-max", "unitless");
        if (!hasPort(node, "in", SignalType::control, PortDirection::input)
            || !hasPort(node, "out", SignalType::control, PortDirection::output)
            || !hasParameter(node, "scale", "linear") || !hasParameter(node, "offset", "unitless")
            || !hasParameter(node, "polarity", "polarity") || (!hasNoCurveFields && !hasAllCurveFields))
            errors.push_back("control-map node '" + node.id + "' requires curve, scale, offset, polarity, and clamps");
    } else if (node.type == "envelope-follower") {
        if (!hasPort(node, "in", SignalType::audio, PortDirection::input)
            || !hasPort(node, "out", SignalType::control, PortDirection::output)
            || !parameterInRange(node, "attack", "milliseconds", 0.1, 500.0)
            || !parameterInRange(node, "release", "milliseconds", 1.0, 5'000.0))
            errors.push_back("envelope-follower node '" + node.id + "' requires audio in, control out, and bounded attack/release milliseconds");
    } else if (node.type == "hold-gate") {
        if (!hasPort(node, "in", SignalType::audio, PortDirection::input)
            || !hasPort(node, "gate", SignalType::control, PortDirection::input)
            || !hasPort(node, "out", SignalType::audio, PortDirection::output)
            || !parameterInRange(node, "threshold", "unitless", 0.0, 1.0)
            || !parameterInRange(node, "attack", "milliseconds", 0.1, 100.0)
            || !parameterInRange(node, "hold", "milliseconds", 1.0, 2'000.0)
            || !parameterInRange(node, "release", "milliseconds", 0.1, 1'000.0))
            errors.push_back("hold-gate node '" + node.id + "' requires audio/control inputs, audio out, and bounded threshold/attack/hold/release");
    } else {
        errors.push_back("unsupported node type '" + node.type + "' on '" + node.id + "'");
    }
    if (!node.presentation.empty()
        && (node.type != "macro" || node.presentation != "gravity"))
        errors.push_back("node '" + node.id + "' has an unsupported presentation designation");
}

OperationKind kindFor(const std::string& type)
{
    if (type == "stereo-input") return OperationKind::input;
    if (type == "stereo-output") return OperationKind::output;
    if (type == "gain") return OperationKind::gain;
    if (type == "sum") return OperationKind::sum;
    if (type == "delay") return OperationKind::delay;
    if (type == "allpass") return OperationKind::allpass;
    if (type == "envelope-follower") return OperationKind::envelopeFollower;
    if (type == "hold-gate") return OperationKind::holdGate;
    return OperationKind::lowpass;
}

std::vector<std::vector<std::string>> cyclicComponents(
    const std::map<std::string, const Node*>& nodes,
    const std::unordered_map<std::string, std::vector<std::string>>& adjacency)
{
    std::unordered_map<std::string, int> index;
    std::unordered_map<std::string, int> lowLink;
    std::unordered_set<std::string> onStack;
    std::vector<std::string> stack;
    std::vector<std::vector<std::string>> components;
    int nextIndex = 0;
    const auto visit = [&](const auto& self, const std::string& id) -> void {
        index[id] = nextIndex; lowLink[id] = nextIndex++; stack.push_back(id); onStack.insert(id);
        auto targets = adjacency.contains(id) ? adjacency.at(id) : std::vector<std::string> {};
        std::ranges::sort(targets);
        for (const auto& target : targets) {
            if (!index.contains(target)) { self(self, target); lowLink[id] = std::min(lowLink[id], lowLink[target]); }
            else if (onStack.contains(target)) lowLink[id] = std::min(lowLink[id], index[target]);
        }
        if (lowLink[id] != index[id]) return;
        std::vector<std::string> component;
        while (true) { auto member = stack.back(); stack.pop_back(); onStack.erase(member); component.push_back(member); if (member == id) break; }
        std::ranges::sort(component);
        const auto selfLoop = component.size() == 1 && adjacency.contains(component.front())
            && std::ranges::find(adjacency.at(component.front()), component.front()) != adjacency.at(component.front()).end();
        if (component.size() > 1 || selfLoop) components.push_back(std::move(component));
    };
    for (const auto& [id, unused] : nodes) { (void)unused; if (!index.contains(id)) visit(visit, id); }
    std::ranges::sort(components);
    return components;
}

std::vector<std::string> concreteCycle(
    const std::vector<std::string>& component,
    const std::unordered_map<std::string, std::vector<std::string>>& adjacency)
{
    const std::unordered_set<std::string> allowed(component.begin(), component.end());
    for (const auto& start : component) {
        std::vector<std::string> path { start };
        std::unordered_set<std::string> inPath { start };
        const auto find = [&](const auto& self, const std::string& id) -> bool {
            auto targets = adjacency.contains(id) ? adjacency.at(id) : std::vector<std::string> {};
            std::ranges::sort(targets);
            for (const auto& target : targets) {
                if (!allowed.contains(target)) continue;
                if (target == start) { path.push_back(start); return true; }
                if (inPath.insert(target).second) { path.push_back(target); if (self(self, target)) return true; path.pop_back(); inPath.erase(target); }
            }
            return false;
        };
        if (find(find, start)) return path;
    }
    return component;
}

std::string loopMessage(const std::vector<std::string>& loop)
{
    std::string message = "zero-delay algebraic loop: ";
    for (std::size_t index = 0; index < loop.size(); ++index) {
        if (index != 0) message += " -> ";
        message += loop[index];
    }
    return message;
}

} // namespace

struct PreparedAcyclicRuntime::Impl final {
    std::size_t maximumBlockSize {};
    DelayMemoryPlan delayMemory;
    std::vector<float> delayArena;
    bool feedbackMode {};
    bool sampleWiseMode {};
    std::size_t silenceBuffer {};
    std::size_t inputLeftBuffer {};
    std::size_t inputRightBuffer {};
    std::size_t outputLeftInput {};
    std::size_t outputRightInput {};
    std::vector<std::string> schedule;
    std::vector<std::vector<float>> buffers;
    std::vector<Operation> operations;
    std::vector<ControlOperation> controlOperations;
    std::unordered_map<std::string, std::size_t> controlOperationById;
    std::vector<RuntimeModulation> modulations;
    std::size_t controlQuantumSamples { 1 };
    std::size_t samplesUntilControlTick {};
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
    return implementation_->buffers.size() * implementation_->maximumBlockSize * sizeof(float)
        + implementation_->delayMemory.allocatedBytes;
}
const DelayMemoryPlan& PreparedAcyclicRuntime::delayMemoryPlan() const noexcept { return implementation_->delayMemory; }

void PreparedAcyclicRuntime::reset() noexcept
{
    for (auto& buffer : implementation_->buffers) std::ranges::fill(buffer, 0.0F);
    for (auto& operation : implementation_->operations) {
        if (operation.kind == OperationKind::gain)
            std::get<reverb::dsp::Gain>(operation.processor).settleTarget();
        else if (operation.kind == OperationKind::delay)
            std::get<reverb::dsp::Delay>(operation.processor).reset();
        else if (operation.kind == OperationKind::allpass) {
            auto& allpass = std::get<reverb::dsp::Allpass>(operation.processor);
            allpass.reset();
            allpass.settleParameters();
        } else if (operation.kind == OperationKind::lowpass) {
            auto& lowpass = std::get<reverb::dsp::OnePoleLowPass>(operation.processor);
            lowpass.reset();
            lowpass.settleParameters();
        } else if (operation.kind == OperationKind::envelopeFollower) {
            std::get<reverb::dsp::EnvelopeFollower>(operation.processor).reset();
        } else if (operation.kind == OperationKind::holdGate) {
            std::get<reverb::dsp::HoldGate>(operation.processor).reset();
        }
    }
    for (auto& control : implementation_->controlOperations) {
        if (control.kind == ControlOperationKind::macro) {
            control.macroTarget = control.macroDefault;
            control.observedMacroTarget = control.macroDefault;
            control.macroRamp.reset(control.macroDefault);
            control.value = control.macroDefault;
        } else {
            control.value = 0.0;
            if (control.kind == ControlOperationKind::lfo) control.lfo.restart();
        }
    }
    for (auto& modulation : implementation_->modulations) {
        modulation.ramp.reset(modulation.mapping.baseValue);
        std::ranges::fill(modulation.values, modulation.mapping.baseValue);
    }
    implementation_->samplesUntilControlTick = 0;
}

bool PreparedAcyclicRuntime::setMacroValue(const std::string_view nodeId, const double value) noexcept
{
    const auto finite = std::isfinite(value) ? std::clamp(value, -1.0, 1.0) : 0.0;
    for (auto& control : implementation_->controlOperations) {
        if (control.kind == ControlOperationKind::macro && control.id == nodeId) {
            control.macroTarget = finite;
            return true;
        }
    }
    return false;
}

void PreparedAcyclicRuntime::applyMacroValue(
    const std::size_t slot, const std::uint64_t key, const double value) noexcept
{
    for (auto& control : implementation_->controlOperations) {
        if (control.kind == ControlOperationKind::macro
            && control.macroSlot == slot && control.macroKey == key) {
            control.macroTarget = std::isfinite(value) ? std::clamp(value, -1.0, 1.0) : 0.0;
            return;
        }
    }
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
    const auto noModulation = std::numeric_limits<std::size_t>::max();
    for (std::size_t sample = 0; sample < count; ++sample) {
        if (!implementation_->modulations.empty() && implementation_->samplesUntilControlTick == 0) {
            for (auto& control : implementation_->controlOperations) {
                if (control.kind == ControlOperationKind::macro) {
                    if (control.macroTarget != control.observedMacroTarget) {
                        control.observedMacroTarget = control.macroTarget;
                        control.macroRamp.setTarget(control.macroTarget, macroSmoothingTicks);
                    }
                    control.value = control.macroRamp.next();
                } else if (control.kind == ControlOperationKind::lfo) {
                    control.value = control.lfo.next();
                } else {
                    const auto input = control.source == noModulation
                        ? 0.0
                        : implementation_->controlOperations[control.source].value;
                    control.value = mapControlValue(
                        input, control.curveFamily, control.curveAmount, control.exponent,
                        control.scale, control.offset, control.polarity,
                        control.clampMinimum, control.clampMaximum);
                }
            }
            for (auto& modulation : implementation_->modulations) {
                const auto control = modulation.source == noModulation
                    ? 0.0
                    : implementation_->controlOperations[modulation.source].value;
                modulation.ramp.setTarget(
                    mappedParameterValue(modulation.mapping, control), implementation_->controlQuantumSamples);
            }
            implementation_->samplesUntilControlTick = implementation_->controlQuantumSamples;
        }
        for (auto& modulation : implementation_->modulations)
            modulation.values[sample] = modulation.ramp.next();
        if (!implementation_->modulations.empty())
            --implementation_->samplesUntilControlTick;
    }
    if (implementation_->sampleWiseMode) {
        auto sampleBuffer = [this](const std::size_t index) { return std::span<float>(implementation_->buffers[index]).first(1); };
        for (std::size_t sampleIndex = 0; sampleIndex < count; ++sampleIndex) {
            implementation_->buffers[implementation_->silenceBuffer][0] = 0.0F;
            implementation_->buffers[implementation_->inputLeftBuffer][0] = inputLeft[sampleIndex];
            implementation_->buffers[implementation_->inputRightBuffer][0] = inputRight[sampleIndex];
            for (auto& operation : implementation_->operations) if (operation.kind == OperationKind::delay) {
                if (operation.delayModulation != noModulation)
                    std::get<reverb::dsp::Delay>(operation.processor).setDelayMilliseconds(
                        implementation_->modulations[operation.delayModulation].values[sampleIndex]);
                implementation_->buffers[operation.outputs.front()][0] = std::get<reverb::dsp::Delay>(operation.processor).readSample();
            }
            for (auto& operation : implementation_->operations) {
                if (operation.kind == OperationKind::input || operation.kind == OperationKind::output || operation.kind == OperationKind::delay) continue;
                auto destination = sampleBuffer(operation.outputs.front());
                std::ranges::copy(sampleBuffer(operation.inputs.front()), destination.begin());
                if (operation.kind == OperationKind::sum) {
                    reverb::dsp::Sum::process(destination, sampleBuffer(operation.inputs[1]), destination);
                    destination.front() *= operation.sumGain;
                } else if (operation.kind == OperationKind::gain) std::get<reverb::dsp::Gain>(operation.processor).process(destination);
                else if (operation.kind == OperationKind::allpass) {
                    if (operation.delayModulation != noModulation || operation.coefficientModulation != noModulation) {
                        const auto delay = operation.delayModulation == noModulation
                            ? operation.baseDelayMilliseconds
                            : implementation_->modulations[operation.delayModulation].values[sampleIndex];
                        const auto coefficient = operation.coefficientModulation == noModulation
                            ? operation.baseCoefficient
                            : implementation_->modulations[operation.coefficientModulation].values[sampleIndex];
                        destination.front() = std::get<reverb::dsp::Allpass>(operation.processor)
                            .processSampleModulated(destination.front(), delay, coefficient);
                    } else std::get<reverb::dsp::Allpass>(operation.processor).process(destination);
                }
                else if (operation.kind == OperationKind::lowpass) std::get<reverb::dsp::OnePoleLowPass>(operation.processor).process(destination);
                else if (operation.kind == OperationKind::envelopeFollower) {
                    destination.front() = std::get<reverb::dsp::EnvelopeFollower>(operation.processor)
                        .processSample(implementation_->buffers[operation.inputs.front()][0]);
                } else if (operation.kind == OperationKind::holdGate) {
                    const auto control = operation.controlInput == noModulation
                        ? 0.0
                        : mapControlValue(
                            implementation_->buffers[operation.controlInput][0],
                            operation.controlCurveFamily, operation.controlCurveAmount,
                            operation.controlExponent, operation.controlScale, operation.controlOffset,
                            operation.controlPolarity, operation.controlClampMinimum,
                            operation.controlClampMaximum);
                    destination.front() = std::get<reverb::dsp::HoldGate>(operation.processor)
                        .processSample(destination.front(), static_cast<float>(control));
                }
            }
            for (auto& operation : implementation_->operations) if (operation.kind == OperationKind::delay)
                std::get<reverb::dsp::Delay>(operation.processor).writeSample(implementation_->buffers[operation.inputs.front()][0]);
            outputLeft[sampleIndex] = implementation_->buffers[implementation_->outputLeftInput][0];
            outputRight[sampleIndex] = implementation_->buffers[implementation_->outputRightInput][0];
        }
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
            auto& processor = std::get<reverb::dsp::Delay>(operation.processor);
            if (operation.delayModulation == noModulation)
                processor.process(destination);
            else
                processor.processModulated(destination,
                    std::span<const double>(implementation_->modulations[operation.delayModulation].values).first(count));
        } else if (operation.kind == OperationKind::allpass) {
            auto& processor = std::get<reverb::dsp::Allpass>(operation.processor);
            if (operation.delayModulation == noModulation && operation.coefficientModulation == noModulation) {
                processor.process(destination);
            } else {
                for (std::size_t sample = 0; sample < count; ++sample) {
                    const auto delay = operation.delayModulation == noModulation
                        ? operation.baseDelayMilliseconds
                        : implementation_->modulations[operation.delayModulation].values[sample];
                    const auto coefficient = operation.coefficientModulation == noModulation
                        ? operation.baseCoefficient
                        : implementation_->modulations[operation.coefficientModulation].values[sample];
                    destination[sample] = processor.processSampleModulated(destination[sample], delay, coefficient);
                }
            }
        } else if (operation.kind == OperationKind::lowpass) {
            std::get<reverb::dsp::OnePoleLowPass>(operation.processor).process(destination);
        }
    }
    std::ranges::copy(buffer(implementation_->outputLeftInput), outputLeft.begin());
    std::ranges::copy(buffer(implementation_->outputRightInput), outputRight.begin());
}

AcyclicCompileResult compileAcyclicGraph(
    const GraphDocument& document, const double sampleRate, const std::size_t maximumBlockSize,
    const bool allowFeedback)
{
    AcyclicCompileResult result;
    const auto compileStarted = std::chrono::steady_clock::now();
    const auto finish = [&]() {
        result.compileMicroseconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - compileStarted).count());
        return std::move(result);
    };
    if (sampleRate <= 0.0) result.errors.push_back("sample rate must be positive");
    if (sampleRate > 192'000.0) result.errors.push_back("sample rate exceeds the 192 kHz runtime limit");
    if (maximumBlockSize == 0) result.errors.push_back("maximum block size must be positive");
    if (document.nodes.size() > 256) result.errors.push_back("patch exceeds the 256-node runtime limit");
    if (document.connections.size() > 512) result.errors.push_back("patch exceeds the 512-connection runtime limit");
    const auto validation = validate(document);
    for (const auto& error : validation.errors) {
        if (!(allowFeedback && error == "directed cycles must contain an explicit delay node"))
            result.errors.push_back(error);
    }
    ControlRatePlan controlPlan;
    if (sampleRate > 0.0 && sampleRate <= 192'000.0 && maximumBlockSize > 0) {
        controlPlan = compileControlRatePlan(document, sampleRate, maximumBlockSize);
        result.errors.insert(result.errors.end(), controlPlan.errors.begin(), controlPlan.errors.end());
        for (const auto& mapping : controlPlan.mappings) {
            const auto target = std::ranges::find(document.nodes, mapping.targetNodeId, &Node::id);
            if (target != document.nodes.end()
                && (target->type == "envelope-follower" || target->type == "hold-gate"))
                result.errors.push_back("node '" + target->id
                    + "' timing/threshold modulation is reserved; edit its millisecond/base controls directly");
        }
    }

    std::map<std::string, const Node*> nodes;
    std::unordered_map<std::string, std::vector<std::string>> adjacency;
    std::unordered_map<std::string, std::vector<std::string>> executionAdjacency;
    std::unordered_map<std::string, std::vector<std::string>> reverse;
    std::unordered_map<std::string, std::size_t> indegree;
    std::unordered_map<std::string, const Connection*> incoming;
    std::unordered_set<std::string> incidentNodes;
    std::size_t inputs = 0; std::size_t outputs = 0;
    std::size_t delayLines = 0;
    std::unordered_map<std::string, std::pair<std::size_t, std::size_t>> delaySamplePlans;
    for (const auto& node : document.nodes) {
        nodes.emplace(node.id, &node); indegree[node.id] = 0; addNodeContractErrors(node, result.errors);
        for (const auto& value : node.parameters) if (!std::isfinite(value.value))
            result.errors.push_back("parameter '" + node.id + "." + value.id + "' must be finite");
        inputs += node.type == "stereo-input"; outputs += node.type == "stereo-output";
        if (node.type == "delay" || node.type == "allpass") {
            ++delayLines;
            if (const auto* delay = parameter(node, "delay"); delay != nullptr) {
                if (delay->value <= 0.0)
                    result.errors.push_back("node '" + node.id + "' delay must be greater than zero milliseconds");
                else if (delay->value > 10'000.0)
                    result.errors.push_back("node '" + node.id + "' exceeds the 10-second delay limit");
                else if (sampleRate > 0.0 && sampleRate <= 192'000.0 && std::isfinite(delay->value)) {
                    const auto requested = std::max<std::size_t>(1,
                        static_cast<std::size_t>(std::llround(sampleRate * delay->value / 1000.0)));
                    const auto delayMapping = std::ranges::find_if(controlPlan.mappings, [&](const auto& mapping) {
                        return mapping.targetNodeId == node.id && mapping.parameterId == "delay";
                    });
                    const auto maximumDelay = delayMapping == controlPlan.mappings.end()
                        ? delay->value
                        : delayMapping->clampMaximum;
                    const auto allocated = node.type == "allpass"
                        ? std::max<std::size_t>(2, static_cast<std::size_t>(
                            std::ceil(sampleRate * std::max(100.0, maximumDelay) / 1000.0)) + 1)
                        : delayMapping == controlPlan.mappings.end()
                            ? requested
                            : std::max<std::size_t>(2, static_cast<std::size_t>(
                                std::ceil(sampleRate * maximumDelay / 1000.0)) + 1);
                    delaySamplePlans[node.id] = { requested, allocated };
                    result.delayMemory.requestedSamples += requested;
                    result.delayMemory.allocatedSamples += allocated;
                }
            }
        }
    }
    result.delayMemory.lineCount = delayLines;
    result.delayMemory.requestedBytes = result.delayMemory.requestedSamples * sizeof(float);
    result.delayMemory.allocatedBytes = result.delayMemory.allocatedSamples * sizeof(float);
    if (delayLines > 64) result.errors.push_back("patch exceeds the 64-delay-line runtime limit");
    if (!result.delayMemory.withinBudget())
        result.errors.push_back("patch requires " + std::to_string(result.delayMemory.allocatedBytes)
            + " bytes of delay memory; project budget is " + std::to_string(result.delayMemory.budgetBytes) + " bytes");
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
        const auto& targetPorts = nodes.at(connection.to.nodeId)->ports;
        const auto targetPort = std::ranges::find(targetPorts, connection.to.portId, &Port::id);
        const auto cutsDelayedAudioDependency = allowFeedback
            && nodes.at(connection.to.nodeId)->type == "delay"
            && targetPort != targetPorts.end()
            && targetPort->signal == SignalType::audio;
        if (!cutsDelayedAudioDependency) {
            executionAdjacency[connection.from.nodeId].push_back(connection.to.nodeId);
            ++indegree[connection.to.nodeId];
        }
    }
    for (const auto& node : document.nodes) {
        if (node.type != "hold-gate") continue;
        const auto gateCable = incoming.find(portKey(node.id, "gate"));
        if (gateCable == incoming.end()) continue;
        const auto source = nodes.at(gateCable->second->from.nodeId);
        if (source->type == "envelope-follower") continue;
        if (source->type == "control-map") {
            const auto mapperInput = incoming.find(portKey(source->id, "in"));
            if (mapperInput != incoming.end()
                && nodes.at(mapperInput->second->from.nodeId)->type == "envelope-follower")
                continue;
        }
        result.errors.push_back("hold-gate node '" + node.id
            + "' control must come from envelope-follower directly or through one control-map");
    }
    for (const auto& connection : document.connections) {
        const auto* source = nodes.at(connection.from.nodeId);
        const auto* target = nodes.at(connection.to.nodeId);
        if (source->type == "envelope-follower"
            && !((target->type == "hold-gate" && connection.to.portId == "gate")
                || (target->type == "control-map" && connection.to.portId == "in")))
            result.errors.push_back("envelope-follower node '" + source->id
                + "' may drive only hold-gate or one control-map");
        if (source->type == "control-map") {
            const auto mapperInput = incoming.find(portKey(source->id, "in"));
            const auto followsEnvelope = mapperInput != incoming.end()
                && nodes.at(mapperInput->second->from.nodeId)->type == "envelope-follower";
            if (followsEnvelope) {
                if (!(target->type == "hold-gate" && connection.to.portId == "gate"))
                    result.errors.push_back("an envelope-follower control-map may drive only hold-gate");
                if (std::ranges::any_of(controlPlan.mappings, [&source](const auto& mapping) {
                        return mapping.targetNodeId == source->id;
                    }))
                    result.errors.push_back("an envelope-follower control-map uses base scale/offset/polarity only");
            }
        }
    }
    if (!result.errors.empty()) return finish();

    std::priority_queue<std::string, std::vector<std::string>, std::greater<>> ready;
    for (const auto& [id, degree] : indegree) if (degree == 0) ready.push(id);
    while (!ready.empty()) {
        auto id = ready.top(); ready.pop(); result.schedule.push_back(id);
        auto targets = executionAdjacency[id]; std::ranges::sort(targets);
        for (const auto& target : targets) if (--indegree[target] == 0) ready.push(target);
    }
    if (result.schedule.size() != nodes.size()) {
        if (allowFeedback) {
            for (const auto& component : cyclicComponents(nodes, executionAdjacency)) {
                auto loop = concreteCycle(component, executionAdjacency);
                result.errors.push_back(loopMessage(loop)); result.offendingLoops.push_back(std::move(loop));
            }
        } else result.errors.push_back("acyclic compiler rejected a directed cycle; feedback compilation is provided by M3.4");
        result.schedule.clear(); return finish();
    }
    if (allowFeedback) {
        for (const auto& component : cyclicComponents(nodes, adjacency)) {
            if (std::ranges::any_of(component, [&](const auto& id) { return nodes.at(id)->type == "delay"; }))
                result.feedbackComponents.push_back(component);
        }
    }

    const auto inputId = std::ranges::find_if(document.nodes, [](const Node& node) { return node.type == "stereo-input"; })->id;
    const auto outputId = std::ranges::find_if(document.nodes, [](const Node& node) { return node.type == "stereo-output"; })->id;
    std::unordered_set<std::string> fromInput { inputId }, toOutput { outputId };
    std::vector<std::string> frontier { inputId };
    while (!frontier.empty()) { auto id = frontier.back(); frontier.pop_back(); for (const auto& next : adjacency[id]) if (fromInput.insert(next).second) frontier.push_back(next); }
    frontier = { outputId };
    while (!frontier.empty()) { auto id = frontier.back(); frontier.pop_back(); for (const auto& prior : reverse[id]) if (toOutput.insert(prior).second) frontier.push_back(prior); }
    for (const auto& [id, node] : nodes) {
        if (node->type == "lfo" || node->type == "control-map") continue;
        const auto incident = incidentNodes.contains(id);
        if (!incident && node->type != "stereo-input" && node->type != "stereo-output")
            result.warnings.push_back("disconnected node '" + id + "' processes silence and its output is discarded");
        else if (!fromInput.contains(id)) result.warnings.push_back("node '" + id + "' is unreachable from stereo input");
        else if (!toOutput.contains(id)) result.warnings.push_back("node '" + id + "' cannot reach stereo output and is discarded");
    }

    try {
        auto implementation = std::make_unique<PreparedAcyclicRuntime::Impl>();
        implementation->maximumBlockSize = maximumBlockSize; implementation->schedule = result.schedule;
        implementation->delayMemory = result.delayMemory;
        implementation->controlQuantumSamples = controlPlan.quantumSamples == 0 ? 1 : controlPlan.quantumSamples;
        implementation->delayArena.assign(result.delayMemory.allocatedSamples, 0.0F);
        implementation->feedbackMode = allowFeedback && !result.feedbackComponents.empty();
        implementation->sampleWiseMode = implementation->feedbackMode
            || std::ranges::any_of(document.nodes, [](const auto& node) {
                return node.type == "envelope-follower" || node.type == "hold-gate";
            });
        implementation->buffers.emplace_back(maximumBlockSize, 0.0F); implementation->silenceBuffer = 0;
        std::unordered_map<std::string, std::size_t> outputBuffers;
        std::size_t delayArenaOffset = 0;
        for (const auto& id : result.schedule) {
            for (const auto& port : nodes.at(id)->ports)
                if (port.direction == PortDirection::output) {
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
            if (const auto macro = std::ranges::find(controlPlan.macros, id, &ControlRatePlan::MacroNode::nodeId);
                macro != controlPlan.macros.end()) {
                ControlOperation control {
                    .id = id,
                    .kind = ControlOperationKind::macro,
                    .value = macro->value,
                    .macroKey = macro->key,
                    .macroSlot = macro->slot,
                    .macroTarget = macro->value,
                    .macroDefault = macro->defaultValue,
                    .observedMacroTarget = macro->value,
                };
                control.macroRamp.reset(macro->value);
                implementation->controlOperationById[id] = implementation->controlOperations.size();
                implementation->controlOperations.push_back(std::move(control));
            } else if (const auto lfo = std::ranges::find(controlPlan.lfos, id, &ControlRatePlan::LfoNode::nodeId);
                lfo != controlPlan.lfos.end()) {
                ControlOperation control { .id = id, .kind = ControlOperationKind::lfo };
                control.lfo.prepare(controlRateHz);
                control.lfo.configure(lfo->frequencyHz, lfo->phaseCycles, lfo->waveform, lfo->runMode);
                control.lfo.restart();
                implementation->controlOperationById[id] = implementation->controlOperations.size();
                implementation->controlOperations.push_back(std::move(control));
            } else if (const auto mapper = std::ranges::find(
                controlPlan.mappers, id, &ControlRatePlan::MapperNode::nodeId);
                mapper != controlPlan.mappers.end()) {
                ControlOperation control {
                    .id = id,
                    .kind = ControlOperationKind::mapper,
                    .scale = mapper->scale,
                    .offset = mapper->offset,
                    .polarity = mapper->inputPolarity,
                    .curveFamily = mapper->curveFamily,
                    .curveAmount = mapper->curveAmount,
                    .exponent = mapper->exponent,
                    .clampMinimum = mapper->clampMinimum,
                    .clampMaximum = mapper->clampMaximum,
                };
                if (const auto source = implementation->controlOperationById.find(mapper->sourceNodeId);
                    source != implementation->controlOperationById.end())
                    control.source = source->second;
                implementation->controlOperationById[id] = implementation->controlOperations.size();
                implementation->controlOperations.push_back(std::move(control));
            }
        }

        for (const auto& id : result.schedule) {
            const auto& node = *nodes.at(id);
            if (node.type == "macro" || node.type == "lfo" || node.type == "control-map") continue;
            Operation operation { .id = id, .kind = kindFor(node.type) };
            for (const auto& port : node.ports) {
                const auto followerPort = node.type == "envelope-follower"
                    && ((port.id == "in" && port.signal == SignalType::audio)
                        || (port.id == "out" && port.signal == SignalType::control));
                if (port.signal != SignalType::audio && !followerPort) continue;
                if (port.direction == PortDirection::input) operation.inputs.push_back(inputBuffer(id, port.id));
                else operation.outputs.push_back(outputBuffers.at(portKey(id, port.id)));
            }
            if (operation.kind == OperationKind::gain) {
                reverb::dsp::Gain processor; processor.prepare(sampleRate, static_cast<float>(parameter(node, "gain")->value), 0.0); operation.processor = std::move(processor);
            } else if (operation.kind == OperationKind::delay) {
                reverb::dsp::Delay processor;
                const auto samples = delaySamplePlans.at(id).second;
                const auto delayMapping = std::ranges::find_if(controlPlan.mappings, [&](const auto& mapping) {
                    return mapping.targetNodeId == id && mapping.parameterId == "delay";
                });
                if (delayMapping == controlPlan.mappings.end()) {
                    processor.prepare(sampleRate, parameter(node, "delay")->value,
                        std::span(implementation->delayArena).subspan(delayArenaOffset, samples));
                } else {
                    processor.prepareModulated(sampleRate, parameter(node, "delay")->value,
                        delayMapping->clampMaximum,
                        std::span(implementation->delayArena).subspan(delayArenaOffset, samples));
                }
                operation.baseDelayMilliseconds = parameter(node, "delay")->value;
                delayArenaOffset += samples; operation.processor = std::move(processor);
            } else if (operation.kind == OperationKind::allpass) {
                reverb::dsp::Allpass processor; const auto delay = parameter(node, "delay")->value;
                const auto samples = delaySamplePlans.at(id).second;
                processor.prepare(sampleRate, delay, static_cast<float>(parameter(node, "coefficient")->value),
                    std::max(100.0, delay), std::span(implementation->delayArena).subspan(delayArenaOffset, samples));
                operation.baseDelayMilliseconds = delay;
                operation.baseCoefficient = parameter(node, "coefficient")->value;
                delayArenaOffset += samples; operation.processor = std::move(processor);
            } else if (operation.kind == OperationKind::lowpass) {
                reverb::dsp::OnePoleLowPass processor; processor.prepare(sampleRate, parameter(node, "cutoff")->value); operation.processor = std::move(processor);
            } else if (operation.kind == OperationKind::envelopeFollower) {
                reverb::dsp::EnvelopeFollower processor;
                processor.prepare(sampleRate, parameter(node, "attack")->value, parameter(node, "release")->value);
                operation.processor = std::move(processor);
            } else if (operation.kind == OperationKind::holdGate) {
                reverb::dsp::HoldGate processor;
                processor.prepare(
                    sampleRate, parameter(node, "threshold")->value,
                    parameter(node, "attack")->value, parameter(node, "hold")->value,
                    parameter(node, "release")->value);
                const auto gateCable = incoming.find(portKey(id, "gate"));
                if (gateCable != incoming.end()) {
                    const auto* controlSource = nodes.at(gateCable->second->from.nodeId);
                    if (controlSource->type == "envelope-follower") {
                        operation.controlInput = outputBuffers.at(portKey(controlSource->id, "out"));
                    } else {
                        const auto mapperInput = incoming.at(portKey(controlSource->id, "in"));
                        operation.controlInput = outputBuffers.at(portKey(mapperInput->from.nodeId, "out"));
                        operation.controlScale = parameter(*controlSource, "scale")->value;
                        operation.controlOffset = parameter(*controlSource, "offset")->value;
                        operation.controlPolarity = parameter(*controlSource, "polarity")->value >= 0.5
                            ? ModulationPolarity::bipolar
                            : ModulationPolarity::unipolar;
                        if (const auto* value = parameter(*controlSource, "curve-family"))
                            operation.controlCurveFamily = value->value >= 1.5 ? ControlCurveFamily::exponential
                                : value->value >= 0.5 ? ControlCurveFamily::power
                                                      : ControlCurveFamily::linear;
                        if (const auto* value = parameter(*controlSource, "curve-amount"))
                            operation.controlCurveAmount = value->value;
                        if (const auto* value = parameter(*controlSource, "exponent"))
                            operation.controlExponent = value->value;
                        if (const auto* value = parameter(*controlSource, "clamp-min"))
                            operation.controlClampMinimum = value->value;
                        if (const auto* value = parameter(*controlSource, "clamp-max"))
                            operation.controlClampMaximum = value->value;
                    }
                }
                operation.processor = std::move(processor);
            } else if (operation.kind == OperationKind::sum) {
                if (const auto* gain = parameter(node, "gain")) operation.sumGain = static_cast<float>(gain->value);
            }
            implementation->operations.push_back(std::move(operation));
        }
        for (const auto& mapping : controlPlan.mappings) {
            if (mapping.parameterId != "delay" && mapping.parameterId != "coefficient") continue;
            const auto operation = std::ranges::find(
                implementation->operations, mapping.targetNodeId, &Operation::id);
            if (operation == implementation->operations.end()
                || (operation->kind != OperationKind::delay && operation->kind != OperationKind::allpass))
                continue;
            RuntimeModulation runtime { .mapping = mapping };
            runtime.ramp.reset(mapping.baseValue);
            runtime.values.assign(maximumBlockSize, mapping.baseValue);
            if (const auto source = implementation->controlOperationById.find(mapping.sourceNodeId);
                source != implementation->controlOperationById.end())
                runtime.source = source->second;
            const auto index = implementation->modulations.size();
            implementation->modulations.push_back(std::move(runtime));
            if (mapping.parameterId == "delay") operation->delayModulation = index;
            else operation->coefficientModulation = index;
        }
        result.runtime = std::unique_ptr<PreparedAcyclicRuntime>(new PreparedAcyclicRuntime(std::move(implementation)));
    } catch (const std::exception& exception) {
        result.errors.push_back(std::string("runtime preparation failed: ") + exception.what());
        result.runtime.reset();
    }
    return finish();
}

AcyclicCompileResult compileFeedbackGraph(
    const GraphDocument& document, const double sampleRate, const std::size_t maximumBlockSize)
{
    return compileAcyclicGraph(document, sampleRate, maximumBlockSize, true);
}

struct AcyclicRuntimeHost::RuntimeEnvelope final {
    std::unique_ptr<PreparedAcyclicRuntime> runtime;
    std::uint64_t revision {};
    std::size_t crossfadeSamples {};
    std::vector<float> crossfadeLeft;
    std::vector<float> crossfadeRight;
};

struct AcyclicRuntimeHost::CompilationRequest final {
    GraphDocument document;
    double sampleRate {};
    std::size_t maximumBlockSize {};
    bool allowFeedback {};
    std::uint64_t revision {};
};

AcyclicRuntimeHost::AcyclicRuntimeHost()
    : compilerThread_([this](const std::stop_token token) { compilerLoop(token); })
{
    static_assert(std::atomic<RuntimeEnvelope*>::is_always_lock_free);
    static_assert(std::atomic<std::size_t>::is_always_lock_free);
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
}

AcyclicRuntimeHost::~AcyclicRuntimeHost()
{
    compilerThread_.request_stop();
    requestCondition_.notify_all();
    if (compilerThread_.joinable()) compilerThread_.join();
    delete pendingRuntime_.exchange(nullptr, std::memory_order_acq_rel);
    delete activeRuntime_.exchange(nullptr, std::memory_order_acq_rel);
    delete fadingRuntime_;
    reclaimRetired();
}

AcyclicPublishResult AcyclicRuntimeHost::compileAndPublish(
    const GraphDocument& document, const double sampleRate, const std::size_t maximumBlockSize)
{
    const auto revision = requestedRevision_.fetch_add(1, std::memory_order_acq_rel) + 1;
    return publishCompiled(compileAcyclicGraph(document, sampleRate, maximumBlockSize), revision, sampleRate);
}

AcyclicPublishResult AcyclicRuntimeHost::compileFeedbackAndPublish(
    const GraphDocument& document, const double sampleRate, const std::size_t maximumBlockSize)
{
    const auto revision = requestedRevision_.fetch_add(1, std::memory_order_acq_rel) + 1;
    return publishCompiled(compileFeedbackGraph(document, sampleRate, maximumBlockSize), revision, sampleRate);
}

AcyclicPublishResult AcyclicRuntimeHost::publishCompiled(
    AcyclicCompileResult result, const std::uint64_t revision, const double sampleRate)
{
    AcyclicPublishResult publication { result.schedule, result.warnings, result.errors, result.delayMemory };
    completedCompilations_.fetch_add(1, std::memory_order_relaxed);
    if (!result.valid()) {
        failedRevision_.store(revision, std::memory_order_release);
        std::scoped_lock lock(failureMutex_);
        failure_.clear();
        for (std::size_t index = 0; index < result.errors.size(); ++index) {
            if (index != 0) failure_ += "; ";
            failure_ += result.errors[index];
        }
        return publication;
    }
    publishPending(std::move(result.runtime), revision, sampleRate);
    return publication;
}

std::uint64_t AcyclicRuntimeHost::requestCompilation(
    GraphDocument document,
    const double sampleRate,
    const std::size_t maximumBlockSize,
    const bool allowFeedback)
{
    const auto revision = requestedRevision_.fetch_add(1, std::memory_order_acq_rel) + 1;
    {
        std::scoped_lock lock(requestMutex_);
        if (latestRequest_ != nullptr)
            supersededRequests_.fetch_add(1, std::memory_order_relaxed);
        latestRequest_ = std::make_unique<CompilationRequest>(CompilationRequest {
            std::move(document), sampleRate, maximumBlockSize, allowFeedback, revision });
    }
    requestCondition_.notify_one();
    return revision;
}

void AcyclicRuntimeHost::compilerLoop(const std::stop_token stopToken)
{
    while (!stopToken.stop_requested()) {
        reclaimRetired();
        std::unique_ptr<CompilationRequest> request;
        {
            std::unique_lock lock(requestMutex_);
            requestCondition_.wait_for(lock, stopToken, std::chrono::milliseconds(2), [this] {
                return latestRequest_ != nullptr;
            });
            if (stopToken.stop_requested()) break;
            request = std::move(latestRequest_);
        }
        if (request == nullptr) continue;
        auto result = compileAcyclicGraph(
            request->document, request->sampleRate, request->maximumBlockSize, request->allowFeedback);
        completedCompilations_.fetch_add(1, std::memory_order_relaxed);
        {
            std::scoped_lock lock(requestMutex_);
            if (requestedRevision_.load(std::memory_order_acquire) > request->revision
                || (latestRequest_ != nullptr && latestRequest_->revision > request->revision)) {
                supersededRequests_.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
        }
        if (result.valid()) {
            publishPending(std::move(result.runtime), request->revision, request->sampleRate);
        } else {
            failedRevision_.store(request->revision, std::memory_order_release);
            std::scoped_lock lock(failureMutex_);
            failure_.clear();
            for (std::size_t index = 0; index < result.errors.size(); ++index) {
                if (index != 0) failure_ += "; ";
                failure_ += result.errors[index];
            }
        }
    }
    reclaimRetired();
}

void AcyclicRuntimeHost::publishPending(
    std::unique_ptr<PreparedAcyclicRuntime> runtime,
    const std::uint64_t revision,
    const double sampleRate)
{
    std::scoped_lock lock(pendingPublicationMutex_);
    const auto maximumBlockSize = runtime->maximumBlockSize();
    const auto crossfadeSamples = static_cast<std::size_t>(std::max(
        1.0, std::round(sampleRate * topologyCrossfadeMilliseconds / 1'000.0)));
    auto* envelope = new RuntimeEnvelope {
        std::move(runtime), revision, crossfadeSamples,
        std::vector<float>(maximumBlockSize), std::vector<float>(maximumBlockSize) };
    const auto activeRevision = activeRevision_.load(std::memory_order_acquire);
    auto* currentPending = pendingRuntime_.load(std::memory_order_acquire);
    if (revision <= activeRevision
        || (currentPending != nullptr
            && revision <= pendingRevision_.load(std::memory_order_acquire))) {
        delete envelope;
        supersededRequests_.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    auto* superseded = pendingRuntime_.exchange(envelope, std::memory_order_acq_rel);
    pendingRevision_.store(revision, std::memory_order_release);
    if (superseded != nullptr) {
        delete superseded;
        supersededRequests_.fetch_add(1, std::memory_order_relaxed);
    }
}

bool AcyclicRuntimeHost::retirementHasCapacity() const noexcept
{
    const auto write = retirementWrite_.load(std::memory_order_relaxed);
    const auto next = (write + 1) % retirementCapacity;
    return next != retirementRead_.load(std::memory_order_acquire);
}

void AcyclicRuntimeHost::retire(RuntimeEnvelope* const runtime) noexcept
{
    const auto write = retirementWrite_.load(std::memory_order_relaxed);
    retired_[write] = runtime;
    retirementWrite_.store((write + 1) % retirementCapacity, std::memory_order_release);
}

void AcyclicRuntimeHost::reclaimRetired() noexcept
{
    auto read = retirementRead_.load(std::memory_order_relaxed);
    const auto write = retirementWrite_.load(std::memory_order_acquire);
    while (read != write) {
        delete retired_[read];
        retired_[read] = nullptr;
        read = (read + 1) % retirementCapacity;
        reclaimedRuntimes_.fetch_add(1, std::memory_order_relaxed);
    }
    retirementRead_.store(read, std::memory_order_release);
}

TopologyPublicationSnapshot AcyclicRuntimeHost::publicationSnapshot() const
{
    TopologyPublicationSnapshot snapshot;
    snapshot.requestedRevision = requestedRevision_.load(std::memory_order_acquire);
    snapshot.pendingRevision = pendingRuntime_.load(std::memory_order_acquire) == nullptr
        ? 0
        : pendingRevision_.load(std::memory_order_acquire);
    snapshot.activeRevision = activeRevision_.load(std::memory_order_acquire);
    snapshot.failedRevision = failedRevision_.load(std::memory_order_acquire);
    snapshot.supersededRequests = supersededRequests_.load(std::memory_order_acquire);
    snapshot.completedCompilations = completedCompilations_.load(std::memory_order_acquire);
    snapshot.reclaimedRuntimes = reclaimedRuntimes_.load(std::memory_order_acquire);
    snapshot.crossfadeFromRevision = crossfadeFromRevision_.load(std::memory_order_acquire);
    snapshot.crossfadePositionSamples = crossfadePositionSamples_.load(std::memory_order_acquire);
    snapshot.crossfadeTotalSamples = crossfadeTotalSamples_.load(std::memory_order_acquire);
    snapshot.completedCrossfades = completedCrossfades_.load(std::memory_order_acquire);
    snapshot.lastCrossfadeFromRevision = lastCrossfadeFromRevision_.load(std::memory_order_acquire);
    snapshot.lastCrossfadeToRevision = lastCrossfadeToRevision_.load(std::memory_order_acquire);
    snapshot.activeDelayLineCount = activeDelayLineCount_.load(std::memory_order_acquire);
    snapshot.activeDelayMemoryBytes = activeDelayMemoryBytes_.load(std::memory_order_acquire);
    std::scoped_lock lock(failureMutex_);
    snapshot.failure = failure_;
    return snapshot;
}

void AcyclicRuntimeHost::process(
    const std::span<const float> inputLeft, const std::span<const float> inputRight,
    const std::span<float> outputLeft, const std::span<float> outputRight) noexcept
{
    auto* pending = pendingRuntime_.load(std::memory_order_acquire);
    auto* active = activeRuntime_.load(std::memory_order_acquire);
    if (fadingRuntime_ == nullptr && pending != nullptr && (active == nullptr || retirementHasCapacity())) {
        pending = pendingRuntime_.exchange(nullptr, std::memory_order_acq_rel);
        if (pending != nullptr) {
            active = activeRuntime_.exchange(pending, std::memory_order_acq_rel);
            activeRevision_.store(pending->revision, std::memory_order_release);
            activeDelayLineCount_.store(
                pending->runtime->delayMemoryPlan().lineCount, std::memory_order_release);
            activeDelayMemoryBytes_.store(
                pending->runtime->delayMemoryPlan().allocatedBytes, std::memory_order_release);
            if (pendingRuntime_.load(std::memory_order_acquire) == nullptr)
                pendingRevision_.store(0, std::memory_order_release);
            if (active != nullptr) {
                fadingRuntime_ = active;
                crossfadePosition_ = 0;
                crossfadeFromRevision_.store(active->revision, std::memory_order_release);
                crossfadePositionSamples_.store(0, std::memory_order_release);
                crossfadeTotalSamples_.store(pending->crossfadeSamples, std::memory_order_release);
            }
            active = pending;
        }
    }
    if (active == nullptr) {
        std::ranges::fill(outputLeft, 0.0F);
        std::ranges::fill(outputRight, 0.0F);
        return;
    }
    const auto applyMacros = [this](PreparedAcyclicRuntime& runtime) noexcept {
        for (std::size_t slot = 0; slot < maximumMacroControls; ++slot) {
            const auto key = macroKeys_[slot].load(std::memory_order_acquire);
            if (key != 0)
                runtime.applyMacroValue(
                    slot, key, macroValues_[slot].load(std::memory_order_acquire));
        }
    };
    applyMacros(*active->runtime);
    if (fadingRuntime_ != nullptr)
        applyMacros(*fadingRuntime_->runtime);
    if (fadingRuntime_ == nullptr) {
        active->runtime->process(inputLeft, inputRight, outputLeft, outputRight);
        return;
    }

    const auto count = outputLeft.size();
    const auto sizesMatch = inputLeft.size() == count && inputRight.size() == count
        && outputRight.size() == count && count <= active->crossfadeLeft.size();
    if (sizesMatch) {
        const auto nextLeft = std::span(active->crossfadeLeft).first(count);
        const auto nextRight = std::span(active->crossfadeRight).first(count);
        active->runtime->process(inputLeft, inputRight, nextLeft, nextRight);
        fadingRuntime_->runtime->process(inputLeft, inputRight, outputLeft, outputRight);
        const auto total = active->crossfadeSamples;
        for (std::size_t index = 0; index < count; ++index) {
            const auto alpha = std::min(1.0F, static_cast<float>(crossfadePosition_ + index + 1)
                / static_cast<float>(total));
            outputLeft[index] += alpha * (nextLeft[index] - outputLeft[index]);
            outputRight[index] += alpha * (nextRight[index] - outputRight[index]);
        }
        crossfadePosition_ = std::min(total, crossfadePosition_ + count);
        crossfadePositionSamples_.store(crossfadePosition_, std::memory_order_release);
    } else {
        active->runtime->process(inputLeft, inputRight, outputLeft, outputRight);
        crossfadePosition_ = active->crossfadeSamples;
    }

    if (crossfadePosition_ >= active->crossfadeSamples) {
        lastCrossfadeFromRevision_.store(fadingRuntime_->revision, std::memory_order_release);
        lastCrossfadeToRevision_.store(active->revision, std::memory_order_release);
        completedCrossfades_.fetch_add(1, std::memory_order_relaxed);
        retire(fadingRuntime_);
        fadingRuntime_ = nullptr;
        crossfadePosition_ = 0;
        crossfadeFromRevision_.store(0, std::memory_order_release);
        crossfadePositionSamples_.store(0, std::memory_order_release);
        crossfadeTotalSamples_.store(0, std::memory_order_release);
    }
}

void AcyclicRuntimeHost::resetActiveRuntimes() noexcept
{
    for (auto& key : macroKeys_)
        key.store(0, std::memory_order_release);
    if (auto* active = activeRuntime_.load(std::memory_order_acquire)) active->runtime->reset();
    if (fadingRuntime_ != nullptr) fadingRuntime_->runtime->reset();
}

bool AcyclicRuntimeHost::hasRuntime() const noexcept
{
    return activeRuntime_.load(std::memory_order_acquire) != nullptr
        || pendingRuntime_.load(std::memory_order_acquire) != nullptr;
}

std::uint64_t AcyclicRuntimeHost::activeRevision() const noexcept
{
    return activeRevision_.load(std::memory_order_acquire);
}

bool AcyclicRuntimeHost::setMacroValue(const std::string_view nodeId, const double value) noexcept
{
    if (nodeId.empty() || !std::isfinite(value))
        return false;
    const auto key = macroControlKey(nodeId);
    const auto slot = macroControlSlot(key);
    macroValues_[slot].store(std::clamp(value, -1.0, 1.0), std::memory_order_release);
    macroKeys_[slot].store(key, std::memory_order_release);
    return true;
}

} // namespace reverb::graph
