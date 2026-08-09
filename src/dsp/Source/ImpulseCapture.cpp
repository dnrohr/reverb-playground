#include <reverb/dsp/ImpulseCapture.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace reverb::dsp {

static_assert(std::atomic<double>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<int>::is_always_lock_free);
static_assert(std::atomic<std::size_t>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

void ImpulseCapture::prepare(const double sampleRate)
{
    const auto boundedRate = std::max(0.0, sampleRate);
    sampleRate_.store(boundedRate, std::memory_order_release);
    const auto capacity = static_cast<std::size_t>(std::ceil(
        std::min(boundedRate, maximumSupportedSampleRate) * maximumLengthMilliseconds / 1'000.0));
    for (auto& slot : slots_) {
        slot.left.assign(capacity, 0.0F);
        slot.right.assign(capacity, 0.0F);
        slot.frames = 0;
    }
    state_.store(ImpulseCaptureState::idle, std::memory_order_release);
    visibleFrames_.store(0, std::memory_order_release);
    publishedSlot_.store(-1, std::memory_order_release);
}

ImpulseCaptureConfig ImpulseCapture::request(ImpulseCaptureConfig config) noexcept
{
    config.maximumLengthMilliseconds = std::clamp(
        config.maximumLengthMilliseconds, minimumLengthMilliseconds, maximumLengthMilliseconds);
    config.stopThresholdDb = std::clamp(
        config.stopThresholdDb, minimumStopThresholdDb, maximumStopThresholdDb);
    config.impulseLevel = std::clamp(config.impulseLevel, 0.001F, maximumImpulseLevel);
    requestedLengthMs_.store(config.maximumLengthMilliseconds, std::memory_order_relaxed);
    requestedThresholdDb_.store(config.stopThresholdDb, std::memory_order_relaxed);
    requestedThresholdLinear_.store(
        std::pow(10.0F, static_cast<float>(config.stopThresholdDb / 20.0)),
        std::memory_order_relaxed);
    requestedMuteLiveInput_.store(config.muteLiveInput, std::memory_order_relaxed);
    requestedImpulseLevel_.store(config.impulseLevel, std::memory_order_relaxed);
    requestedGeneration_.fetch_add(1, std::memory_order_release);
    visibleFrames_.store(0, std::memory_order_release);
    state_.store(ImpulseCaptureState::armed, std::memory_order_release);
    return config;
}

bool ImpulseCapture::beginIfRequested() noexcept
{
    const auto requested = requestedGeneration_.load(std::memory_order_acquire);
    if (requested == activeGeneration_.load(std::memory_order_relaxed))
        return false;
    activeGeneration_.store(requested, std::memory_order_relaxed);
    activeConfig_ = {
        requestedLengthMs_.load(std::memory_order_relaxed),
        requestedThresholdDb_.load(std::memory_order_relaxed),
        requestedMuteLiveInput_.load(std::memory_order_relaxed),
        requestedImpulseLevel_.load(std::memory_order_relaxed),
    };
    const auto rate = sampleRate_.load(std::memory_order_relaxed);
    maximumFrames_ = std::min(slots_.front().left.size(), static_cast<std::size_t>(
        std::ceil(rate * activeConfig_.maximumLengthMilliseconds / 1'000.0)));
    quietFramesRequired_ = static_cast<std::size_t>(std::ceil(rate * 0.1));
    thresholdLinear_ = requestedThresholdLinear_.load(std::memory_order_relaxed);
    quietFrames_ = 0;
    signalSeen_ = false;
    const auto published = publishedSlot_.load(std::memory_order_acquire);
    const auto reader = readerSlot_.load(std::memory_order_acquire);
    for (int candidate = 0; candidate < static_cast<int>(slots_.size()); ++candidate) {
        if (candidate != published && candidate != reader) {
            writerSlot_ = candidate;
            break;
        }
    }
    auto& slot = slots_[static_cast<std::size_t>(writerSlot_)];
    slot.generation = requested;
    slot.sampleRate = rate;
    slot.config = activeConfig_;
    slot.frames = 0;
    slot.stoppedAtThreshold = false;
    visibleFrames_.store(0, std::memory_order_release);
    state_.store(ImpulseCaptureState::capturing, std::memory_order_release);
    if (maximumFrames_ == 0)
        finish(false);
    return true;
}

void ImpulseCapture::append(const std::span<const float> left, const std::span<const float> right) noexcept
{
    if (state_.load(std::memory_order_relaxed) != ImpulseCaptureState::capturing)
        return;
    auto& slot = slots_[static_cast<std::size_t>(writerSlot_)];
    const auto available = maximumFrames_ - slot.frames;
    const auto count = std::min({ left.size(), right.size(), available });
    for (std::size_t index = 0; index < count; ++index) {
        const auto frame = slot.frames + index;
        slot.left[frame] = left[index];
        slot.right[frame] = right[index];
        const auto magnitude = std::max(std::abs(left[index]), std::abs(right[index]));
        if (magnitude > thresholdLinear_) {
            signalSeen_ = true;
            quietFrames_ = 0;
        } else if (signalSeen_) {
            ++quietFrames_;
        }
    }
    slot.frames += count;
    visibleFrames_.store(slot.frames, std::memory_order_release);
    const auto minimumBeforeThreshold = quietFramesRequired_;
    if (signalSeen_ && slot.frames >= minimumBeforeThreshold && quietFrames_ >= quietFramesRequired_)
        finish(true);
    else if (slot.frames >= maximumFrames_)
        finish(false);
}

void ImpulseCapture::finish(const bool stoppedAtThreshold) noexcept
{
    auto& slot = slots_[static_cast<std::size_t>(writerSlot_)];
    slot.stoppedAtThreshold = stoppedAtThreshold;
    publishedGeneration_.store(slot.generation, std::memory_order_relaxed);
    publishedSlot_.store(writerSlot_, std::memory_order_release);
    state_.store(ImpulseCaptureState::complete, std::memory_order_release);
}

ImpulseCaptureState ImpulseCapture::state() const noexcept { return state_.load(std::memory_order_acquire); }
std::uint64_t ImpulseCapture::generation() const noexcept { return publishedGeneration_.load(std::memory_order_acquire); }
std::size_t ImpulseCapture::capturedFrames() const noexcept { return visibleFrames_.load(std::memory_order_acquire); }
ImpulseCaptureConfig ImpulseCapture::activeConfig() const noexcept { return activeConfig_; }

ImpulseCaptureResult ImpulseCapture::copyLatest() const
{
    const auto index = publishedSlot_.load(std::memory_order_acquire);
    if (index < 0)
        throw std::logic_error("no impulse capture is available");
    readerSlot_.store(index, std::memory_order_release);
    try {
        const auto& slot = slots_[static_cast<std::size_t>(index)];
        ImpulseCaptureResult result;
        result.generation = slot.generation;
        result.sampleRate = slot.sampleRate;
        result.config = slot.config;
        result.stoppedAtThreshold = slot.stoppedAtThreshold;
        result.left.assign(slot.left.begin(), slot.left.begin() + static_cast<std::ptrdiff_t>(slot.frames));
        result.right.assign(slot.right.begin(), slot.right.begin() + static_cast<std::ptrdiff_t>(slot.frames));
        readerSlot_.store(-1, std::memory_order_release);
        return result;
    } catch (...) {
        readerSlot_.store(-1, std::memory_order_release);
        throw;
    }
}

} // namespace reverb::dsp
