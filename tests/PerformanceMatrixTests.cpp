#include <catch2/catch_test_macros.hpp>

#include <reverb/render/PerformanceMatrix.h>

#include <nlohmann/json.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <set>

TEST_CASE("Headless performance matrix separates normal and topology-crossfade measurements")
{
    const auto result = reverb::render::measurePerformanceCase(
        { "barr-reference", 48'000.0, 128, 8, 5 });
    REQUIRE(result.request.graphId == "barr-reference");
    REQUIRE(result.normal.medianMicroseconds > 0.0);
    REQUIRE(result.normal.sampleCount == 8);
    REQUIRE(result.normal.percentile95Microseconds >= result.normal.medianMicroseconds);
    REQUIRE(result.normal.peakMicroseconds >= result.normal.percentile95Microseconds);
    REQUIRE(result.topologyCrossfade.medianMicroseconds > 0.0);
    REQUIRE(result.topologyCrossfade.sampleCount >= 5);
    REQUIRE(result.crossfadeMedianOverheadRatio > 0.0);
    REQUIRE(result.nodeCount > 0);
    REQUIRE(result.connectionCount > 0);
    REQUIRE(result.preparedMemoryBytes > 0);
    REQUIRE(result.requestToActiveMicroseconds > 0);
    REQUIRE(result.finiteOutput);

    const auto document = nlohmann::json::parse(reverb::render::performanceMatrixJson(
        { result }, "test machine", "test toolchain", "test commit"));
    REQUIRE(document.at("formatVersion") == 1);
    REQUIRE(document.at("scope").get<std::string>().find("not cross-machine comparable")
        != std::string::npos);
    REQUIRE(document.at("cases").size() == 1);
    const auto& measured = document.at("cases").front();
    REQUIRE(measured.contains("normal"));
    REQUIRE(measured.contains("topologyCrossfade"));
    REQUIRE(measured.at("normal").contains("percentile95LoadPercent"));
    REQUIRE(measured.at("topologyCrossfade").at("sampleCount").get<std::size_t>() >= 5);
    REQUIRE(measured.at("compile").contains("requestToActiveMicroseconds"));
    REQUIRE(measured.at("graph").contains("latencySamples"));
    REQUIRE(measured.at("budgets").contains("withinNormalBudget"));
}

TEST_CASE("Published performance baseline covers every flagship graph rate and block size")
{
    const auto path = std::filesystem::path(REVERB_MEASUREMENTS_DIR)
        / "performance-matrix-v1.json";
    std::ifstream stream(path);
    REQUIRE(stream.good());
    const auto document = nlohmann::json::parse(stream);
    REQUIRE(document.at("buildConfiguration") == "Release");
    REQUIRE(document.at("scope").get<std::string>().find("not cross-machine comparable")
        != std::string::npos);

    const std::array graphs {
        "barr-reference", "gravity-diffusion", "safe-parallel-shimmer",
        "split-feedback-shimmer", "reverse-cosmic-shimmer",
    };
    const std::array rates { 44'100, 48'000, 96'000 };
    const std::array blocks { 32, 64, 128, 256, 512 };
    std::set<std::string> cases;
    for (const auto& measured : document.at("cases")) {
        const auto key = measured.at("graphId").get<std::string>() + "/"
            + std::to_string(measured.at("sampleRate").get<int>()) + "/"
            + std::to_string(measured.at("blockSize").get<int>());
        REQUIRE(cases.insert(key).second);
        REQUIRE(measured.at("measuredBlocks").get<int>() >= 200);
        REQUIRE(measured.at("crossfadeRepetitions").get<int>() >= 20);
        REQUIRE(measured.at("topologyCrossfade").at("sampleCount").get<int>() >= 20);
        REQUIRE(measured.at("finiteOutput").get<bool>());
        REQUIRE(measured.at("graph").at("preparedMemoryBytes").get<std::size_t>() > 0);
    }
    for (const auto* graph : graphs)
        for (const auto rate : rates)
            for (const auto block : blocks)
                REQUIRE(cases.contains(std::string(graph) + "/" + std::to_string(rate)
                    + "/" + std::to_string(block)));
    REQUIRE(cases.size() == 75);
}
