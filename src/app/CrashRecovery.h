#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

namespace reverb::app {

enum class TerminationKind {
    forcedCrash,
    unhandledException,
    assertionFailure,
    startupFailure,
    cleanExit,
};

[[nodiscard]] const char* terminationKindName(TerminationKind kind) noexcept;

struct CrashContext final {
    juce::String phase { "startup" };
    juce::String activeFactory { "unknown" };
    juce::String graphHash { "unknown" };
    std::uint64_t graphRevision {};
    double sampleRate {};
    int blockSize {};
    juce::String safetyStatus { "unknown" };
    juce::String publicationStatus { "unknown" };
};

struct RecoveryState final {
    bool available {};
    bool quarantined {};
    int attempts {};
    juce::String incidentId;
    juce::String candidateName;
    juce::String candidateHash;
    juce::String message;
};

// Process-level crash handling and standalone recovery bookkeeping. All mutating
// methods are message/startup-thread APIs; the audio callback only reads/writes
// the processor's existing lock-free mute state and never calls this service.
class CrashRecovery final {
public:
    static CrashRecovery& instance();

    void startSession(bool standalone);
    void finishSessionCleanly();
    void recordStartupFailure(const juce::String& detail);
    void updateContext(const CrashContext& context);
    void storeRecoveryCandidate(const juce::String& patchJson);

    [[nodiscard]] RecoveryState recoveryState() const;
    [[nodiscard]] juce::String recoveryStateJson() const;
    [[nodiscard]] juce::String restoreRecoveryCandidate();
    void declineRecovery();
    void openReportsFolder() const;

    // Deterministic development/qualification fixture. A supported fixture
    // writes the same summary/minidump pair as the process handler without
    // terminating the caller.
    [[nodiscard]] bool writeDiagnosticFixture(TerminationKind kind, const juce::String& detail);
    void handleWindowsException(std::uint32_t code, void* exceptionPointers) noexcept;

    [[nodiscard]] juce::File reportsDirectory() const;
    static constexpr int maximumRetainedIncidents = 8;
    static constexpr int maximumBreadcrumbs = 16;
    static constexpr int maximumFieldCharacters = 256;

private:
    CrashRecovery() = default;
    ~CrashRecovery();
    CrashRecovery(const CrashRecovery&) = delete;
    CrashRecovery& operator=(const CrashRecovery&) = delete;

    void installProcessHandler();
    void startCrashWorker();
    void stopCrashWorker();
    [[nodiscard]] bool writeIncident(TerminationKind kind, const juce::String& detail,
        void* exceptionPointers, std::uint32_t exceptionThreadId = 0);
    void retainNewestIncidents();
    void noteBreadcrumb(const juce::String& value);
    [[nodiscard]] juce::String nextIncidentId() const;
    [[nodiscard]] juce::File sessionMarker() const;
    [[nodiscard]] juce::File autosaveFile() const;
    [[nodiscard]] juce::File recoveryAttemptsFile() const;

    mutable std::mutex mutex_;
    juce::File reportsDirectory_;
    CrashContext context_;
    juce::StringArray breadcrumbs_;
    RecoveryState recovery_;
    bool sessionStarted_ {};
    bool standalone_ {};
    std::atomic<bool> handlerActive_ {};
    std::atomic<bool> exceptionQueued_ {};
    std::atomic<bool> workerStopping_ {};
    std::atomic<std::uint32_t> pendingExceptionCode_ {};
    std::atomic<std::uint32_t> pendingExceptionThreadId_ {};
    std::atomic<void*> pendingExceptionPointers_ {};
    void* crashEvent_ {};
    void* crashCompleteEvent_ {};
    std::thread crashWorker_;
};

} // namespace reverb::app
