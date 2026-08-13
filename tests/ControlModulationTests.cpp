#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/ControlModulation.h>
#include <reverb/graph/ControlRate.h>
#include <reverb/graph/GraphDocument.h>
#include <reverb/graph/PatchJson.h>

#include <cmath>
#include <limits>

TEST_CASE("Sine and triangle LFO frequency phase and waveforms are deterministic")
{
    using namespace reverb::graph;
    ControlLfo sine;
    sine.prepare(8.0);
    sine.configure(1.0, 0.25, LfoWaveform::sine, LfoRunMode::restart);
    sine.restart();
    REQUIRE(sine.next() == Catch::Approx(1.0));
    REQUIRE(sine.next() == Catch::Approx(std::sqrt(0.5)));

    ControlLfo triangle;
    triangle.prepare(4.0);
    triangle.configure(1.0, 0.0, LfoWaveform::triangle, LfoRunMode::restart);
    triangle.restart();
    REQUIRE(triangle.next() == Catch::Approx(-1.0));
    REQUIRE(triangle.next() == Catch::Approx(0.0));
    REQUIRE(triangle.next() == Catch::Approx(1.0));
    REQUIRE(triangle.next() == Catch::Approx(0.0));
}

TEST_CASE("LFO reset distinguishes restart from free-run")
{
    using namespace reverb::graph;
    ControlLfo restart;
    restart.prepare(8.0);
    restart.configure(1.0, 0.25, LfoWaveform::sine, LfoRunMode::restart);
    restart.restart();
    static_cast<void>(restart.next());
    restart.reset();
    REQUIRE(restart.phase() == Catch::Approx(0.25));

    ControlLfo freeRun;
    freeRun.prepare(8.0);
    freeRun.configure(1.0, 0.25, LfoWaveform::sine, LfoRunMode::freeRun);
    freeRun.restart();
    static_cast<void>(freeRun.next());
    const auto runningPhase = freeRun.phase();
    freeRun.reset();
    REQUIRE(freeRun.phase() == Catch::Approx(runningPhase));
}

TEST_CASE("Explicit scale offset and polarity mapping predicts its bounded output range")
{
    using namespace reverb::graph;
    REQUIRE(mapControlValue(-1.0, 0.25, 0.5, ModulationPolarity::bipolar) == Catch::Approx(0.25));
    REQUIRE(mapControlValue(1.0, 0.25, 0.5, ModulationPolarity::bipolar) == Catch::Approx(0.75));
    REQUIRE(mappedControlRange(0.25, 0.5, ModulationPolarity::bipolar).minimum == Catch::Approx(0.25));
    REQUIRE(mappedControlRange(0.25, 0.5, ModulationPolarity::bipolar).maximum == Catch::Approx(0.75));
    REQUIRE(mappedControlRange(-2.0, 0.25, ModulationPolarity::unipolar).minimum == Catch::Approx(-1.0));
    REQUIRE(mappedControlRange(-2.0, 0.25, ModulationPolarity::unipolar).maximum == Catch::Approx(0.25));
}

TEST_CASE("Curve mapper families are finite monotonic and linear-compatible")
{
    using namespace reverb::graph;
    for (const auto polarity : { ModulationPolarity::unipolar, ModulationPolarity::bipolar }) {
        const auto lower = polarity == ModulationPolarity::bipolar ? -1.0 : 0.0;
        double previousPower = -2.0;
        double previousExponential = -2.0;
        for (int index = 0; index <= 100; ++index) {
            const auto input = lower + (1.0 - lower) * static_cast<double>(index) / 100.0;
            const auto linear = mapControlValue(input, 0.75, 0.1, polarity);
            const auto extendedLinear = mapControlValue(
                input, ControlCurveFamily::linear, 5.0, 3.0, 0.75, 0.1,
                polarity, -1.0, 1.0);
            const auto power = mapControlValue(
                input, ControlCurveFamily::power, 0.0, 2.5, 0.75, 0.1,
                polarity, -1.0, 1.0);
            const auto exponential = mapControlValue(
                input, ControlCurveFamily::exponential, 4.0, 1.0, 0.75, 0.1,
                polarity, -1.0, 1.0);
            REQUIRE(extendedLinear == Catch::Approx(linear));
            REQUIRE(std::isfinite(power));
            REQUIRE(std::isfinite(exponential));
            REQUIRE(power >= previousPower);
            REQUIRE(exponential >= previousExponential);
            previousPower = power;
            previousExponential = exponential;
        }
    }
    const auto range = mappedControlRange(
        ControlCurveFamily::power, 0.0, 2.0, -2.0, 0.25,
        ModulationPolarity::unipolar, -0.6, 0.8);
    REQUIRE(range.minimum == Catch::Approx(-0.6));
    REQUIRE(range.maximum == Catch::Approx(0.25));
}

TEST_CASE("Curve mapper plan persists every field and rejects invalid curves before publication")
{
    using namespace reverb::graph;
    const auto mapper = [](const double family, const double amount, const double exponent,
                            const double clampMinimum, const double clampMaximum) {
        return Node { "map", "control-map", {
            { "in", SignalType::control, PortDirection::input },
            { "out", SignalType::control, PortDirection::output },
        }, {
            { "scale", 0.8, "linear", {} }, { "offset", -0.1, "unitless", {} },
            { "polarity", 1.0, "polarity", {} }, { "curve-family", family, "curve-family", {} },
            { "curve-amount", amount, "unitless", {} }, { "exponent", exponent, "unitless", {} },
            { "clamp-min", clampMinimum, "unitless", {} }, { "clamp-max", clampMaximum, "unitless", {} },
        } };
    };

    GraphDocument valid;
    valid.nodes = { mapper(2.0, -3.5, 2.25, -0.75, 0.9) };
    const auto plan = compileControlRatePlan(valid, 48'000.0, 512);
    REQUIRE(plan.valid());
    REQUIRE(plan.mappers.front().curveFamily == ControlCurveFamily::exponential);
    REQUIRE(plan.mappers.front().curveAmount == Catch::Approx(-3.5));
    REQUIRE(plan.mappers.front().exponent == Catch::Approx(2.25));
    REQUIRE(plan.mappers.front().clampMinimum == Catch::Approx(-0.75));
    REQUIRE(plan.mappers.front().clampMaximum == Catch::Approx(0.9));
    const auto serialized = writePatchJson(valid);
    const auto restored = parsePatchJson(serialized);
    REQUIRE(restored == valid);
    REQUIRE(writePatchJson(restored) == serialized);

    GraphDocument unsupported;
    unsupported.nodes = { mapper(3.0, 0.0, 1.0, -1.0, 1.0) };
    REQUIRE_FALSE(compileControlRatePlan(unsupported, 48'000.0, 512).valid());
    GraphDocument reversedClamp;
    reversedClamp.nodes = { mapper(1.0, 0.0, 2.0, 0.5, -0.5) };
    REQUIRE_FALSE(compileControlRatePlan(reversedClamp, 48'000.0, 512).valid());
    GraphDocument nonFinite;
    nonFinite.nodes = { mapper(1.0, std::numeric_limits<double>::infinity(), 2.0, -1.0, 1.0) };
    REQUIRE_FALSE(compileControlRatePlan(nonFinite, 48'000.0, 512).valid());
}

TEST_CASE("One control output branches to multiple parameter sockets")
{
    using namespace reverb::graph;
    GraphDocument document;
    document.nodes = {
        { "lfo", "lfo", { { "out", SignalType::control, PortDirection::output } }, {} },
        { "a", "gain", { { "gain-mod", SignalType::control, PortDirection::input } }, {} },
        { "b", "delay", { { "delay-mod", SignalType::control, PortDirection::input } }, {} },
    };
    document.connections = {
        { "branch-a", { "lfo", "out" }, { "a", "gain-mod" } },
        { "branch-b", { "lfo", "out" }, { "b", "delay-mod" } },
    };
    REQUIRE(validate(document).valid());
}

TEST_CASE("Macro exposes a bounded named source and branches to the fixed control limit")
{
    using namespace reverb::graph;
    GraphDocument document;
    document.nodes.push_back({ "macro-1", "macro", {
        { "out", SignalType::control, PortDirection::output },
    }, {
        { "value", 0.25, "normalized" }, { "default-value", 0.0, "normalized" },
        { "center-detent", 1.0, "boolean" },
    }, "Gravity" });
    for (std::size_t index = 0; index < 63; ++index) {
        const auto id = "target-" + std::to_string(index);
        document.nodes.push_back({ id, "delay", {
            { "delay-mod", SignalType::control, PortDirection::input },
        }, {
            { "delay", 10.0, "milliseconds", ParameterModulation {
                "delay-mod", 2.0, ModulationPolarity::bipolar, 0.1, 100.0 } },
        } });
        document.connections.push_back({ "branch-" + std::to_string(index),
            { "macro-1", "out" }, { id, "delay-mod" } });
    }
    const auto plan = compileControlRatePlan(document, 48'000.0, 512);
    REQUIRE(plan.valid());
    REQUIRE(plan.macros.size() == 1);
    REQUIRE(plan.macros.front().value == Catch::Approx(0.25));
    REQUIRE(plan.macros.front().defaultValue == Catch::Approx(0.0));
    REQUIRE(plan.macros.front().centerDetent);
    REQUIRE(plan.mappings.size() == 63);

    document.nodes.push_back({ "target-over-limit", "delay", {
        { "delay-mod", SignalType::control, PortDirection::input },
    }, {
        { "delay", 10.0, "milliseconds", ParameterModulation {
            "delay-mod", 2.0, ModulationPolarity::bipolar, 0.1, 100.0 } },
    } });
    document.connections.push_back({ "branch-over-limit", { "macro-1", "out" },
        { "target-over-limit", "delay-mod" } });
    REQUIRE_FALSE(compileControlRatePlan(document, 48'000.0, 512).valid());
}

TEST_CASE("Macro invalid routes and malformed controls fail before publication")
{
    using namespace reverb::graph;
    GraphDocument invalidRoute;
    invalidRoute.nodes = {
        { "macro", "macro", { { "out", SignalType::control, PortDirection::output } }, {
            { "value", 0.0, "normalized" }, { "default-value", 0.0, "normalized" },
            { "center-detent", 1.0, "boolean" },
        }, "Named" },
        { "gain", "gain", { { "in", SignalType::audio, PortDirection::input } }, {} },
    };
    invalidRoute.connections = { { "wrong-type", { "macro", "out" }, { "gain", "in" } } };
    REQUIRE_FALSE(validate(invalidRoute).valid());

    auto malformed = invalidRoute;
    malformed.connections.clear();
    malformed.nodes.resize(1);
    malformed.nodes.front().name.clear();
    malformed.nodes.front().parameters.front().value = 2.0;
    REQUIRE_FALSE(compileControlRatePlan(malformed, 48'000.0, 512).valid());
}

TEST_CASE("Control-rate compilation prepares LFO and mapping block semantics")
{
    using namespace reverb::graph;
    GraphDocument document;
    document.nodes = {
        { "lfo", "lfo", { { "out", SignalType::control, PortDirection::output } }, {
            { "frequency", 0.5, "hertz", {} }, { "phase", 0.25, "cycles", {} },
            { "waveform", 1.0, "waveform", {} }, { "run-mode", 1.0, "run-mode", {} },
        } },
        { "map", "control-map", {
            { "in", SignalType::control, PortDirection::input },
            { "out", SignalType::control, PortDirection::output },
        }, {
            { "scale", -0.5, "linear", {} }, { "offset", 0.25, "unitless", {} },
            { "polarity", 1.0, "polarity", {} },
        } },
    };
    document.connections = { { "lfo-map", { "lfo", "out" }, { "map", "in" } } };
    const auto plan = compileControlRatePlan(document, 48'000.0, 512);
    REQUIRE(plan.valid());
    REQUIRE(plan.lfos.size() == 1);
    REQUIRE(plan.lfos.front().waveform == LfoWaveform::triangle);
    REQUIRE(plan.lfos.front().runMode == LfoRunMode::restart);
    REQUIRE(plan.mappers.size() == 1);
    REQUIRE(plan.mappers.front().sourceNodeId == "lfo");
    REQUIRE(plan.mappers.front().predictedRange.minimum == Catch::Approx(-0.25));
    REQUIRE(plan.mappers.front().predictedRange.maximum == Catch::Approx(0.75));
}
