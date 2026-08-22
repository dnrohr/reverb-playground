#pragma once

#include <atomic>
#include <cstdint>

namespace reverb::app {

enum class StartupPhase : std::uint8_t {
    showingShell,
    connectingAudio,
    openingEditor,
    ready,
    failed,
};

class StartupProgress final {
public:
    [[nodiscard]] StartupPhase phase() const noexcept
    {
        return phase_.load(std::memory_order_acquire);
    }

    bool advanceTo(const StartupPhase next) noexcept
    {
        auto current = phase_.load(std::memory_order_relaxed);
        while (canAdvance(current, next)) {
            if (phase_.compare_exchange_weak(
                    current, next, std::memory_order_release, std::memory_order_relaxed)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] static constexpr bool canAdvance(
        const StartupPhase current, const StartupPhase next) noexcept
    {
        if (current == StartupPhase::failed || current == StartupPhase::ready) {
            return false;
        }
        if (next == StartupPhase::failed) {
            return true;
        }
        return static_cast<std::uint8_t>(next) > static_cast<std::uint8_t>(current)
            && next != StartupPhase::failed;
    }

private:
    std::atomic<StartupPhase> phase_ {StartupPhase::showingShell};
};

} // namespace reverb::app
