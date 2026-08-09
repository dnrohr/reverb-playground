#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/ImpulseCapture.h>

#include <vector>

TEST_CASE("Impulse capture does not stop on predelay before observing a response")
{
    reverb::dsp::ImpulseCapture capture;
    capture.prepare(48'000.0);
    (void) capture.request({ .maximumLengthMilliseconds = 500.0, .stopThresholdDb = -60.0 });
    REQUIRE(capture.beginIfRequested());
    static_assert(noexcept(capture.append({}, {})));

    std::vector<float> silence(7'200, 0.0F);
    capture.append(silence, silence);
    REQUIRE(capture.state() == reverb::dsp::ImpulseCaptureState::capturing);

    std::vector<float> onset(1, 0.1F);
    capture.append(onset, onset);
    silence.resize(4'799);
    capture.append(silence, silence);
    REQUIRE(capture.state() == reverb::dsp::ImpulseCaptureState::capturing);
    silence.resize(1);
    capture.append(silence, silence);

    REQUIRE(capture.state() == reverb::dsp::ImpulseCaptureState::complete);
    const auto result = capture.copyLatest();
    REQUIRE(result.stoppedAtThreshold);
    REQUIRE(result.left.size() == 12'001);
}

TEST_CASE("Impulse capture maximum length is a hard frame bound")
{
    reverb::dsp::ImpulseCapture capture;
    capture.prepare(48'000.0);
    (void) capture.request({ .maximumLengthMilliseconds = 100.0, .stopThresholdDb = -120.0 });
    REQUIRE(capture.beginIfRequested());
    std::vector<float> signal(8'000, 0.1F);
    capture.append(signal, signal);
    const auto result = capture.copyLatest();
    REQUIRE_FALSE(result.stoppedAtThreshold);
    REQUIRE(result.left.size() == 4'800);
    REQUIRE(result.right.size() == 4'800);
}
