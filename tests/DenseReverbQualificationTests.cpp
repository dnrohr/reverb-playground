#include <catch2/catch_test_macros.hpp>

#include <reverb/render/DenseReverbQualification.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>
#include <string>

TEST_CASE("Dense qualification separates objective metrics across programs settings and designs")
{
    const auto report = reverb::render::qualifyDenseReverbs();
    REQUIRE(report.sampleRate == 48'000.0);
    REQUIRE(report.blockSize == 128);
    REQUIRE(report.cases.size() == 32);
    std::set<std::string> keys;
    for (const auto& measured : report.cases) {
        const auto key = std::string(reverb::render::denseProgramName(measured.program)) + "/"
            + reverb::render::denseSettingName(measured.setting) + "/" + measured.designId;
        REQUIRE(keys.insert(key).second);
        REQUIRE(measured.finite);
        REQUIRE(measured.renderedRmsDb <= -23.9);
        REQUIRE(measured.normalizedPeak <= 0.500001);
        REQUIRE(measured.echoDensity >= 0.0);
        REQUIRE(measured.echoDensity <= 1.0);
        REQUIRE(measured.recurrence >= 0.0);
        REQUIRE(measured.recurrence <= 1.0);
        REQUIRE(measured.monoEnergyRatio >= 0.0);
        REQUIRE(measured.monoEnergyRatio <= 1.000001);
    }
    const auto document = nlohmann::json::parse(
        reverb::render::denseQualificationJson(report, "test-commit"));
    REQUIRE(document.at("measurement") == "dense-reverb-product-qualification");
    REQUIRE(document.at("buildCommit") == "test-commit");
    REQUIRE(document.at("cases").size() == 32);
    REQUIRE(document.contains("thresholds"));
    REQUIRE(document.contains("listeningOrder"));
}
