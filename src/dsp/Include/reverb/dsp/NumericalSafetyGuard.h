#pragma once

#include <cstddef>
#include <span>

namespace reverb::dsp {

enum class SafetyViolation {
    none,
    nonFinite,
    runawayLevel
};

struct SafetyStatus {
    SafetyViolation violation { SafetyViolation::none };
    std::size_t sampleIndex {};
};

class NumericalSafetyGuard final {
public:
    explicit NumericalSafetyGuard(float maximumAbsoluteSample = 16.0F) noexcept;

    [[nodiscard]] SafetyStatus inspectAndMute(std::span<float> samples) noexcept;
    [[nodiscard]] bool isMuted() const noexcept;
    void reset() noexcept;

private:
    float maximumAbsoluteSample_;
    bool muted_ {};
};

} // namespace reverb::dsp
