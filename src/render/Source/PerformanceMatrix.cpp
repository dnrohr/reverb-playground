#include <reverb/render/PerformanceMatrix.h>

#include <reverb/dsp/BarrReference.h>
#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/DenseFigureEightGraph.h>
#include <reverb/graph/FourLineFdnGraph.h>
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
    if (id == "dense-figure-eight") return reverb::graph::makeDenseFigureEightGraph();
    if (id == "four-line-fdn") return reverb::graph::makeFourLineFdnGraph();
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

std::vector<ProcessorFamilyCost> attributeProcessorFamilies(
    const reverb::graph::PreparedGraphDiagnostics& diagnostics,
    const std::size_t connectionCount,
    const CallbackDistribution& normal)
{
    std::map<std::string, ProcessorFamilyCost> costs;
    const auto add = [&](const std::string& family, const std::size_t nodes, const std::size_t units) {
        auto& cost = costs[family];
        cost.family = family;
        cost.nodeCount += nodes;
        cost.modelUnitsPerSample += units;
    };
    for (const auto& workload : diagnostics.workloadFamilies) {
        const auto family = workload.family == "gain" || workload.family == "sum" ? "matrix"
            : workload.family == "delay" || workload.family == "allpass" ? "delay"
            : workload.family == "lowpass" ? "damping"
            : workload.family == "lfo" || workload.family == "control-map"
                || workload.family == "macro" ? "modulation"
            : workload.family == "sample-wise-dispatch" ? "routing"
            : "other";
        add(family, workload.nodeCount, workload.estimatedScalarOperationsPerSample);
    }
    // Cable traversal, live-buffer movement, and final input/output publication are real work
    // that is not represented by a processor node's arithmetic estimate.
    add("routing", connectionCount, connectionCount + diagnostics.copiesAvoided + 4);
    std::size_t totalUnits = 0;
    for (const auto& [_, cost] : costs) totalUnits += cost.modelUnitsPerSample;
    std::vector<ProcessorFamilyCost> result;
    for (auto& [_, cost] : costs) {
        const auto share = totalUnits == 0 ? 0.0
            : static_cast<double>(cost.modelUnitsPerSample) / static_cast<double>(totalUnits);
        cost.attributedSharePercent = share * 100.0;
        cost.attributedMedianMicroseconds = normal.medianMicroseconds * share;
        cost.attributedPercentile95Microseconds = normal.percentile95Microseconds * share;
        result.push_back(std::move(cost));
    }
    return result;
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

    host.setEnergyTelemetryEnabled(true);
    for (auto warmup = 0; warmup < 8; ++warmup) finite = process() && finite;
    std::vector<double> telemetryTimes;
    telemetryTimes.reserve(request.measuredBlocks);
    for (std::size_t block = 0; block < request.measuredBlocks; ++block) {
        const auto started = Clock::now();
        finite = process() && finite;
        telemetryTimes.push_back(elapsedMicroseconds(started));
    }
    host.setEnergyTelemetryEnabled(false);

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
    result.telemetryEnabled = distribution(telemetryTimes, deadline);
    result.topologyCrossfade = distribution(crossfadeTimes, deadline);
    result.telemetryMedianOverheadRatio = result.normal.medianMicroseconds > 0.0
        ? result.telemetryEnabled.medianMicroseconds / result.normal.medianMicroseconds : 0.0;
    result.crossfadeMedianOverheadRatio = result.normal.medianMicroseconds > 0.0
        ? result.topologyCrossfade.medianMicroseconds / result.normal.medianMicroseconds : 0.0;
    result.graphLatencySamples = normalSnapshot.activeLatency.totalSamples;
    result.preparedMemoryBytes = normalSnapshot.activePlanDiagnostics.preparedStorageBytes;
    result.delayMemoryBytes = normalSnapshot.activeDelayMemoryBytes;
    result.nodeCount = normalSnapshot.activePlanDiagnostics.nodeCount;
    result.connectionCount = normalSnapshot.activePlanDiagnostics.connectionCount;
    result.feedbackRegionCount = normalSnapshot.activePlanDiagnostics.feedbackRegionCount;
    result.blockWiseRegionCount = normalSnapshot.activePlanDiagnostics.blockWiseRegionCount;
    result.sampleWiseRegionCount = normalSnapshot.activePlanDiagnostics.sampleWiseRegionCount;
    result.logicalAudioBufferCount = normalSnapshot.activePlanDiagnostics.logicalAudioBufferCount;
    result.logicalSignalCount = normalSnapshot.activePlanDiagnostics.logicalSignalCount;
    result.elidedNonAudioBufferCount = normalSnapshot.activePlanDiagnostics.elidedNonAudioBufferCount;
    result.physicalAudioBufferCount = normalSnapshot.activePlanDiagnostics.physicalAudioBufferCount;
    result.peakLiveBufferCount = normalSnapshot.activePlanDiagnostics.peakLiveBufferCount;
    result.bufferBytesSaved = normalSnapshot.activePlanDiagnostics.bufferBytesSaved;
    result.inPlaceAliasCount = normalSnapshot.activePlanDiagnostics.inPlaceAliasCount;
    result.copiesAvoided = normalSnapshot.activePlanDiagnostics.copiesAvoided;
    result.fusedKernelCount = normalSnapshot.activePlanDiagnostics.fusedKernelCount;
    result.fusedNodeCount = normalSnapshot.activePlanDiagnostics.fusedNodeCount;
    result.sampleWiseFusedKernelCount = normalSnapshot.activePlanDiagnostics.sampleWiseFusedKernelCount;
    result.simdKernelCount = normalSnapshot.activePlanDiagnostics.simdKernelCount;
    result.estimatedOperationsPerSample = normalSnapshot.activePlanDiagnostics.estimatedScalarOperationsPerSample;
    result.processorFamilies = attributeProcessorFamilies(
        normalSnapshot.activePlanDiagnostics, result.connectionCount, result.normal);
    if (!result.processorFamilies.empty()) {
        result.dominantProcessorFamily = std::ranges::max_element(
            result.processorFamilies, {}, &ProcessorFamilyCost::modelUnitsPerSample)->family;
    }
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
            { "telemetryEnabled", distributionJson(result.telemetryEnabled) },
            { "topologyCrossfade", distributionJson(result.topologyCrossfade) },
            { "telemetryMedianOverheadRatio", result.telemetryMedianOverheadRatio },
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
                { "delayMemoryBytes", result.delayMemoryBytes },
                { "nodeCount", result.nodeCount },
                { "connectionCount", result.connectionCount },
                { "feedbackRegionCount", result.feedbackRegionCount },
                { "blockWiseRegionCount", result.blockWiseRegionCount },
                { "sampleWiseRegionCount", result.sampleWiseRegionCount },
                { "logicalAudioBufferCount", result.logicalAudioBufferCount },
                { "logicalSignalCount", result.logicalSignalCount },
                { "elidedNonAudioBufferCount", result.elidedNonAudioBufferCount },
                { "physicalAudioBufferCount", result.physicalAudioBufferCount },
                { "peakLiveBufferCount", result.peakLiveBufferCount },
                { "bufferBytesSaved", result.bufferBytesSaved },
                { "inPlaceAliasCount", result.inPlaceAliasCount },
                { "copiesAvoided", result.copiesAvoided },
                { "fusedKernelCount", result.fusedKernelCount },
                { "fusedNodeCount", result.fusedNodeCount },
                { "sampleWiseFusedKernelCount", result.sampleWiseFusedKernelCount },
                { "simdKernelCount", result.simdKernelCount },
                { "estimatedOperationsPerSample", result.estimatedOperationsPerSample },
                { "executionDomain", result.executionDomain },
                { "dominantProcessorFamily", result.dominantProcessorFamily },
                { "processorFamilies", [&result] {
                    auto families = nlohmann::ordered_json::array();
                    for (const auto& family : result.processorFamilies) families.push_back({
                        { "family", family.family }, { "nodeCount", family.nodeCount },
                        { "modelUnitsPerSample", family.modelUnitsPerSample },
                        { "attributedMedianMicroseconds", family.attributedMedianMicroseconds },
                        { "attributedPercentile95Microseconds", family.attributedPercentile95Microseconds },
                        { "attributedSharePercent", family.attributedSharePercent },
                    });
                    return families;
                }() },
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
        { "familyAttribution", "Measured normal callback time apportioned by prepared-plan scalar work plus routing units; telemetry and crossfade are independently timed, and attributed family values are not independent stopwatch measurements" },
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

BarrExecutionComparison measureBarrExecutionComparison(
    const double sampleRate, const std::size_t blockSize, const std::size_t measuredBlocks)
{
    if (sampleRate <= 0.0 || blockSize == 0 || measuredBlocks < 20)
        throw std::invalid_argument("Barr comparison requires a positive rate/block and at least 20 blocks");
    std::vector<float> inputLeft(blockSize), inputRight(blockSize), directLeft(blockSize),
        directRight(blockSize), genericLeft(blockSize), genericRight(blockSize);
    std::uint32_t noise = 0x4d595df4U;
    for (std::size_t index = 0; index < blockSize; ++index) {
        noise = noise * 1664525U + 1013904223U;
        inputLeft[index] = static_cast<float>((static_cast<double>(noise)
            / static_cast<double>(std::numeric_limits<std::uint32_t>::max()) - 0.5) * 0.1);
        inputRight[index] = static_cast<float>(0.05 * std::sin(
            2.0 * 3.14159265358979323846 * 311.0 * static_cast<double>(index) / sampleRate));
    }
    reverb::dsp::BarrReference direct;
    direct.prepare(sampleRate);
    reverb::graph::AcyclicRuntimeHost generic;
    const auto publication = generic.compileFeedbackAndPublish(
        reverb::graph::makeBarrReferenceGraph(), sampleRate, blockSize);
    if (!publication.valid()) throw std::runtime_error("Barr generic comparison failed to compile");
    const auto runDirect = [&] { direct.process(inputLeft, inputRight, directLeft, directRight); };
    const auto runGeneric = [&] { generic.process(inputLeft, inputRight, genericLeft, genericRight); };
    runGeneric();
    runDirect();
    for (auto warmup = 0; warmup < 32; ++warmup) { runDirect(); runGeneric(); }
    auto equivalent = directLeft == genericLeft && directRight == genericRight;
    std::vector<double> directTimes, genericTimes;
    directTimes.reserve(measuredBlocks);
    genericTimes.reserve(measuredBlocks);
    for (std::size_t block = 0; block < measuredBlocks; ++block) {
        auto started = Clock::now();
        runDirect();
        directTimes.push_back(elapsedMicroseconds(started));
        started = Clock::now();
        runGeneric();
        genericTimes.push_back(elapsedMicroseconds(started));
        equivalent = equivalent && directLeft == genericLeft && directRight == genericRight;
    }
    const auto deadline = static_cast<double>(blockSize) * 1'000'000.0 / sampleRate;
    BarrExecutionComparison result;
    result.sampleRate = sampleRate;
    result.blockSize = blockSize;
    result.measuredBlocks = measuredBlocks;
    result.directReference = distribution(directTimes, deadline);
    result.optimizedGeneric = distribution(genericTimes, deadline);
    result.genericToDirectP95Ratio = result.directReference.percentile95Microseconds > 0.0
        ? result.optimizedGeneric.percentile95Microseconds / result.directReference.percentile95Microseconds : 0.0;
    result.sampleEquivalent = equivalent;
    result.finiteOutput = std::ranges::all_of(genericLeft, [](float value) { return std::isfinite(value); })
        && std::ranges::all_of(genericRight, [](float value) { return std::isfinite(value); });
    return result;
}

std::string barrExecutionComparisonJson(
    const std::vector<BarrExecutionComparison>& results,
    std::string machineLabel,
    std::string toolchain,
    std::string buildCommit)
{
    auto cases = nlohmann::ordered_json::array();
    for (const auto& result : results) cases.push_back({
        { "sampleRate", result.sampleRate }, { "blockSize", result.blockSize },
        { "measuredBlocks", result.measuredBlocks },
        { "directReference", distributionJson(result.directReference) },
        { "optimizedGeneric", distributionJson(result.optimizedGeneric) },
        { "genericToDirectP95Ratio", result.genericToDirectP95Ratio },
        { "sampleEquivalent", result.sampleEquivalent }, { "finiteOutput", result.finiteOutput },
    });
    return nlohmann::ordered_json {
        { "formatVersion", 1 }, { "measurement", "barr-direct-versus-optimized-generic" },
        { "scope", "same-machine paired callback timing; no additional specialized prototype" },
        { "machine", std::move(machineLabel) }, { "toolchain", std::move(toolchain) },
        { "buildConfiguration", "Release" }, { "buildCommit", std::move(buildCommit) },
        { "source", "identical deterministic LCG noise plus 311 Hz sine" },
        { "cases", std::move(cases) },
    }.dump(2) + "\n";
}

} // namespace reverb::render
