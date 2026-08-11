#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/graph/ControlModulation.h>
#include <reverb/graph/ControlRate.h>
#include <reverb/graph/GraphDocument.h>

#include <cmath>

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
