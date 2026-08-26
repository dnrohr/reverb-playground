#include <reverb/render/PerformanceMatrix.h>

#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/GravityDiffusionGraph.h>
#include <reverb/graph/ReverseCosmicShimmerGraph.h>
#include <reverb/graph/SafeParallelShimmerGraph.h>
#include <reverb/graph/SplitFeedbackShimmerGraph.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

namespace reverb::render {
namespace {

using Clock = std::chrono::steady_clock;

reverb::graph::GraphDocument graphFor(const std::string& id)
{
    if (id == "barr-reference") return reverb::graph::makeBarrReferenceGraph();
    if (id == "gravity-diffusion") return reverb::graph::makeGravityDiffusionGraph();
    if (id == "safe-parallel-shimmer") return reverb::graph::makeSafeParallelShimmerGraph();
    if (id == "split-feedback-shimmer") return reverb::graph::makeSplitFeedbackShimmerGraph();
    if (id == "reverse-cosmic-shimmer") return reverb::graph::makeReverseCosmicShimmerGraph();
    throw std::invalid_argument("unknown performance graph '" + id + "'");
}

double percentile(std::vector<double> values, const double fraction)
{
    if (values.empty()) return 0.0;
    std::ranges::sort(values);
    const auto index = static_cast<std::size_t>(std::ceil(
        fraction * static_cast<double>(values.size()))) - 1;
    return values[std::min(index, values.size() - 1)];
}

CallbackDistribution distribution(
    const std::vector<double>& microseconds,
    const double deadlineMicroseconds)
{
    CallbackDistribution result;
    if (microseconds.empty()) return result;
    result.sampleCount = microseconds.size();
    result.medianMicroseconds = percentile(microseconds, 0.5);
    result.percentile95Microseconds = percentile(microseconds, 0.95);
    result.peakMicroseconds = *std::ranges::max_element(microseconds);
    const auto load = [deadlineMicroseconds](const double value) {
        return deadlineMicroseconds > 0.0 ? value * 100.0 / deadlineMicroseconds : 0.0;
    };
    result.medianLoadPercent = load(result.medianMicroseconds);
    result.percentile95LoadPercent = load(result.percentile95Microseconds);
    result.peakLoadPercent = load(result.peakMicroseconds);
    result.underruns = static_cast<std::size_t>(std::ranges::count_if(
        microseconds, [deadlineMicroseconds](const double value) { return value > deadlineMicroseconds; }));
    return result;
}

double elapsedMicroseconds(const Clock::time_point started)
{
    return std::chrono::duration<double, std::micro>(Clock::now() - started).count();
}

nlohmann::ordered_json distributionJson(const CallbackDistribution& value)
{
    return {
        { "sampleCount", value.sampleCount },
        { "medianMicroseconds", value.medianMicroseconds },
        { "percentile95Microseconds", value.percentile95Microseconds },
        { "peakMicroseconds", value.peakMicroseconds },
        { "medianLoadPercent", value.medianLoadPercent },
        { "percentile95LoadPercent", value.percentile95LoadPercent },
        { "peakLoadPercent", value.peakLoadPercent },
        { "underruns", value.underruns },
    };
}

} // namespace

PerformanceCaseResult measurePerformanceCase(const PerformanceCaseRequest& request)
{
    if (request.sampleRate <= 0.0 || request.blockSize == 0 || request.measuredBlocks < 5
        || request.crossfadeRepetitions < 5)
        throw std::invalid_argument(
            "performance case requires positive rate/block and at least five measured blocks/transitions");
    auto graph = graphFor(request.graphId);
    reverb::graph::AcyclicRuntimeHost host;
    const auto publication = host.compileFeedbackAndPublish(graph, request.sampleRate, request.blockSize);
    if (!publication.valid()) {
        std::string message;
        for (const auto& error : publication.errors) message += (message.empty() ? "" : "; ") + error;
        throw std::runtime_error("performance graph compilation failed: " + message);
    }

    std::vector<float> inputLeft(request.blockSize);
    std::vector<float> inputRight(request.blockSize);
    std::vector<float> outputLeft(request.blockSize);
    std::vector<float> outputRight(request.blockSize);
    std::uint32_t noise = 0x4d595df4U;
    for (std::size_t index = 0; index < request.blockSize; ++index) {
        noise = noise * 1664525U + 1013904223U;
        inputLeft[index] = static_cast<float>((static_cast<double>(noise)
            / static_cast<double>(std::numeric_limits<std::uint32_t>::max()) - 0.5) * 0.1);
        inputRight[index] = static_cast<float>(0.05 * std::sin(
            2.0 * 3.14159265358979323846 * 311.0 * static_cast<double>(index) / request.sampleRate));
    }
    const auto process = [&] {
        host.process(inputLeft, inputRight, outputLeft, outputRight);
        return std::ranges::all_of(outputLeft, [](const float value) { return std::isfinite(value); })
            && std::ranges::all_of(outputRight, [](const float value) { return std::isfinite(value); });
    };
    auto finite = process();
    for (auto warmup = 0; warmup < 16; ++warmup) finite = process() && finite;

    std::vector<double> normalTimes;
    normalTimes.reserve(request.measuredBlocks);
    for (std::size_t block = 0; block < request.measuredBlocks; ++block) {
        const auto started = Clock::now();
        finite = process() && finite;
        normalTimes.push_back(elapsedMicroseconds(started));
    }
    const auto normalSnapshot = host.publicationSnapshot();

    std::vector<double> crossfadeTimes;
    for (std::size_t repetition = 0; repetition < request.crossfadeRepetitions; ++repetition) {
        const auto crossfadePublication = host.compileFeedbackAndPublish(
            graph, request.sampleRate, request.blockSize);
        if (!crossfadePublication.valid()) throw std::runtime_error("crossfade graph compilation failed");
        do {
            const auto started = Clock::now();
            finite = process() && finite;
            crossfadeTimes.push_back(elapsedMicroseconds(started));
        } while (host.publicationSnapshot().crossfadeTotalSamples != 0);
    }

    const auto deadline = static_cast<double>(request.blockSize) * 1'000'000.0 / request.sampleRate;
    PerformanceCaseResult result;
    result.request = request;
    result.normal = distribution(normalTimes, deadline);
    result.topologyCrossfade = distribution(crossfadeTimes, deadline);
    result.crossfadeMedianOverheadRatio = result.normal.medianMicroseconds > 0.0
        ? result.topologyCrossfade.medianMicroseconds / result.normal.medianMicroseconds : 0.0;
    result.graphLatencySamples = normalSnapshot.activeLatency.totalSamples;
    result.preparedMemoryBytes = normalSnapshot.activePlanDiagnostics.preparedStorageBytes;
    result.nodeCount = normalSnapshot.activePlanDiagnostics.nodeCount;
    result.connectionCount = normalSnapshot.activePlanDiagnostics.connectionCount;
    result.feedbackRegionCount = normalSnapshot.activePlanDiagnostics.feedbackRegionCount;
    result.estimatedOperationsPerSample = normalSnapshot.activePlanDiagnostics.estimatedScalarOperationsPerSample;
    result.executionDomain = normalSnapshot.activePlanDiagnostics.executionDomain;
    result.validationMicroseconds = normalSnapshot.activePlanDiagnostics.compileTiming.validationMicroseconds;
    result.schedulingMicroseconds = normalSnapshot.activePlanDiagnostics.compileTiming.schedulingMicroseconds;
    result.preparationMicroseconds = normalSnapshot.activePlanDiagnostics.compileTiming.preparationMicroseconds;
    result.compileMicroseconds = normalSnapshot.activePlanDiagnostics.compileTiming.totalMicroseconds;
    result.requestToActiveMicroseconds = normalSnapshot.activeRequestToActiveMicroseconds;
    result.finiteOutput = finite;
    result.withinNormalBudget = finite && result.normal.percentile95LoadPercent <= 80.0
        && result.normal.underruns == 0;
    result.withinCrossfadeBudget = finite && result.topologyCrossfade.percentile95LoadPercent <= 160.0;
    return result;
}

std::string performanceMatrixJson(
    const std::vector<PerformanceCaseResult>& results,
    std::string machineLabel,
    std::string toolchain,
    std::string buildCommit)
{
    auto cases = nlohmann::ordered_json::array();
    for (const auto& result : results) {
        cases.push_back({
            { "graphId", result.request.graphId },
            { "sampleRate", result.request.sampleRate },
            { "blockSize", result.request.blockSize },
            { "measuredBlocks", result.request.measuredBlocks },
            { "crossfadeRepetitions", result.request.crossfadeRepetitions },
            { "normal", distributionJson(result.normal) },
            { "topologyCrossfade", distributionJson(result.topologyCrossfade) },
            { "crossfadeMedianOverheadRatio", result.crossfadeMedianOverheadRatio },
            { "compile", {
                { "validationMicroseconds", result.validationMicroseconds },
                { "schedulingMicroseconds", result.schedulingMicroseconds },
                { "preparationMicroseconds", result.preparationMicroseconds },
                { "totalMicroseconds", result.compileMicroseconds },
                { "requestToActiveMicroseconds", result.requestToActiveMicroseconds },
            } },
            { "graph", {
                { "latencySamples", result.graphLatencySamples },
                { "preparedMemoryBytes", result.preparedMemoryBytes },
                { "nodeCount", result.nodeCount },
                { "connectionCount", result.connectionCount },
                { "feedbackRegionCount", result.feedbackRegionCount },
                { "estimatedOperationsPerSample", result.estimatedOperationsPerSample },
                { "executionDomain", result.executionDomain },
            } },
            { "finiteOutput", result.finiteOutput },
            { "budgets", {
                { "normalP95LoadPercentMaximum", 80.0 },
                { "crossfadeP95LoadPercentMaximum", 160.0 },
                { "withinNormalBudget", result.withinNormalBudget },
                { "withinCrossfadeBudget", result.withinCrossfadeBudget },
            } },
        });
    }
    const nlohmann::ordered_json document {
        { "formatVersion", 1 },
        { "scope", "single-machine pre-optimization baseline; values are not cross-machine comparable" },
        { "machine", std::move(machineLabel) },
        { "toolchain", std::move(toolchain) },
        { "buildConfiguration", "Release" },
        { "buildCommit", std::move(buildCommit) },
        { "source", "deterministic LCG noise plus 311 Hz sine; source preparation excluded from callback timing" },
        { "cases", std::move(cases) },
    };
    return document.dump(2) + "\n";
}

void writePerformanceMatrix(
    const std::filesystem::path& output,
    const std::vector<PerformanceCaseResult>& results,
    std::string machineLabel,
    std::string toolchain,
    std::string buildCommit)
{
    std::ofstream stream(output, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("could not open performance output");
    stream << performanceMatrixJson(results, std::move(machineLabel), std::move(toolchain), std::move(buildCommit));
    if (!stream) throw std::runtime_error("could not write performance output");
}

} // namespace reverb::render
