#pragma once

#include <reverb/graph/GraphDocument.h>

#include <atomic>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
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
        std::span<float> outputRight) noexcept;
    void reset() noexcept;

    [[nodiscard]] const std::vector<std::string>& schedule() const noexcept;
    [[nodiscard]] std::size_t maximumBlockSize() const noexcept;
    [[nodiscard]] std::size_t preparedStorageBytes() const noexcept;
    [[nodiscard]] const DelayMemoryPlan& delayMemoryPlan() const noexcept;

private:
    struct Impl;
    explicit PreparedAcyclicRuntime(std::unique_ptr<Impl> implementation) noexcept;
    std::unique_ptr<Impl> implementation_;
    friend struct AcyclicCompileResult;
    friend AcyclicCompileResult compileAcyclicGraph(const GraphDocument&, double, std::size_t, bool);
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

    [[nodiscard]] bool valid() const noexcept { return runtime != nullptr && errors.empty(); }
};

struct AcyclicPublishResult final {
    std::vector<std::string> schedule;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    DelayMemoryPlan delayMemory;
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
    std::string failure;
};

[[nodiscard]] AcyclicCompileResult compileAcyclicGraph(
    const GraphDocument& document,
    double sampleRate,
    std::size_t maximumBlockSize,
    bool allowFeedback = false);

[[nodiscard]] AcyclicCompileResult compileFeedbackGraph(
    const GraphDocument& document,
    double sampleRate,
    std::size_t maximumBlockSize);

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

private:
    struct RuntimeEnvelope;
    struct CompilationRequest;
    static constexpr std::size_t retirementCapacity = 16;

    [[nodiscard]] AcyclicPublishResult publishCompiled(
        AcyclicCompileResult result, std::uint64_t revision, double sampleRate);
    void compilerLoop(std::stop_token stopToken);
    void publishPending(
        std::unique_ptr<PreparedAcyclicRuntime> runtime, std::uint64_t revision, double sampleRate);
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
    mutable std::mutex failureMutex_;
    std::mutex pendingPublicationMutex_;
    std::string failure_;
};

} // namespace reverb::graph
