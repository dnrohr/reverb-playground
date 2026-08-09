#include <reverb/dsp/RuntimeDiagnostics.h>

#include <algorithm>
#include <chrono>

namespace reverb::dsp {
namespace {

std::uint64_t monotonicNanoseconds() noexcept
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

} // namespace

static_assert(std::atomic<double>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<std::size_t>::is_always_lock_free);

void RuntimeDiagnostics::prepare(
    const double sampleRate,
    const std::size_t delayLineCount,
    const std::size_t delayMemoryBytes) noexcept
{
    sampleRate_.store(std::max(0.0, sampleRate), std::memory_order_release);
    delayLineCount_.store(delayLineCount, std::memory_order_release);
    delayMemoryBytes_.store(delayMemoryBytes, std::memory_order_release);
    processedBlocks_.store(0, std::memory_order_relaxed);
    liveLoadPercent_.store(0.0F, std::memory_order_relaxed);
    peakLoadPercent_.store(0.0F, std::memory_order_relaxed);
    clippedSamples_.store(0, std::memory_order_relaxed);
    clippedBlocks_.store(0, std::memory_order_relaxed);
    smoothedLoadPercent_ = 0.0F;
    peakLoadPercentAudio_ = 0.0F;
}

std::uint64_t RuntimeDiagnostics::beginBlock() const noexcept
{
    return monotonicNanoseconds();
}

void RuntimeDiagnostics::endBlock(
    const std::uint64_t startedNanoseconds,
    const std::size_t frameCount,
    const std::size_t clippedSamples) noexcept
{
    const auto elapsed = monotonicNanoseconds() - startedNanoseconds;
    const auto rate = sampleRate_.load(std::memory_order_relaxed);
    const auto budget = rate > 0.0
        ? 1.0e9 * static_cast<double>(frameCount) / rate
        : 0.0;
    const auto load = budget > 0.0
        ? static_cast<float>(100.0 * static_cast<double>(elapsed) / budget)
        : 0.0F;
    smoothedLoadPercent_ += 0.15F * (load - smoothedLoadPercent_);
    peakLoadPercentAudio_ = std::max(peakLoadPercentAudio_, load);
    liveLoadPercent_.store(smoothedLoadPercent_, std::memory_order_relaxed);
    peakLoadPercent_.store(peakLoadPercentAudio_, std::memory_order_relaxed);
    processedBlocks_.fetch_add(1, std::memory_order_relaxed);
    if (clippedSamples > 0) {
        clippedSamples_.fetch_add(clippedSamples, std::memory_order_relaxed);
        clippedBlocks_.fetch_add(1, std::memory_order_relaxed);
    }
}

std::uint64_t RuntimeDiagnostics::advanceRevision() noexcept
{
    return activeRevision_.fetch_add(1, std::memory_order_acq_rel) + 1;
}

std::uint64_t RuntimeDiagnostics::activeRevision() const noexcept
{
    return activeRevision_.load(std::memory_order_acquire);
}

void RuntimeDiagnostics::recordSafety(const SafetyStatus status, const SafetyChannel channel) noexcept
{
    if (status.violation == SafetyViolation::none)
        return;
    safetySequence_.fetch_add(1, std::memory_order_acq_rel);
    lastViolation_.store(status.violation, std::memory_order_relaxed);
    lastViolationChannel_.store(channel, std::memory_order_relaxed);
    lastViolationSampleIndex_.store(status.sampleIndex, std::memory_order_relaxed);
    lastViolationRevision_.store(activeRevision(), std::memory_order_relaxed);
    safetyEventGeneration_.fetch_add(1, std::memory_order_relaxed);
    safetySequence_.fetch_add(1, std::memory_order_release);
}

void RuntimeDiagnostics::recordRecovery() noexcept
{
    recoveryCount_.fetch_add(1, std::memory_order_relaxed);
}

RuntimeDiagnosticsSnapshot RuntimeDiagnostics::snapshot() const noexcept
{
    RuntimeDiagnosticsSnapshot result;
    result.sampleRate = sampleRate_.load(std::memory_order_acquire);
    result.processedBlocks = processedBlocks_.load(std::memory_order_acquire);
    result.liveLoadPercent = liveLoadPercent_.load(std::memory_order_acquire);
    result.peakLoadPercent = peakLoadPercent_.load(std::memory_order_acquire);
    result.clippedSamples = clippedSamples_.load(std::memory_order_acquire);
    result.clippedBlocks = clippedBlocks_.load(std::memory_order_acquire);
    result.delayLineCount = delayLineCount_.load(std::memory_order_acquire);
    result.delayMemoryBytes = delayMemoryBytes_.load(std::memory_order_acquire);
    result.activeRevision = activeRevision();
    result.recoveryCount = recoveryCount_.load(std::memory_order_acquire);
    for (int attempt = 0; attempt < 8; ++attempt) {
        const auto before = safetySequence_.load(std::memory_order_acquire);
        if ((before & 1U) != 0U)
            continue;
        result.safetyEventGeneration = safetyEventGeneration_.load(std::memory_order_relaxed);
        result.lastViolation = lastViolation_.load(std::memory_order_relaxed);
        result.lastViolationChannel = lastViolationChannel_.load(std::memory_order_relaxed);
        result.lastViolationSampleIndex = lastViolationSampleIndex_.load(std::memory_order_relaxed);
        result.lastViolationRevision = lastViolationRevision_.load(std::memory_order_relaxed);
        if (before == safetySequence_.load(std::memory_order_acquire))
            return result;
    }
    result.safetyEventCoherent = false;
    result.lastViolation = SafetyViolation::none;
    return result;
}

} // namespace reverb::dsp
