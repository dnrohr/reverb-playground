#include <reverb/dsp/EnergyTelemetry.h>
#include <reverb/dsp/LiveReferenceHarness.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>

using Catch::Matchers::WithinAbs;

TEST_CASE("Energy telemetry is fixed-rate, coherent, and free when disabled")
{
    reverb::dsp::EnergyTelemetry telemetry;
    telemetry.prepare(300.0);

    std::array<float, 5> mono {};
    mono[2] = 1.0F;
    REQUIRE_FALSE(telemetry.beginBlock());
    auto disabled = telemetry.snapshot();
    REQUIRE_FALSE(disabled.enabled);
    REQUIRE(disabled.generation == 0);
    REQUIRE(disabled.observedSampleValues == 0);

    telemetry.setEnabled(true);
    REQUIRE(telemetry.beginBlock());
    telemetry.observeMono(reverb::dsp::BarrEnergyLane::sum, mono);
    telemetry.observeStereo(reverb::dsp::BarrEnergyLane::output, mono, mono);
    telemetry.endBlock(mono.size());
    REQUIRE(telemetry.snapshot().generation == 0);
    REQUIRE(telemetry.beginBlock());
    telemetry.observeMono(reverb::dsp::BarrEnergyLane::sum, mono);
    telemetry.observeStereo(reverb::dsp::BarrEnergyLane::output, mono, mono);
    telemetry.endBlock(mono.size());
    const auto enabled = telemetry.snapshot();
    REQUIRE(enabled.enabled);
    REQUIRE(enabled.coherent);
    REQUIRE(enabled.generation == 1);
    REQUIRE(enabled.observedSampleValues == 30);
    REQUIRE_THAT(enabled.rms[static_cast<std::size_t>(reverb::dsp::BarrEnergyLane::sum)],
        WithinAbs(std::sqrt(0.2F), 1.0e-6F));
    REQUIRE_THAT(enabled.rms[static_cast<std::size_t>(reverb::dsp::BarrEnergyLane::output)],
        WithinAbs(std::sqrt(0.2F), 1.0e-6F));
}

TEST_CASE("Polling or dropping telemetry snapshots cannot change rendered audio")
{
    reverb::dsp::LiveReferenceHarness ignored;
    reverb::dsp::LiveReferenceHarness polled;
    ignored.prepare(48'000.0);
    polled.prepare(48'000.0);
    ignored.setEnergyTelemetryEnabled(true);
    polled.setEnergyTelemetryEnabled(true);
    ignored.triggerImpulse();
    polled.triggerImpulse();

    std::array<float, 64> silence {};
    std::array<float, 64> ignoredLeft {}, ignoredRight {}, polledLeft {}, polledRight {};
    for (int block = 0; block < 40; ++block) {
        ignored.process(silence, silence, ignoredLeft, ignoredRight);
        polled.process(silence, silence, polledLeft, polledRight);
        if (block % 3 == 0)
            REQUIRE(polled.energyTelemetrySnapshot().coherent);
        REQUIRE(ignoredLeft == polledLeft);
        REQUIRE(ignoredRight == polledRight);
    }
    REQUIRE(ignored.energyTelemetrySnapshot().generation > 0);
    REQUIRE(polled.energyTelemetrySnapshot().generation > 0);
}
