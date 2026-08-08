#pragma once

#include <reverb/graph/GraphDocument.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace reverb::graph {

struct AcyclicCompileResult;

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

    [[nodiscard]] const std::vector<std::string>& schedule() const noexcept;
    [[nodiscard]] std::size_t maximumBlockSize() const noexcept;
    [[nodiscard]] std::size_t preparedStorageBytes() const noexcept;

private:
    struct Impl;
    explicit PreparedAcyclicRuntime(std::unique_ptr<Impl> implementation) noexcept;
    std::unique_ptr<Impl> implementation_;
    friend struct AcyclicCompileResult;
    friend AcyclicCompileResult compileAcyclicGraph(const GraphDocument&, double, std::size_t);
};

struct AcyclicCompileResult final {
    std::unique_ptr<PreparedAcyclicRuntime> runtime;
    std::vector<std::string> schedule;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    [[nodiscard]] bool valid() const noexcept { return runtime != nullptr && errors.empty(); }
};

struct AcyclicPublishResult final {
    std::vector<std::string> schedule;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    [[nodiscard]] bool valid() const noexcept { return errors.empty(); }
};

[[nodiscard]] AcyclicCompileResult compileAcyclicGraph(
    const GraphDocument& document,
    double sampleRate,
    std::size_t maximumBlockSize);

class AcyclicRuntimeHost final {
public:
    [[nodiscard]] AcyclicPublishResult compileAndPublish(
        const GraphDocument& document,
        double sampleRate,
        std::size_t maximumBlockSize);

    void process(
        std::span<const float> inputLeft,
        std::span<const float> inputRight,
        std::span<float> outputLeft,
        std::span<float> outputRight) noexcept;

    [[nodiscard]] bool hasRuntime() const noexcept;

private:
    std::mutex publicationMutex_;
    std::unique_ptr<PreparedAcyclicRuntime> ownedRuntime_;
    std::atomic<PreparedAcyclicRuntime*> activeRuntime_ {};
    std::atomic_bool processing_ {};
};

} // namespace reverb::graph
