#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/render/ResponseMeasurements.h>
#include <reverb/render/OfflineRenderer.h>
#include <reverb/graph/BarrReferenceGraph.h>

#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

TEST_CASE("Response measurements recover known exponential RT60")
{
    constexpr double sampleRate = 48'000.0;
    constexpr double expectedRt60 = 0.75;
    std::vector<float> left(96'000, 0.0F);
    std::vector<float> right(left.size(), 0.0F);
    for (std::size_t frame = 240; frame < left.size(); ++frame) {
        const auto time = static_cast<double>(frame - 240) / sampleRate;
        left[frame] = static_cast<float>(std::pow(10.0, -3.0 * time / expectedRt60));
        right[frame] = left[frame] * 0.5F;
    }

    const auto result = reverb::render::measureResponse(left, right, sampleRate);

    REQUIRE(result.onsetFrame == 240);
    REQUIRE(result.lastActiveFrame.has_value());
    REQUIRE(result.impulseLengthFrames == *result.lastActiveFrame - 239);
    REQUIRE(result.peakLeft == 1.0);
    REQUIRE(result.peakRight == 0.5);
    REQUIRE(result.stereoDifferenceRms > 0.0);
    REQUIRE(result.rt60Seconds.has_value());
    REQUIRE(*result.rt60Seconds == Catch::Approx(expectedRt60).epsilon(0.01));
    REQUIRE(result.decayCurve.size() >= 256);

    const auto json = nlohmann::json::parse(reverb::render::writeMeasurementsJson(result));
    REQUIRE(json.at("measurementVersion") == 1);
    REQUIRE(json.at("onsetFrame") == 240);
    REQUIRE(json.at("rt60Seconds").is_number());
}

TEST_CASE("RT60 declines when range is insufficient or tail noise is too high")
{
    constexpr double sampleRate = 48'000.0;
    std::vector<float> shortLeft(2'400, 0.0F);
    std::vector<float> shortRight(shortLeft.size(), 0.0F);
    for (std::size_t frame = 0; frame < shortLeft.size(); ++frame)
        shortLeft[frame] = static_cast<float>(std::pow(10.0, -3.0 * (static_cast<double>(frame) / sampleRate)));
    REQUIRE_FALSE(reverb::render::measureResponse(shortLeft, shortRight, sampleRate).rt60Seconds.has_value());

    std::vector<float> noisyLeft(96'000, 0.001F);
    std::vector<float> noisyRight(noisyLeft.size(), -0.001F);
    noisyLeft.front() = 1.0F;
    REQUIRE_FALSE(reverb::render::measureResponse(noisyLeft, noisyRight, sampleRate).rt60Seconds.has_value());
}

TEST_CASE("Silent response has no onset length or RT60")
{
    const std::vector<float> silence(1'000, 0.0F);
    const auto result = reverb::render::measureResponse(silence, silence, 48'000.0);
    REQUIRE_FALSE(result.onsetFrame.has_value());
    REQUIRE_FALSE(result.lastActiveFrame.has_value());
    REQUIRE_FALSE(result.impulseLengthFrames.has_value());
    REQUIRE_FALSE(result.rt60Seconds.has_value());
    REQUIRE(result.peakLeft == 0.0);
}

TEST_CASE("Barr reference measurements match versioned artifact")
{
    const reverb::render::RenderRequest request {
        reverb::graph::makeBarrReferenceGraph(),
        reverb::render::InputKind::impulse,
        48'000.0,
        96'000,
    };
    const auto rendered = reverb::render::renderOffline(request);
    auto measured = reverb::render::measureResponse(rendered.left, rendered.right, request.sampleRate);
    measured.engineVersion = request.patch.engineVersion;
    measured.patchId = "barr-reference";

    const auto artifactPath = std::filesystem::path { REVERB_TEST_FIXTURES_DIR }
        / ".." / ".." / "artifacts" / "measurements" / "barr-reference-v1.json";
    std::ifstream stream(artifactPath, std::ios::binary);
    REQUIRE(stream.good());
    const auto artifact = nlohmann::json::parse(stream);

    REQUIRE(artifact.at("measurementVersion") == 1);
    REQUIRE(artifact.at("engineVersion") == measured.engineVersion);
    REQUIRE(artifact.at("patchId") == measured.patchId);
    REQUIRE(artifact.at("onsetFrame") == *measured.onsetFrame);
    REQUIRE(artifact.at("lastActiveFrame") == *measured.lastActiveFrame);
    REQUIRE(artifact.at("impulseLengthFrames") == *measured.impulseLengthFrames);
    REQUIRE(artifact.at("peakLeft").get<double>() == Catch::Approx(measured.peakLeft).epsilon(1.0e-7));
    REQUIRE(artifact.at("peakRight").get<double>() == Catch::Approx(measured.peakRight).epsilon(1.0e-7));
    REQUIRE(artifact.at("stereoDifferenceRms").get<double>()
        == Catch::Approx(measured.stereoDifferenceRms).epsilon(1.0e-7));
    REQUIRE(artifact.at("rt60Seconds").get<double>()
        == Catch::Approx(*measured.rt60Seconds).epsilon(1.0e-6));
    REQUIRE(artifact.at("decayCurve").size() == measured.decayCurve.size());
}
