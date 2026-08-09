#include <reverb/dsp/LiveReferenceHarness.h>
#include <reverb/dsp/RuntimeDiagnostics.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

TEST_CASE("Runtime diagnostics distinguish prepared estimates from live measurements")
{
    reverb::dsp::RuntimeDiagnostics diagnostics;
    diagnostics.prepare(48'000.0, 6, 12'345);
    const auto started = diagnostics.beginBlock();
    diagnostics.endBlock(started, 480, 7);
    const auto snapshot = diagnostics.snapshot();

    REQUIRE(reverb::dsp::RuntimeDiagnostics::estimatedScalarOperationsPerSample == 48);
    REQUIRE(snapshot.sampleRate == 48'000.0);
    REQUIRE(snapshot.delayLineCount == 6);
    REQUIRE(snapshot.delayMemoryBytes == 12'345);
    REQUIRE(snapshot.processedBlocks == 1);
    REQUIRE(std::isfinite(snapshot.liveLoadPercent));
    REQUIRE(snapshot.liveLoadPercent >= 0.0F);
    REQUIRE(snapshot.peakLoadPercent >= snapshot.liveLoadPercent);
    REQUIRE(snapshot.clippedSamples == 7);
    REQUIRE(snapshot.clippedBlocks == 1);
}

TEST_CASE("NaN infinity and runaway events retain their active revision through recovery")
{
    reverb::dsp::LiveReferenceHarness harness;
    harness.prepare(48'000.0);
    harness.setMasterGain(1.0F);
    harness.setRuntimeParameter(reverb::dsp::BarrParameterId::diffuserOneCoefficient, 0.4);
    std::array<float, 64> left {}, right {}, outputLeft {}, outputRight {};

    left[0] = std::numeric_limits<float>::quiet_NaN();
    harness.process(left, right, outputLeft, outputRight);
    auto diagnostic = harness.runtimeDiagnosticsSnapshot();
    REQUIRE(harness.isSafetyLatched());
    REQUIRE(diagnostic.safetyEventCoherent);
    REQUIRE(diagnostic.lastViolation == reverb::dsp::SafetyViolation::nonFinite);
    REQUIRE(diagnostic.lastViolationRevision == 2);
    REQUIRE(diagnostic.safetyEventGeneration == 1);
    REQUIRE(std::ranges::all_of(outputLeft, [](const float value) { return value == 0.0F; }));
    REQUIRE(std::ranges::all_of(outputRight, [](const float value) { return value == 0.0F; }));

    harness.setRuntimeParameter(reverb::dsp::BarrParameterId::sumGain, 0.25);
    harness.setMasterGain(0.1F);
    REQUIRE(harness.masterGain() == 0.1F);
    REQUIRE(harness.runtimeDiagnosticsSnapshot().activeRevision == 3);
    REQUIRE(harness.runtimeDiagnosticsSnapshot().lastViolationRevision == 2);
    left.fill(0.0F);
    harness.process(left, right, outputLeft, outputRight);
    REQUIRE(harness.isSafetyLatched());
    REQUIRE(std::ranges::all_of(outputLeft, [](const float value) { return value == 0.0F; }));
    harness.requestSafetyReset();
    harness.process(left, right, outputLeft, outputRight);
    diagnostic = harness.runtimeDiagnosticsSnapshot();
    REQUIRE_FALSE(harness.isSafetyLatched());
    REQUIRE(diagnostic.recoveryCount == 1);
    REQUIRE(diagnostic.lastViolationRevision == 2);

    left[0] = std::numeric_limits<float>::infinity();
    harness.process(left, right, outputLeft, outputRight);
    diagnostic = harness.runtimeDiagnosticsSnapshot();
    REQUIRE(diagnostic.lastViolation == reverb::dsp::SafetyViolation::nonFinite);
    REQUIRE(diagnostic.lastViolationRevision == 3);
    REQUIRE(diagnostic.safetyEventGeneration == 2);
    REQUIRE(std::ranges::all_of(outputLeft, [](const float value) { return value == 0.0F; }));
    REQUIRE(std::ranges::all_of(outputRight, [](const float value) { return value == 0.0F; }));

    harness.requestSafetyReset();
    left.fill(0.0F);
    harness.process(left, right, outputLeft, outputRight);
    left.fill(1.0e10F);
    right.fill(1.0e10F);
    harness.process(left, right, outputLeft, outputRight);
    diagnostic = harness.runtimeDiagnosticsSnapshot();
    REQUIRE(diagnostic.lastViolation == reverb::dsp::SafetyViolation::runawayLevel);
    REQUIRE(diagnostic.lastViolationRevision == 3);
    REQUIRE(diagnostic.safetyEventGeneration == 3);
    REQUIRE(diagnostic.clippedSamples > 0);
    REQUIRE(std::ranges::all_of(outputLeft, [](const float value) { return value == 0.0F; }));
    REQUIRE(std::ranges::all_of(outputRight, [](const float value) { return value == 0.0F; }));
}
