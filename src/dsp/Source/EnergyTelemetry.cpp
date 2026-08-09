#include <reverb/dsp/EnergyTelemetry.h>

#include <algorithm>
#include <cmath>

namespace reverb::dsp {
namespace {

constexpr std::array<std::string_view, barrEnergyLaneCount> nodeIds {
    "input", "sum", "input-filter", "diffuser-1", "diffuser-2",
    "tank-1", "tank-2", "left-tap", "right-tap", "output",
};

float monoRms(const std::span<const float> samples) noexcept
{
    if (samples.empty())
        return 0.0F;
    double energy = 0.0;
    for (const auto sample : samples)
        energy += static_cast<double>(sample) * static_cast<double>(sample);
    return static_cast<float>(std::sqrt(energy / static_cast<double>(samples.size())));
}

} // namespace

static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);

std::string_view barrEnergyLaneNodeId(const BarrEnergyLane lane) noexcept
{
    const auto index = static_cast<std::size_t>(lane);
    return index < nodeIds.size() ? nodeIds[index] : std::string_view {};
}

EnergyTelemetry::EnergyTelemetry() noexcept
{
    for (auto& value : publishedRms_)
        value.store(0.0F, std::memory_order_relaxed);
}

void EnergyTelemetry::prepare(const double sampleRate) noexcept
{
    publishIntervalFrames_ = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::llround(std::max(1.0, sampleRate) / 30.0)));
    framesUntilPublish_ = publishIntervalFrames_;
    observedSampleValues_ = 0;
    blockRms_.fill(0.0F);
    pendingRms_.fill(0.0F);
    sequence_.store(0, std::memory_order_relaxed);
    generation_.store(0, std::memory_order_relaxed);
    publishedObservedSampleValues_.store(0, std::memory_order_relaxed);
    for (auto& value : publishedRms_)
        value.store(0.0F, std::memory_order_relaxed);
}

void EnergyTelemetry::setEnabled(const bool enabled) noexcept
{
    enabled_.store(enabled, std::memory_order_release);
}

bool EnergyTelemetry::isEnabled() const noexcept
{
    return enabled_.load(std::memory_order_acquire);
}

bool EnergyTelemetry::beginBlock() noexcept
{
    if (!enabled_.load(std::memory_order_relaxed))
        return false;
    blockRms_.fill(0.0F);
    return true;
}

void EnergyTelemetry::observeMono(
    const BarrEnergyLane lane, const std::span<const float> samples) noexcept
{
    const auto index = static_cast<std::size_t>(lane);
    if (index >= blockRms_.size())
        return;
    blockRms_[index] = monoRms(samples);
    observedSampleValues_ += samples.size();
}

void EnergyTelemetry::observeStereo(
    const BarrEnergyLane lane,
    const std::span<const float> left,
    const std::span<const float> right) noexcept
{
    const auto index = static_cast<std::size_t>(lane);
    const auto count = std::min(left.size(), right.size());
    if (index >= blockRms_.size() || count == 0)
        return;
    double energy = 0.0;
    for (std::size_t frame = 0; frame < count; ++frame) {
        energy += static_cast<double>(left[frame]) * static_cast<double>(left[frame]);
        energy += static_cast<double>(right[frame]) * static_cast<double>(right[frame]);
    }
    blockRms_[index] = static_cast<float>(std::sqrt(energy / static_cast<double>(count * 2)));
    observedSampleValues_ += count * 2;
}

void EnergyTelemetry::endBlock(const std::size_t frameCount) noexcept
{
    for (std::size_t index = 0; index < pendingRms_.size(); ++index)
        pendingRms_[index] = std::max(pendingRms_[index], blockRms_[index]);

    if (frameCount < framesUntilPublish_) {
        framesUntilPublish_ -= frameCount;
        return;
    }
    publish();
    pendingRms_.fill(0.0F);
    framesUntilPublish_ = publishIntervalFrames_;
}

void EnergyTelemetry::publish() noexcept
{
    sequence_.fetch_add(1, std::memory_order_acq_rel);
    for (std::size_t index = 0; index < pendingRms_.size(); ++index)
        publishedRms_[index].store(pendingRms_[index], std::memory_order_relaxed);
    publishedObservedSampleValues_.store(observedSampleValues_, std::memory_order_relaxed);
    generation_.fetch_add(1, std::memory_order_relaxed);
    sequence_.fetch_add(1, std::memory_order_release);
}

EnergyTelemetrySnapshot EnergyTelemetry::snapshot() const noexcept
{
    EnergyTelemetrySnapshot result;
    result.enabled = isEnabled();
    if (!result.enabled)
        return result;
    for (int attempt = 0; attempt < 8; ++attempt) {
        const auto before = sequence_.load(std::memory_order_acquire);
        if ((before & 1U) != 0U)
            continue;
        for (std::size_t index = 0; index < result.rms.size(); ++index)
            result.rms[index] = publishedRms_[index].load(std::memory_order_relaxed);
        result.generation = generation_.load(std::memory_order_relaxed);
        result.observedSampleValues = publishedObservedSampleValues_.load(std::memory_order_relaxed);
        const auto after = sequence_.load(std::memory_order_acquire);
        if (before == after)
            return result;
    }
    result.coherent = false;
    result.rms.fill(0.0F);
    return result;
}

} // namespace reverb::dsp
