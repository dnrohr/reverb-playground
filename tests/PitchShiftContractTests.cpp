#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/PitchShiftContract.h>

#include <array>
#include <cmath>

TEST_CASE("Pitch shift semitones map to the bounded musical ratio")
{
    using Catch::Approx;
    using reverb::dsp::pitch_shift::ratioForSemitones;
    REQUIRE(ratioForSemitones(-24.0) == Approx(0.5));
    REQUIRE(ratioForSemitones(-12.0) == Approx(0.5));
    REQUIRE(ratioForSemitones(0.0) == Approx(1.0));
    REQUIRE(ratioForSemitones(12.0) == Approx(2.0));
    REQUIRE(ratioForSemitones(7.0) == Approx(std::pow(2.0, 7.0 / 12.0)));
    REQUIRE(ratioForSemitones(24.0) == Approx(2.0));
    REQUIRE(ratioForSemitones(-100.0) == Approx(0.5));
    REQUIRE(ratioForSemitones(100.0) == Approx(2.0));
    REQUIRE(ratioForSemitones(std::nan("")) == Approx(2.0));
}

TEST_CASE("Pitch shift fixed latency and storage cover the worst reverse grain")
{
    using namespace reverb::dsp::pitch_shift;
    REQUIRE(maximumReadExcursionMilliseconds() == Catch::Approx(360.0));
    const std::array expectedLatency { 15'878ULL, 17'282ULL, 34'562ULL };
    const std::array expectedStorage { 31'756ULL, 34'564ULL, 69'124ULL };
    for (std::size_t index = 0; index < qualificationSampleRates.size(); ++index) {
        CAPTURE(qualificationSampleRates[index]);
        REQUIRE(reportedLatencySamples(qualificationSampleRates[index]) == expectedLatency[index]);
        REQUIRE(preparedStorageSamples(qualificationSampleRates[index]) == expectedStorage[index]);
        REQUIRE(preparedStorageBytes(qualificationSampleRates[index]) == expectedStorage[index] * sizeof(float));
    }
    REQUIRE(reportedLatencySamples(maximumPreparationSampleRate) == 69'122);
    REQUIRE(preparedStorageSamples(maximumPreparationSampleRate) == 138'244);
    REQUIRE(preparedStorageBytes(maximumPreparationSampleRate) == 552'976);
    REQUIRE(preparedStorageSamples(0.0) == 0);
    REQUIRE(preparedStorageSamples(std::nan("")) == 0);
}

TEST_CASE("Pitch shift operation ceilings include parameter and topology transitions")
{
    using namespace reverb::dsp::pitch_shift;
    REQUIRE(steadyScalarOperationsPerSample < parameterTransitionScalarOperationsPerSample);
    REQUIRE(topologyTransitionScalarOperationsPerSample
        == parameterTransitionScalarOperationsPerSample * 2);
    REQUIRE(worstCaseBlockOperations(64) == 8'768);
    REQUIRE(worstCaseBlockOperations(1'024) == 139'328);
    REQUIRE(minimumOverlap < defaultOverlap);
    REQUIRE(defaultOverlap < maximumOverlap);
    REQUIRE(minimumGrainMilliseconds < defaultGrainMilliseconds);
    REQUIRE(defaultGrainMilliseconds < maximumGrainMilliseconds);
}
