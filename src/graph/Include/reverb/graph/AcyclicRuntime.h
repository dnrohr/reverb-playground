#pragma once

#include <reverb/graph/GraphDocument.h>
#include <reverb/graph/ControlRate.h>

#include <atomic>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <map>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace reverb::graph {

struct AcyclicCompileResult;

inline constexpr std::size_t delayMemoryBudgetBytes = 64U * 1024U * 1024U;

struct DelayMemoryPlan final {
    std::size_t lineCount {};
    std::size_t requestedSamples {};
    std::size_t allocatedSamples {};
    std::size_t requestedBytes {};
    std::size_t allocatedBytes {};
    std::size_t budgetBytes { delayMemoryBudgetBytes };

    [[nodiscard]] bool withinBudget() const noexcept { return allocatedBytes <= budgetBytes; }
};

struct LatencyPath final {
    std::string outputPort;
    std::size_t samples {};
    std::vector<std::string> nodeIds;
};

struct LatencyJoin final {
    std::string nodeId;
    std::size_t minimumInputSamples {};
    std::size_t maximumInputSamples {};
    [[nodiscard]] std::size_t uncompensatedSamples() const noexcept
    {
        return maximumInputSamples - minimumInputSamples;
    }
};

struct GraphLatencyPlan final {
    std::size_t totalSamples {};
    std::vector<LatencyPath> outputPaths;
    std::vector<LatencyJoin> parallelJoins;
};

struct WorkloadFamily final {
    std::string family;
    std::size_t nodeCount {};
    std::size_t estimatedScalarOperationsPerSample {};
};

struct BufferRetentionReason final {
    std::string reason;
    std::size_t signalCount {};
};

struct CompilePhaseTiming final {
    std::uint64_t validationMicroseconds {};
    std::uint64_t schedulingMicroseconds {};
    std::uint64_t preparationMicroseconds {};
    std::uint64_t totalMicroseconds {};
};

struct PreparedGraphDiagnostics final {
    std::size_t nodeCount {};
    std::size_t connectionCount {};
    std::size_t feedbackRegionCount {};
    std::size_t blockWiseRegionCount {};
    std::size_t sampleWiseRegionCount {};
    std::size_t preparedStorageBytes {};
    std::size_t estimatedScalarOperationsPerSample {};
    std::size_t logicalAudioBufferCount {};
    std::size_t logicalSignalCount {};
    std::size_t elidedNonAudioBufferCount {};
    std::size_t physicalAudioBufferCount {};
    std::size_t peakLiveBufferCount {};
    std::size_t bufferBytesSaved {};
    std::size_t inPlaceAliasCount {};
    std::size_t copiesAvoided {};
    std::size_t fusedKernelCount {};
    std::size_t fusedNodeCount {};
    std::size_t sampleWiseFusedKernelCount {};
    std::size_t simdKernelCount {};
    std::string executionDomain;
    std::vector<WorkloadFamily> workloadFamilies;
    std::vector<BufferRetentionReason> bufferRetentionReasons;
    std::vector<BufferRetentionReason> fusionPreventionReasons;
    CompilePhaseTiming compileTiming;
};

struct GraphEnergyNode final { std::string nodeId; float rms {}; };
struct GraphEnergySnapshot final {
    bool enabled {};
    bool coherent { true };
    std::uint64_t revision {};
    std::uint64_t generation {};
    std::uint64_t observedSampleValues {};
    std::vector<GraphEnergyNode> nodes;
};

class PreparedAcyclicRuntime final {
public:
    ~PreparedAcyclicRuntime();
    PreparedAcyclicRuntime(PreparedAcyclicRuntime&&) noexcept;
    PreparedAcyclicRuntime& operator=(PreparedAcyclicRuntime&&) noexcept;
    PreparedAcyclicRuntime(const PreparedAcyclicRuntime&) = delete;
    PreparedAcyclicRuntime& operator=(const PreparedAcyclicRuntime&) = delete;

    void process(
        std::span<const float> inputLeft,
        std::span<const float> inputRight,
        std::span<float> outputLeft,
        std::span<float> outputRight,
        bool observeEnergy = false) noexcept;
    void reset() noexcept;
    [[nodiscard]] bool setMacroValue(std::string_view nodeId, double value) noexcept;
    void applyMacroValue(std::size_t slot, std::uint64_t key, double value) noexcept;

    [[nodiscard]] const std::vector<std::string>& schedule() const noexcept;
    [[nodiscard]] std::size_t maximumBlockSize() const noexcept;
    [[nodiscard]] std::size_t preparedStorageBytes() const noexcept;
    [[nodiscard]] const DelayMemoryPlan& delayMemoryPlan() const noexcept;
    [[nodiscard]] const GraphLatencyPlan& latencyPlan() const noexcept;
    [[nodiscard]] const PreparedGraphDiagnostics& planDiagnostics() const noexcept;
    [[nodiscard]] const std::vector<std::string>& energyNodeIds() const noexcept;
    [[nodiscard]] const std::vector<float>& blockEnergyRms() const noexcept;
    [[nodiscard]] std::uint64_t blockEnergyObservedValues() const noexcept;

private:
    struct Impl;
    explicit PreparedAcyclicRuntime(std::unique_ptr<Impl> implementation) noexcept;
    std::unique_ptr<Impl> implementation_;
    friend struct AcyclicCompileResult;
    friend AcyclicCompileResult compileAcyclicGraph(
        const GraphDocument&, double, std::size_t, bool, bool);
};

struct AcyclicCompileResult final {
    std::unique_ptr<PreparedAcyclicRuntime> runtime;
    std::vector<std::string> schedule;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    std::vector<std::vector<std::string>> feedbackComponents;
    std::vector<std::vector<std::string>> offendingLoops;
    std::uint64_t compileMicroseconds {};
    DelayMemoryPlan delayMemory;
    GraphLatencyPlan latency;
    PreparedGraphDiagnostics planDiagnostics;

    [[nodiscard]] bool valid() const noexcept { return runtime != nullptr && errors.empty(); }
};

struct AcyclicPublishResult final {
    std::vector<std::string> schedule;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    DelayMemoryPlan delayMemory;
    GraphLatencyPlan latency;
    PreparedGraphDiagnostics planDiagnostics;
    [[nodiscard]] bool valid() const noexcept { return errors.empty(); }
};

struct TopologyPublicationSnapshot final {
    std::uint64_t requestedRevision {};
    std::uint64_t pendingRevision {};
    std::uint64_t activeRevision {};
    std::uint64_t failedRevision {};
    std::uint64_t supersededRequests {};
    std::uint64_t completedCompilations {};
    std::uint64_t reclaimedRuntimes {};
    std::uint64_t crossfadeFromRevision {};
    std::size_t crossfadePositionSamples {};
    std::size_t crossfadeTotalSamples {};
    std::uint64_t completedCrossfades {};
    std::uint64_t lastCrossfadeFromRevision {};
    std::uint64_t lastCrossfadeToRevision {};
    std::size_t activeDelayLineCount {};
    std::size_t activeDelayMemoryBytes {};
    GraphLatencyPlan activeLatency;
    PreparedGraphDiagnostics activePlanDiagnostics;
    std::uint64_t activeRequestToActiveMicroseconds {};
    std::uint64_t supersededCompilations {};
    std::uint64_t lastSupersededCompileMicroseconds {};
    std::string failure;
};

[[nodiscard]] AcyclicCompileResult compileAcyclicGraph(
    const GraphDocument& document,
    double sampleRate,
    std::size_t maximumBlockSize,
    bool allowFeedback = false,
    bool enableSampleWiseFusion = true);

[[nodiscard]] AcyclicCompileResult compileFeedbackGraph(
    const GraphDocument& document,
    double sampleRate,
    std::size_t maximumBlockSize,
    bool enableSampleWiseFusion = true);

class AcyclicRuntimeHost final {
public:
    static constexpr double topologyCrossfadeMilliseconds = 10.0;
    AcyclicRuntimeHost();
    ~AcyclicRuntimeHost();
    AcyclicRuntimeHost(const AcyclicRuntimeHost&) = delete;
    AcyclicRuntimeHost& operator=(const AcyclicRuntimeHost&) = delete;

    [[nodiscard]] AcyclicPublishResult compileAndPublish(
        const GraphDocument& document,
        double sampleRate,
        std::size_t maximumBlockSize);
    [[nodiscard]] AcyclicPublishResult compileFeedbackAndPublish(
        const GraphDocument& document,
        double sampleRate,
        std::size_t maximumBlockSize);
    [[nodiscard]] std::uint64_t requestCompilation(
        GraphDocument document,
        double sampleRate,
        std::size_t maximumBlockSize,
        bool allowFeedback = true);
    [[nodiscard]] TopologyPublicationSnapshot publicationSnapshot() const;

    void process(
        std::span<const float> inputLeft,
        std::span<const float> inputRight,
        std::span<float> outputLeft,
        std::span<float> outputRight) noexcept;
    void resetActiveRuntimes() noexcept;

    [[nodiscard]] bool hasRuntime() const noexcept;
    [[nodiscard]] std::uint64_t activeRevision() const noexcept;
    [[nodiscard]] std::size_t activeLatencySamples() const noexcept;
    [[nodiscard]] bool setMacroValue(std::string_view nodeId, double value) noexcept;
    void setEnergyTelemetryEnabled(bool enabled) noexcept;
    [[nodiscard]] GraphEnergySnapshot energySnapshot() const;

private:
    struct RuntimeEnvelope;
    struct CompilationRequest;
    static constexpr std::size_t retirementCapacity = 16;

    [[nodiscard]] AcyclicPublishResult publishCompiled(
        AcyclicCompileResult result, std::uint64_t revision, double sampleRate,
        std::uint64_t requestedAtNanoseconds);
    void compilerLoop(std::stop_token stopToken);
    void publishPending(
        std::unique_ptr<PreparedAcyclicRuntime> runtime, std::uint64_t revision, double sampleRate,
        std::uint64_t requestedAtNanoseconds);
    void reclaimRetired() noexcept;
    [[nodiscard]] bool retirementHasCapacity() const noexcept;
    void retire(RuntimeEnvelope* runtime) noexcept;

    mutable std::mutex requestMutex_;
    std::condition_variable_any requestCondition_;
    std::unique_ptr<CompilationRequest> latestRequest_;
    std::jthread compilerThread_;
    std::atomic<RuntimeEnvelope*> activeRuntime_ {};
    std::atomic<RuntimeEnvelope*> pendingRuntime_ {};
    RuntimeEnvelope* fadingRuntime_ {};
    std::size_t crossfadePosition_ {};
    std::array<RuntimeEnvelope*, retirementCapacity> retired_ {};
    std::atomic<std::size_t> retirementWrite_ {};
    std::atomic<std::size_t> retirementRead_ {};
    std::atomic<std::uint64_t> requestedRevision_ {};
    std::atomic<std::uint64_t> pendingRevision_ {};
    std::atomic<std::uint64_t> activeRevision_ {};
    std::atomic<std::uint64_t> failedRevision_ {};
    std::atomic<std::uint64_t> supersededRequests_ {};
    std::atomic<std::uint64_t> completedCompilations_ {};
    std::atomic<std::uint64_t> reclaimedRuntimes_ {};
    std::atomic<std::uint64_t> crossfadeFromRevision_ {};
    std::atomic<std::size_t> crossfadePositionSamples_ {};
    std::atomic<std::size_t> crossfadeTotalSamples_ {};
    std::atomic<std::uint64_t> completedCrossfades_ {};
    std::atomic<std::uint64_t> lastCrossfadeFromRevision_ {};
    std::atomic<std::uint64_t> lastCrossfadeToRevision_ {};
    std::atomic<std::size_t> activeDelayLineCount_ {};
    std::atomic<std::size_t> activeDelayMemoryBytes_ {};
    std::atomic<std::size_t> activeLatencySamples_ {};
    std::atomic<std::uint64_t> activeRequestToActiveMicroseconds_ {};
    std::atomic<std::uint64_t> supersededCompilations_ {};
    std::atomic<std::uint64_t> lastSupersededCompileMicroseconds_ {};
    std::array<std::atomic<std::uint64_t>, maximumMacroControls> macroKeys_ {};
    std::array<std::atomic<double>, maximumMacroControls> macroValues_ {};
    mutable std::mutex failureMutex_;
    mutable std::mutex latencyPlansMutex_;
    std::map<std::uint64_t, GraphLatencyPlan> latencyPlans_;
    std::map<std::uint64_t, PreparedGraphDiagnostics> planDiagnostics_;
    std::map<std::uint64_t, std::vector<std::string>> energyNodeIds_;
    static constexpr std::size_t maximumEnergyNodes = 256;
    std::atomic<bool> energyEnabled_ {};
    std::array<std::atomic<float>, maximumEnergyNodes> energyRms_ {};
    std::atomic<std::size_t> energyNodeCount_ {};
    std::atomic<std::uint64_t> energyRevision_ {};
    std::atomic<std::uint64_t> energyGeneration_ {};
    std::atomic<std::uint64_t> energyObservedValues_ {};
    std::atomic<std::uint64_t> energySequence_ {};
    std::mutex pendingPublicationMutex_;
    std::string failure_;
};

} // namespace reverb::graph
