#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace reverb::dsp::pitch_shift {

enum class GrainDirection { forward, reverse };

inline constexpr double minimumSemitones = -24.0;
inline constexpr double maximumSemitones = 24.0;
inline constexpr double defaultSemitones = 12.0;
inline constexpr double minimumGrainMilliseconds = 20.0;
inline constexpr double maximumGrainMilliseconds = 120.0;
inline constexpr double defaultGrainMilliseconds = 60.0;
inline constexpr double minimumOverlap = 0.10;
inline constexpr double maximumOverlap = 1.0;
inline constexpr double defaultOverlap = 0.50;
inline constexpr double parameterTransitionMilliseconds = 20.0;
inline constexpr double minimumPreparationSampleRate = 22'050.0;
inline constexpr double maximumPreparationSampleRate = 192'000.0;
inline constexpr std::array<double, 3> qualificationSampleRates { 44'100.0, 48'000.0, 96'000.0 };
inline constexpr std::size_t interpolationGuardSamples = 4;
inline constexpr std::size_t steadyScalarOperationsPerSample = 72;
inline constexpr std::size_t parameterTransitionScalarOperationsPerSample = 136;
inline constexpr std::size_t topologyTransitionScalarOperationsPerSample = 272;
inline constexpr std::size_t blockOverheadScalarOperations = 64;
inline constexpr double maximumEqualPowerOutputMagnitude = 1.414'214;

[[nodiscard]] inline double ratioForSemitones(const double semitones) noexcept
{
    const auto finiteSemitones = std::isfinite(semitones) ? semitones : defaultSemitones;
    return std::pow(2.0, std::clamp(finiteSemitones, minimumSemitones, maximumSemitones) / 12.0);
}

[[nodiscard]] inline double maximumReadExcursionMilliseconds() noexcept
{
    return (1.0 + ratioForSemitones(maximumSemitones)) * maximumGrainMilliseconds;
}

[[nodiscard]] inline std::size_t reportedLatencySamples(const double sampleRate) noexcept
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0) return 0;
    return static_cast<std::size_t>(std::ceil(
        maximumReadExcursionMilliseconds() * sampleRate / 1'000.0)) + 2;
}

[[nodiscard]] inline std::size_t preparedStorageSamples(const double sampleRate) noexcept
{
    const auto latency = reportedLatencySamples(sampleRate);
    return latency == 0 ? 0 : latency + 2;
}

[[nodiscard]] inline std::size_t preparedStorageBytes(const double sampleRate) noexcept
{
    return preparedStorageSamples(sampleRate) * sizeof(float);
}

[[nodiscard]] inline std::size_t worstCaseBlockOperations(const std::size_t frames) noexcept
{
    return parameterTransitionScalarOperationsPerSample * frames + blockOverheadScalarOperations;
}

} // namespace reverb::dsp::pitch_shift
