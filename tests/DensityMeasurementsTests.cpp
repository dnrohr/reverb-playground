#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <reverb/render/DensityMeasurements.h>

#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

namespace {

std::pair<std::vector<float>, std::vector<float>> fixture(const double rate, const bool dense)
{
    const auto frames = static_cast<std::size_t>(rate);
    std::vector<float> left(frames), right(frames);
    if (!dense) {
        for (auto ms : { 40, 113, 229, 401, 677, 887 }) {
            const auto frame = static_cast<std::size_t>(rate * ms / 1000.0);
            left[frame] = 0.7F; right[frame] = ms % 2 ? -0.3F : 0.4F;
        }
        return { left, right };
    }
    std::uint32_t state = 0x9462a35dU;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        state ^= state << 13U; state ^= state >> 17U; state ^= state << 5U;
        const auto noise = (static_cast<double>(state) / 4'294'967'295.0 * 2.0 - 1.0);
        const auto envelope = std::exp(-3.0 * static_cast<double>(frame) / frames);
        left[frame] = static_cast<float>(noise * envelope * 0.2);
        right[frame] = static_cast<float>((0.37 * noise + 0.63 * std::sin(frame * 0.173)) * envelope * 0.2);
    }
    return { left, right };
}

} // namespace

TEST_CASE("Density measurements distinguish controlled sparse and dense responses across rates")
{
    for (const auto rate : { 44'100.0, 48'000.0, 96'000.0 }) {
        const auto [sparseLeft, sparseRight] = fixture(rate, false);
        const auto [denseLeft, denseRight] = fixture(rate, true);
        const auto sparse = reverb::render::measureDensity(sparseLeft, sparseRight, rate);
        const auto dense = reverb::render::measureDensity(denseLeft, denseRight, rate);
        REQUIRE(sparse.regions.size() == 3);
        REQUIRE(dense.regions.size() == 3);
        REQUIRE(dense.regions[1].echoDensity > sparse.regions[1].echoDensity + 0.5);
        REQUIRE(dense.regions[1].activePeaksPerSecond > sparse.regions[1].activePeaksPerSecond * 10.0);
        REQUIRE(dense.regions[1].crestFactor < sparse.regions[1].crestFactor);
        REQUIRE(dense.regions[1].kurtosis < sparse.regions[1].kurtosis);
        REQUIRE(std::abs(dense.regions[1].spectralFlatness - sparse.regions[1].spectralFlatness) > 0.2);
    }
}

TEST_CASE("Density measurements are rate-stable and serialize every perceptual dimension")
{
    std::array<reverb::render::DensityMeasurements, 3> values;
    std::size_t index = 0;
    for (const auto rate : { 44'100.0, 48'000.0, 96'000.0 }) {
        const auto [left, right] = fixture(rate, true);
        values[index++] = reverb::render::measureDensity(left, right, rate);
    }
    for (std::size_t i = 1; i < values.size(); ++i) {
        REQUIRE(values[i].regions[1].echoDensity == Catch::Approx(values[0].regions[1].echoDensity).margin(0.03));
        REQUIRE(values[i].regions[1].crestFactor == Catch::Approx(values[0].regions[1].crestFactor).margin(0.15));
        REQUIRE(values[i].regions[1].kurtosis == Catch::Approx(values[0].regions[1].kurtosis).margin(0.2));
        REQUIRE(values[i].regions[1].stereoCorrelation == Catch::Approx(values[0].regions[1].stereoCorrelation).margin(0.03));
    }
    values[0].engineVersion = "test"; values[0].patchId = "dense-fixture";
    const auto json = nlohmann::json::parse(reverb::render::writeDensityMeasurementsJson(values[0]));
    REQUIRE(json.at("measurementVersion") == 1);
    REQUIRE(json.at("regions").size() == 3);
    for (const auto& name : { "echoDensity", "activePeaksPerSecond", "crestFactor", "kurtosis",
            "energyVariation", "recurrence", "recurrenceMilliseconds", "spectralFlatness", "stereoCorrelation" })
        REQUIRE(json.at("regions").at(1).contains(name));
}

TEST_CASE("Versioned density baseline covers every released factory at every qualified rate")
{
    std::ifstream stream(std::filesystem::path { REVERB_MEASUREMENTS_DIR } / "factory-density-baseline-v1.json",
        std::ios::binary);
    REQUIRE(stream.good());
    const auto report = nlohmann::json::parse(stream);
    REQUIRE(report.at("reportVersion") == 1);
    REQUIRE(report.at("analysis") == "perceptual-density-v1");
    REQUIRE(report.at("entries").size() == 24);
    std::set<std::string> factories;
    std::set<int> rates;
    for (const auto& entry : report.at("entries")) {
        factories.insert(entry.at("patchId").get<std::string>());
        rates.insert(entry.at("sampleRate").get<int>());
        REQUIRE(entry.at("regions").size() == 3);
        REQUIRE(entry.at("windows").size() > 50);
        for (const auto& region : entry.at("regions")) {
            REQUIRE(std::isfinite(region.at("echoDensity").get<double>()));
            REQUIRE(std::isfinite(region.at("recurrence").get<double>()));
            REQUIRE(std::isfinite(region.at("spectralFlatness").get<double>()));
            REQUIRE(std::isfinite(region.at("stereoCorrelation").get<double>()));
        }
    }
    REQUIRE(factories == std::set<std::string> { "barr-reference", "causal-reverse-envelope",
        "gravity-diffusion", "level-gated-room", "modulated-cosmic-reverse", "reverse-cosmic-shimmer",
        "safe-parallel-shimmer", "split-feedback-shimmer" });
    REQUIRE(rates == std::set<int> { 44'100, 48'000, 96'000 });
}
