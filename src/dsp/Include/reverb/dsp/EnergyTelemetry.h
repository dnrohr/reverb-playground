#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace reverb::dsp {

enum class BarrEnergyLane : std::size_t {
    input,
    sum,
    inputFilter,
    diffuserOne,
    diffuserTwo,
    tankOne,
    tankTwo,
    leftTap,
    rightTap,
    output,
    count,
};

inline constexpr auto barrEnergyLaneCount = static_cast<std::size_t>(BarrEnergyLane::count);

[[nodiscard]] std::string_view barrEnergyLaneNodeId(BarrEnergyLane lane) noexcept;

struct EnergyTelemetrySnapshot final {
    bool enabled {};
    bool coherent { true };
    std::uint64_t generation {};
    std::uint64_t observedSampleValues {};
    std::array<float, barrEnergyLaneCount> rms {};
};

class EnergyTelemetry final {
public:
    EnergyTelemetry() noexcept;

    void prepare(double sampleRate) noexcept;
    void setEnabled(bool enabled) noexcept;
    [[nodiscard]] bool isEnabled() const noexcept;

    // Audio-thread-only measurement API. beginBlock performs the sole disabled-path check.
    [[nodiscard]] bool beginBlock() noexcept;
    void observeMono(BarrEnergyLane lane, std::span<const float> samples) noexcept;
    void observeStereo(
        BarrEnergyLane lane,
        std::span<const float> left,
        std::span<const float> right) noexcept;
    void endBlock(std::size_t frameCount) noexcept;

    // Safe for the message thread. A failed bounded seqlock read is marked incoherent.
    [[nodiscard]] EnergyTelemetrySnapshot snapshot() const noexcept;

private:
    void publish() noexcept;

    std::atomic<bool> enabled_ {};
    std::atomic<std::uint64_t> sequence_ {};
    std::atomic<std::uint64_t> generation_ {};
    std::atomic<std::uint64_t> publishedObservedSampleValues_ {};
    std::array<std::atomic<float>, barrEnergyLaneCount> publishedRms_ {};
    std::array<float, barrEnergyLaneCount> blockRms_ {};
    std::array<float, barrEnergyLaneCount> pendingRms_ {};
    std::uint64_t observedSampleValues_ {};
    std::size_t framesUntilPublish_ { 1 };
    std::size_t publishIntervalFrames_ { 1 };
};

} // namespace reverb::dsp
