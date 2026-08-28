#include <catch2/catch_test_macros.hpp>

#include <reverb/render/DenseReverbQualification.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
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
        REQUIRE(measured.spectralRippleDb >= 0.0);
        REQUIRE(measured.spectralRippleDb <= 60.0);
        REQUIRE(measured.monoEnergyRatio >= 0.0);
        REQUIRE(measured.monoEnergyRatio <= 1.000001);
    }
    const auto document = nlohmann::json::parse(
        reverb::render::denseQualificationJson(report, "test-commit"));
    REQUIRE(document.at("measurement") == "dense-reverb-product-qualification");
    REQUIRE(document.at("buildCommit") == "test-commit");
    REQUIRE(document.at("cases").size() == 32);
    REQUIRE(document.contains("thresholds"));
    REQUIRE(document.at("thresholds").at("maximumSpectralRippleDb") == 12.0);
    REQUIRE(document.contains("listeningOrder"));
}

TEST_CASE("Published dense qualification keeps objective failures visible for retuning")
{
    const auto directory = std::filesystem::path(REVERB_MEASUREMENTS_DIR)
        / "m24-dense-qualification";
    std::ifstream stream(directory / "objective-report.json");
    REQUIRE(stream.good());
    const auto report = nlohmann::json::parse(stream);
    REQUIRE(report.at("buildCommit") == "b970de54a813");
    REQUIRE(report.at("cases").size() == 32);
    std::size_t failedDimensions = 0;
    double barrDensity = 0.0;
    double gravityDensity = 0.0;
    double figureEightDensity = 0.0;
    double fdnDensity = 0.0;
    for (const auto& measured : report.at("cases")) {
        REQUIRE(measured.at("objective").at("finite").get<bool>());
        const auto density = measured.at("objective").at("echoDensity").get<double>();
        const auto design = measured.at("designId").get<std::string>();
        if (design == "barr-reference") barrDensity += density;
        else if (design == "gravity-diffusion") gravityDensity += density;
        else if (design == "dense-figure-eight") figureEightDensity += density;
        else if (design == "four-line-fdn") fdnDensity += density;
        for (const auto& [_, passed] : measured.at("passes").items())
            failedDimensions += static_cast<std::size_t>(!passed.get<bool>());
    }
    REQUIRE(figureEightDensity > barrDensity + 2.0);
    REQUIRE(fdnDensity > barrDensity + 2.0);
    REQUIRE(figureEightDensity > gravityDensity + 2.0);
    REQUIRE(fdnDensity > gravityDensity + 2.0);
    REQUIRE(failedDimensions > 0);
    for (const auto& name : { "percussion-room", "speech-room", "piano-hall", "pad-hall",
             "noise-dark", "speech-dark", "piano-modulated", "pad-modulated" })
        REQUIRE(std::filesystem::file_size(directory / (std::string(name) + "-comparison.wav")) > 1'000);
}
