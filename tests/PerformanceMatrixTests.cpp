#include <catch2/catch_test_macros.hpp>

#include <reverb/render/PerformanceMatrix.h>

#include <nlohmann/json.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

TEST_CASE("Headless performance matrix separates normal and topology-crossfade measurements")
{
    const auto result = reverb::render::measurePerformanceCase(
        { "barr-reference", 48'000.0, 128, 8, 5 });
    REQUIRE(result.request.graphId == "barr-reference");
    REQUIRE(result.normal.medianMicroseconds > 0.0);
    REQUIRE(result.normal.sampleCount == 8);
    REQUIRE(result.telemetryEnabled.sampleCount == 8);
    REQUIRE(result.telemetryEnabled.medianMicroseconds > 0.0);
    REQUIRE(result.telemetryMedianOverheadRatio > 0.0);
    REQUIRE(result.normal.percentile95Microseconds >= result.normal.medianMicroseconds);
    REQUIRE(result.normal.peakMicroseconds >= result.normal.percentile95Microseconds);
    REQUIRE(result.topologyCrossfade.medianMicroseconds > 0.0);
    REQUIRE(result.topologyCrossfade.sampleCount >= 5);
    REQUIRE(result.crossfadeMedianOverheadRatio > 0.0);
    REQUIRE(result.nodeCount > 0);
    REQUIRE(result.connectionCount > 0);
    REQUIRE(result.preparedMemoryBytes > 0);
    REQUIRE(result.delayMemoryBytes > 0);
    REQUIRE(result.requestToActiveMicroseconds > 0);
    REQUIRE(result.finiteOutput);
    REQUIRE_FALSE(result.processorFamilies.empty());
    REQUIRE_FALSE(result.dominantProcessorFamily.empty());

    const auto document = nlohmann::json::parse(reverb::render::performanceMatrixJson(
        { result }, "test machine", "test toolchain", "test commit"));
    REQUIRE(document.at("formatVersion") == 1);
    REQUIRE(document.at("scope").get<std::string>().find("not cross-machine comparable")
        != std::string::npos);
    REQUIRE(document.at("cases").size() == 1);
    const auto& measured = document.at("cases").front();
    REQUIRE(measured.contains("normal"));
    REQUIRE(measured.contains("telemetryEnabled"));
    REQUIRE(measured.contains("topologyCrossfade"));
    REQUIRE(measured.at("normal").contains("percentile95LoadPercent"));
    REQUIRE(measured.at("topologyCrossfade").at("sampleCount").get<std::size_t>() >= 5);
    REQUIRE(measured.at("compile").contains("requestToActiveMicroseconds"));
    REQUIRE(measured.at("graph").contains("latencySamples"));
    REQUIRE(measured.at("graph").contains("processorFamilies"));
    REQUIRE(measured.at("budgets").contains("withinNormalBudget"));
}

TEST_CASE("Barr direct and optimized generic execution use identical paired fixtures")
{
    const auto result = reverb::render::measureBarrExecutionComparison(48'000.0, 128, 20);
    REQUIRE(result.directReference.sampleCount == 20);
    REQUIRE(result.optimizedGeneric.sampleCount == 20);
    REQUIRE(result.directReference.percentile95Microseconds > 0.0);
    REQUIRE(result.optimizedGeneric.percentile95Microseconds > 0.0);
    REQUIRE(result.genericToDirectP95Ratio > 0.0);
    REQUIRE(result.sampleEquivalent);
    REQUIRE(result.finiteOutput);
    const auto json = nlohmann::json::parse(reverb::render::barrExecutionComparisonJson(
        { result }, "test", "test", "test"));
    REQUIRE(json.at("measurement") == "barr-direct-versus-optimized-generic");
    REQUIRE(json.at("cases").front().at("sampleEquivalent") == true);
}

TEST_CASE("Published M18 specialization evidence covers the supported Barr envelope")
{
    std::ifstream stream(std::filesystem::path(REVERB_MEASUREMENTS_DIR)
        / "barr-execution-comparison-m18-4.json");
    REQUIRE(stream.good());
    const auto report = nlohmann::json::parse(stream);
    REQUIRE(report.at("measurement") == "barr-direct-versus-optimized-generic");
    REQUIRE(report.at("buildConfiguration") == "Release");
    REQUIRE(report.at("cases").size() == 15);
    for (const auto& measured : report.at("cases")) {
        REQUIRE(measured.at("measuredBlocks").get<std::size_t>() >= 2'000);
        REQUIRE(measured.at("sampleEquivalent").get<bool>());
        REQUIRE(measured.at("finiteOutput").get<bool>());
        REQUIRE(measured.at("directReference").at("underruns") == 0);
        REQUIRE(measured.at("optimizedGeneric").at("underruns") == 0);
    }
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

TEST_CASE("M17 hybrid comparison publishes region counts for the complete matrix")
{
    const auto path = std::filesystem::path(REVERB_MEASUREMENTS_DIR)
        / "performance-matrix-m17-1.json";
    std::ifstream stream(path);
    REQUIRE(stream.good());
    const auto document = nlohmann::json::parse(stream);
    REQUIRE(document.at("cases").size() == 75);
    std::size_t hybridCases = 0;
    for (const auto& measured : document.at("cases")) {
        const auto& graph = measured.at("graph");
        REQUIRE(graph.contains("blockWiseRegionCount"));
        REQUIRE(graph.contains("sampleWiseRegionCount"));
        if (graph.at("executionDomain") == "hybrid") {
            ++hybridCases;
            REQUIRE(graph.at("blockWiseRegionCount").get<std::size_t>() > 0);
            REQUIRE(graph.at("sampleWiseRegionCount").get<std::size_t>() > 0);
        }
        REQUIRE(measured.at("finiteOutput").get<bool>());
        REQUIRE(measured.at("budgets").at("withinNormalBudget").get<bool>());
        REQUIRE(measured.at("budgets").at("withinCrossfadeBudget").get<bool>());
    }
    REQUIRE(hybridCases == 60);
}

TEST_CASE("M17 buffer-liveness comparison reduces storage without changing delay arenas")
{
    const auto directory = std::filesystem::path(REVERB_MEASUREMENTS_DIR);
    std::ifstream baselineStream(directory / "performance-matrix-m17-1.json");
    std::ifstream optimizedStream(directory / "performance-matrix-m17-2.json");
    REQUIRE(baselineStream.good());
    REQUIRE(optimizedStream.good());
    const auto baseline = nlohmann::json::parse(baselineStream);
    const auto optimized = nlohmann::json::parse(optimizedStream);
    REQUIRE(optimized.at("cases").size() == 75);

    std::map<std::string, const nlohmann::json*> baselineCases;
    for (const auto& measured : baseline.at("cases")) {
        const auto key = measured.at("graphId").get<std::string>() + "/"
            + std::to_string(measured.at("sampleRate").get<int>()) + "/"
            + std::to_string(measured.at("blockSize").get<int>());
        baselineCases.emplace(key, &measured);
    }

    for (const auto& measured : optimized.at("cases")) {
        const auto key = measured.at("graphId").get<std::string>() + "/"
            + std::to_string(measured.at("sampleRate").get<int>()) + "/"
            + std::to_string(measured.at("blockSize").get<int>());
        const auto baselineCase = baselineCases.at(key);
        const auto& graph = measured.at("graph");
        const auto& baselineGraph = baselineCase->at("graph");
        REQUIRE(graph.at("physicalAudioBufferCount").get<std::size_t>()
            <= graph.at("logicalAudioBufferCount").get<std::size_t>());
        REQUIRE(graph.at("logicalAudioBufferCount").get<std::size_t>()
            <= graph.at("logicalSignalCount").get<std::size_t>());
        REQUIRE(graph.at("peakLiveBufferCount").get<std::size_t>()
            <= graph.at("physicalAudioBufferCount").get<std::size_t>());
        REQUIRE(graph.at("bufferBytesSaved").get<std::size_t>() > 0);
        REQUIRE(graph.at("copiesAvoided").get<std::size_t>() > 0);
        REQUIRE(graph.at("preparedMemoryBytes").get<std::size_t>()
            <= baselineGraph.at("preparedMemoryBytes").get<std::size_t>());
        REQUIRE(graph.at("preparedMemoryBytes").get<std::size_t>()
            + graph.at("bufferBytesSaved").get<std::size_t>()
            == baselineGraph.at("preparedMemoryBytes").get<std::size_t>());
        REQUIRE(graph.at("delayMemoryBytes").get<std::size_t>() > 0);
        REQUIRE(measured.at("finiteOutput").get<bool>());
        REQUIRE(measured.at("budgets").at("withinNormalBudget").get<bool>());
        REQUIRE(measured.at("budgets").at("withinCrossfadeBudget").get<bool>());
    }
}

TEST_CASE("M17 fused-kernel comparison covers flagship block paths without memory regression")
{
    const auto directory = std::filesystem::path(REVERB_MEASUREMENTS_DIR);
    std::ifstream baselineStream(directory / "performance-matrix-m17-2.json");
    std::ifstream optimizedStream(directory / "performance-matrix-m17-3.json");
    REQUIRE(baselineStream.good());
    REQUIRE(optimizedStream.good());
    const auto baseline = nlohmann::json::parse(baselineStream);
    const auto optimized = nlohmann::json::parse(optimizedStream);
    REQUIRE(optimized.at("cases").size() == 75);

    std::map<std::string, const nlohmann::json*> baselineCases;
    for (const auto& measured : baseline.at("cases")) {
        const auto key = measured.at("graphId").get<std::string>() + "/"
            + std::to_string(measured.at("sampleRate").get<int>()) + "/"
            + std::to_string(measured.at("blockSize").get<int>());
        baselineCases.emplace(key, &measured);
    }

    std::size_t shimmerComparisonCases = 0;
    for (const auto& measured : optimized.at("cases")) {
        const auto key = measured.at("graphId").get<std::string>() + "/"
            + std::to_string(measured.at("sampleRate").get<int>()) + "/"
            + std::to_string(measured.at("blockSize").get<int>());
        const auto& graph = measured.at("graph");
        const auto& baselineGraph = baselineCases.at(key)->at("graph");
        REQUIRE(graph.at("fusedKernelCount").get<std::size_t>()
            <= graph.at("fusedNodeCount").get<std::size_t>());
        REQUIRE(graph.at("simdKernelCount").get<std::size_t>() > 0);
        REQUIRE(graph.at("preparedMemoryBytes").get<std::size_t>()
            <= baselineGraph.at("preparedMemoryBytes").get<std::size_t>());
        REQUIRE(graph.at("delayMemoryBytes") == baselineGraph.at("delayMemoryBytes"));
        REQUIRE(measured.at("finiteOutput").get<bool>());
        REQUIRE(measured.at("budgets").at("withinNormalBudget").get<bool>());
        REQUIRE(measured.at("budgets").at("withinCrossfadeBudget").get<bool>());

        const auto graphId = measured.at("graphId").get<std::string>();
        const auto sampleRate = measured.at("sampleRate").get<int>();
        if ((graphId == "split-feedback-shimmer" || graphId == "reverse-cosmic-shimmer")
            && (sampleRate == 48'000 || sampleRate == 96'000)) {
            ++shimmerComparisonCases;
            REQUIRE(graph.at("fusedKernelCount").get<std::size_t>() > 0);
            REQUIRE(graph.at("fusedNodeCount").get<std::size_t>() >= 2);
        }
    }
    REQUIRE(shimmerComparisonCases == 20);
}

TEST_CASE("Published M23 dense profile identifies a dominant family at every supported rate and block")
{
    const auto path = std::filesystem::path(REVERB_MEASUREMENTS_DIR)
        / "dense-network-profile-m23-1.json";
    std::ifstream stream(path);
    REQUIRE(stream.good());
    const auto document = nlohmann::json::parse(stream);
    REQUIRE(document.at("buildConfiguration") == "Release");
    REQUIRE(document.at("buildCommit") == "69660a4832bf");
    REQUIRE(document.at("familyAttribution").get<std::string>().find("not independent stopwatch")
        != std::string::npos);
    REQUIRE(document.at("cases").size() == 30);

    const std::array graphs { "dense-figure-eight", "four-line-fdn" };
    const std::array rates { 44'100, 48'000, 96'000 };
    const std::array blocks { 32, 64, 128, 256, 512 };
    std::set<std::string> cases;
    for (const auto& measured : document.at("cases")) {
        const auto key = measured.at("graphId").get<std::string>() + "/"
            + std::to_string(measured.at("sampleRate").get<int>()) + "/"
            + std::to_string(measured.at("blockSize").get<int>());
        REQUIRE(cases.insert(key).second);
        REQUIRE(measured.at("measuredBlocks").get<std::size_t>() == 1'000);
        REQUIRE(measured.at("normal").at("underruns") == 0);
        REQUIRE(measured.at("telemetryEnabled").at("sampleCount") == 1'000);
        REQUIRE(measured.at("telemetryMedianOverheadRatio").get<double>() > 0.0);
        REQUIRE(measured.at("topologyCrossfade").at("sampleCount").get<std::size_t>() >= 20);
        REQUIRE(measured.at("finiteOutput").get<bool>());
        REQUIRE(measured.at("budgets").at("withinNormalBudget").get<bool>());
        const auto& graph = measured.at("graph");
        REQUIRE_FALSE(graph.at("dominantProcessorFamily").get<std::string>().empty());
        REQUIRE(graph.at("processorFamilies").size() >= 4);
        double share = 0.0;
        for (const auto& family : graph.at("processorFamilies")) {
            REQUIRE(family.at("modelUnitsPerSample").get<std::size_t>() > 0);
            REQUIRE(family.at("attributedPercentile95Microseconds").get<double>() > 0.0);
            share += family.at("attributedSharePercent").get<double>();
        }
        REQUIRE(share > 99.9999);
        REQUIRE(share < 100.0001);
    }
    for (const auto* graph : graphs)
        for (const auto rate : rates)
            for (const auto block : blocks)
                REQUIRE(cases.contains(std::string(graph) + "/" + std::to_string(rate)
                    + "/" + std::to_string(block)));
}
