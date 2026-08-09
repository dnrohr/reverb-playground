#pragma once

#include <reverb/dsp/BarrReference.h>
#include <reverb/dsp/ImpulseCapture.h>
#include <reverb/dsp/EnergyTelemetry.h>
#include <reverb/dsp/NumericalSafetyGuard.h>
#include <reverb/dsp/RuntimeDiagnostics.h>

#include <atomic>
#include <array>
#include <span>

namespace reverb::dsp {

class LiveReferenceHarness final {
public:
    LiveReferenceHarness();
    void prepare(double sampleRate);
    void reset() noexcept;
    void process(
        std::span<const float> inputLeft,
        std::span<const float> inputRight,
        std::span<float> outputLeft,
        std::span<float> outputRight) noexcept;

    void triggerImpulse() noexcept;
    ImpulseCaptureConfig requestImpulseCapture(ImpulseCaptureConfig config) noexcept;
    void setMasterGain(float linearGain) noexcept;
    void setEmergencyMuted(bool muted) noexcept;
    void requestSafetyReset() noexcept;
    void setRuntimeParameter(BarrParameterId id, double value) noexcept;
    void setEnergyTelemetryEnabled(bool enabled) noexcept;

    [[nodiscard]] float masterGain() const noexcept;
    [[nodiscard]] bool isEmergencyMuted() const noexcept;
    [[nodiscard]] bool isSafetyLatched() const noexcept;
    [[nodiscard]] double sampleRate() const noexcept;
    [[nodiscard]] double runtimeParameter(BarrParameterId id) const noexcept;
    [[nodiscard]] ImpulseCaptureState captureState() const noexcept;
    [[nodiscard]] std::uint64_t captureGeneration() const noexcept;
    [[nodiscard]] std::size_t capturedFrames() const noexcept;
    [[nodiscard]] ImpulseCaptureResult copyLatestCapture() const;
    [[nodiscard]] EnergyTelemetrySnapshot energyTelemetrySnapshot() const noexcept;
    [[nodiscard]] RuntimeDiagnosticsSnapshot runtimeDiagnosticsSnapshot() const noexcept;

private:
    BarrReference reference_;
    EnergyTelemetry energyTelemetry_;
    RuntimeDiagnostics diagnostics_;
    ImpulseCapture capture_;
    NumericalSafetyGuard leftGuard_;
    NumericalSafetyGuard rightGuard_;
    std::atomic<float> masterGain_ { 0.5F };
    std::atomic<bool> impulsePending_ {};
    std::atomic<bool> emergencyMuted_ {};
    std::atomic<bool> safetyResetPending_ {};
    std::atomic<bool> safetyLatched_ {};
    std::atomic<double> sampleRate_ {};
    static constexpr auto parameterCount = static_cast<std::size_t>(BarrParameterId::count);
    std::array<std::atomic<double>, parameterCount> parameterTargets_ {};
};

} // namespace reverb::dsp
