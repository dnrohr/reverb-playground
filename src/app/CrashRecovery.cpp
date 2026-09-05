#include "CrashRecovery.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

#if JUCE_WINDOWS
#include <Windows.h>
#include <DbgHelp.h>
#endif

namespace reverb::app {
namespace {

CrashRecovery* activeCrashRecovery {};
#if JUCE_WINDOWS
LPTOP_LEVEL_EXCEPTION_FILTER previousExceptionFilter {};
#endif

juce::String bounded(const juce::String& value)
{
    return value.substring(0, CrashRecovery::maximumFieldCharacters)
        .replaceCharacters("\r\n", "  ");
}

bool replaceAtomically(const juce::File& destination, const juce::String& text)
{
    destination.getParentDirectory().createDirectory();
    const auto temporary = destination.getSiblingFile(destination.getFileName() + ".partial");
    temporary.deleteFile();
    if (!temporary.replaceWithText(text, false, false, "\n"))
        return false;
    return temporary.replaceFileIn(destination);
}

#if JUCE_WINDOWS
LONG WINAPI processExceptionFilter(EXCEPTION_POINTERS* exceptionPointers)
{
    if (activeCrashRecovery != nullptr)
        activeCrashRecovery->handleWindowsException(
            exceptionPointers->ExceptionRecord->ExceptionCode, exceptionPointers);
    return previousExceptionFilter != nullptr
        ? previousExceptionFilter(exceptionPointers) : EXCEPTION_CONTINUE_SEARCH;
}
#endif

} // namespace

const char* terminationKindName(const TerminationKind kind) noexcept
{
    switch (kind) {
    case TerminationKind::forcedCrash: return "forced-crash";
    case TerminationKind::unhandledException: return "unhandled-exception";
    case TerminationKind::assertionFailure: return "assertion-failure";
    case TerminationKind::startupFailure: return "startup-failure";
    case TerminationKind::cleanExit: return "clean-exit";
    }
    return "unknown";
}

CrashRecovery& CrashRecovery::instance()
{
    static CrashRecovery service;
    return service;
}

CrashRecovery::~CrashRecovery()
{
    stopCrashWorker();
}

juce::File CrashRecovery::reportsDirectory() const
{
    const std::scoped_lock lock(mutex_);
    if (reportsDirectory_ != juce::File {})
        return reportsDirectory_;
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Reverb Playground").getChildFile("Crash Reports");
}

juce::File CrashRecovery::sessionMarker() const { return reportsDirectory_.getChildFile("active-session.json"); }
juce::File CrashRecovery::autosaveFile() const { return reportsDirectory_.getChildFile("last-known-valid.autosave.rvp.json"); }
juce::File CrashRecovery::recoveryAttemptsFile() const { return reportsDirectory_.getChildFile("recovery-attempts.txt"); }

void CrashRecovery::startSession(const bool standalone)
{
    const std::scoped_lock lock(mutex_);
    if (sessionStarted_) return;
    standalone_ = standalone;
    reportsDirectory_ = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Reverb Playground").getChildFile("Crash Reports");
    reportsDirectory_.createDirectory();

    if (standalone_ && sessionMarker().existsAsFile()) {
        recovery_.available = autosaveFile().existsAsFile();
        recovery_.candidateName = autosaveFile().getFileName();
        recovery_.candidateHash = juce::String::toHexString(
            autosaveFile().loadFileAsString().hashCode64());
        recovery_.incidentId = juce::JSON::parse(sessionMarker().loadFileAsString())
            .getProperty("incidentId", "previous-session").toString();
        recovery_.attempts = recoveryAttemptsFile().existsAsFile()
            ? recoveryAttemptsFile().loadFileAsString().getIntValue() : 0;
        recovery_.quarantined = recovery_.attempts >= 2;
        recovery_.message = recovery_.quarantined
            ? "The recovery candidate failed twice and is quarantined. Start clean or inspect reports."
            : "The previous standalone session did not exit cleanly. Recovery is optional and starts muted.";
    }

    const auto incident = nextIncidentId();
    const nlohmann::json marker {
        { "schemaVersion", 1 }, { "incidentId", incident.toStdString() },
        { "started", juce::Time::getCurrentTime().toISO8601(true).toStdString() },
        { "mode", standalone_ ? "standalone" : "vst3" },
    };
    if (standalone_)
        static_cast<void>(replaceAtomically(sessionMarker(), marker.dump(2)));
    noteBreadcrumb("session-started");
    sessionStarted_ = true;
    activeCrashRecovery = this;
    startCrashWorker();
    installProcessHandler();
    retainNewestIncidents();
}

void CrashRecovery::finishSessionCleanly()
{
    const std::scoped_lock lock(mutex_);
    if (!sessionStarted_) return;
    noteBreadcrumb("clean-exit");
    if (standalone_) {
        sessionMarker().deleteFile();
        recoveryAttemptsFile().deleteFile();
    }
    sessionStarted_ = false;
}

void CrashRecovery::recordStartupFailure(const juce::String& detail)
{
    static_cast<void>(writeDiagnosticFixture(TerminationKind::startupFailure, detail));
}

void CrashRecovery::updateContext(const CrashContext& context)
{
    const std::scoped_lock lock(mutex_);
    context_ = context;
    noteBreadcrumb("context:" + bounded(context.phase) + ":revision-" + juce::String(context.graphRevision));
    if (standalone_ && sessionStarted_) {
        const auto existing = juce::JSON::parse(sessionMarker().loadFileAsString());
        const nlohmann::json marker {
            { "schemaVersion", 1 },
            { "incidentId", existing.getProperty("incidentId", "active-session").toString().toStdString() },
            { "lastHeartbeat", juce::Time::getCurrentTime().toISO8601(true).toStdString() },
            { "phase", bounded(context.phase).toStdString() },
            { "graphRevision", context.graphRevision },
        };
        static_cast<void>(replaceAtomically(sessionMarker(), marker.dump(2)));
    }
}

void CrashRecovery::storeRecoveryCandidate(const juce::String& patchJson)
{
    if (patchJson.isEmpty() || patchJson.length() > 8 * 1024 * 1024) return;
    const std::scoped_lock lock(mutex_);
    if (!standalone_ || !sessionStarted_) return;
    // Preserve the prior candidate byte-for-byte until the user explicitly
    // restores or declines it; the default graph rendered behind the recovery
    // prompt must not silently replace the state being offered.
    if (recovery_.available) return;
    if (replaceAtomically(autosaveFile(), patchJson))
        noteBreadcrumb("autosave-valid-graph");
}

RecoveryState CrashRecovery::recoveryState() const
{
    const std::scoped_lock lock(mutex_);
    return recovery_;
}

juce::String CrashRecovery::recoveryStateJson() const
{
    const auto state = recoveryState();
    return nlohmann::json {
        { "available", state.available }, { "quarantined", state.quarantined },
        { "attempts", state.attempts }, { "incidentId", state.incidentId.toStdString() },
        { "candidateName", state.candidateName.toStdString() },
        { "candidateHash", state.candidateHash.toStdString() }, { "message", state.message.toStdString() },
    }.dump();
}

juce::String CrashRecovery::restoreRecoveryCandidate()
{
    const std::scoped_lock lock(mutex_);
    if (!recovery_.available || recovery_.quarantined || !autosaveFile().existsAsFile()) return {};
    const auto candidate = autosaveFile().loadFileAsString();
    ++recovery_.attempts;
    static_cast<void>(replaceAtomically(recoveryAttemptsFile(), juce::String(recovery_.attempts)));
    noteBreadcrumb("recovery-opted-in");
    recovery_.available = false;
    return candidate;
}

void CrashRecovery::declineRecovery()
{
    const std::scoped_lock lock(mutex_);
    recovery_.available = false;
    recovery_.message = "Recovery declined; the local report remains available.";
    recoveryAttemptsFile().deleteFile();
    noteBreadcrumb("recovery-declined");
}

void CrashRecovery::openReportsFolder() const
{
    reportsDirectory().startAsProcess();
}

void CrashRecovery::installProcessHandler()
{
#if JUCE_WINDOWS
    previousExceptionFilter = SetUnhandledExceptionFilter(processExceptionFilter);
#endif
}

juce::String CrashRecovery::nextIncidentId() const
{
    return juce::Time::getCurrentTime().formatted("%Y%m%dT%H%M%S") + "-"
        + juce::String::toHexString(juce::Random::getSystemRandom().nextInt()).paddedLeft('0', 8);
}

void CrashRecovery::noteBreadcrumb(const juce::String& value)
{
    breadcrumbs_.add(bounded(value));
    while (breadcrumbs_.size() > maximumBreadcrumbs) breadcrumbs_.remove(0);
}

bool CrashRecovery::writeDiagnosticFixture(const TerminationKind kind, const juce::String& detail)
{
    return writeIncident(kind, detail, nullptr);
}

void CrashRecovery::handleWindowsException(const std::uint32_t code, void* exceptionPointers) noexcept
{
    bool expected = false;
    if (!exceptionQueued_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return;
    pendingExceptionCode_.store(code, std::memory_order_release);
#if JUCE_WINDOWS
    pendingExceptionThreadId_.store(GetCurrentThreadId(), std::memory_order_release);
#else
    pendingExceptionThreadId_.store(0, std::memory_order_release);
#endif
    pendingExceptionPointers_.store(exceptionPointers, std::memory_order_release);
#if JUCE_WINDOWS
    if (crashEvent_ != nullptr && crashCompleteEvent_ != nullptr) {
        ResetEvent(static_cast<HANDLE>(crashCompleteEvent_));
        SetEvent(static_cast<HANDLE>(crashEvent_));
        static_cast<void>(WaitForSingleObject(static_cast<HANDLE>(crashCompleteEvent_), 10'000));
    }
#endif
}

void CrashRecovery::startCrashWorker()
{
#if JUCE_WINDOWS
    if (crashWorker_.joinable()) return;
    crashEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    crashCompleteEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (crashEvent_ == nullptr || crashCompleteEvent_ == nullptr) {
        if (crashEvent_ != nullptr) CloseHandle(static_cast<HANDLE>(crashEvent_));
        if (crashCompleteEvent_ != nullptr) CloseHandle(static_cast<HANDLE>(crashCompleteEvent_));
        crashEvent_ = nullptr;
        crashCompleteEvent_ = nullptr;
        return;
    }
    workerStopping_.store(false, std::memory_order_release);
    crashWorker_ = std::thread([this] {
        while (!workerStopping_.load(std::memory_order_acquire)) {
            if (WaitForSingleObject(static_cast<HANDLE>(crashEvent_), INFINITE) != WAIT_OBJECT_0)
                break;
            if (workerStopping_.load(std::memory_order_acquire)) break;
            const auto code = pendingExceptionCode_.load(std::memory_order_acquire);
            const auto threadId = pendingExceptionThreadId_.load(std::memory_order_acquire);
            const auto* pointers = pendingExceptionPointers_.load(std::memory_order_acquire);
            static_cast<void>(writeIncident(TerminationKind::unhandledException,
                "Windows exception 0x" + juce::String::toHexString(static_cast<int>(code)),
                const_cast<void*>(pointers), threadId));
            exceptionQueued_.store(false, std::memory_order_release);
            SetEvent(static_cast<HANDLE>(crashCompleteEvent_));
        }
    });
#endif
}

void CrashRecovery::stopCrashWorker()
{
#if JUCE_WINDOWS
    workerStopping_.store(true, std::memory_order_release);
    if (crashEvent_ != nullptr) SetEvent(static_cast<HANDLE>(crashEvent_));
    if (crashWorker_.joinable()) crashWorker_.join();
    if (crashEvent_ != nullptr) CloseHandle(static_cast<HANDLE>(crashEvent_));
    if (crashCompleteEvent_ != nullptr) CloseHandle(static_cast<HANDLE>(crashCompleteEvent_));
    crashEvent_ = nullptr;
    crashCompleteEvent_ = nullptr;
    if (activeCrashRecovery == this) {
        SetUnhandledExceptionFilter(previousExceptionFilter);
        activeCrashRecovery = nullptr;
    }
#endif
}

bool CrashRecovery::writeIncident(const TerminationKind kind, const juce::String& detail,
    void* exceptionPointers, const std::uint32_t exceptionThreadId)
{
    bool expected = false;
    if (!handlerActive_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return false;

    const std::scoped_lock lock(mutex_);
    if (reportsDirectory_ == juce::File {}) {
        reportsDirectory_ = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Reverb Playground").getChildFile("Crash Reports");
        reportsDirectory_.createDirectory();
    }
    const auto incident = nextIncidentId();
    const auto stem = "reverb-playground-" + incident;
    const auto dumpPartial = reportsDirectory_.getChildFile(stem + ".dmp.partial");
    const auto dump = reportsDirectory_.getChildFile(stem + ".dmp");
    bool dumpComplete = false;
#if JUCE_WINDOWS
    if (auto file = CreateFileW(dumpPartial.getFullPathName().toWideCharPointer(), GENERIC_WRITE, 0,
            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr); file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION exceptionInformation {};
        exceptionInformation.ThreadId = exceptionThreadId != 0 ? exceptionThreadId : GetCurrentThreadId();
        exceptionInformation.ExceptionPointers = static_cast<EXCEPTION_POINTERS*>(exceptionPointers);
        exceptionInformation.ClientPointers = FALSE;
        dumpComplete = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
            MiniDumpNormal, exceptionPointers != nullptr ? &exceptionInformation : nullptr, nullptr, nullptr) != FALSE;
        CloseHandle(file);
        if (dumpComplete) {
            dumpComplete = dumpPartial.replaceFileIn(dump);
        }
    }
#else
    juce::ignoreUnused(exceptionPointers);
    dumpComplete = dumpPartial.replaceWithText("Minidumps are supported by the Windows build.")
        && dumpPartial.moveFileTo(dump);
#endif

    nlohmann::json summary {
        { "schemaVersion", 1 }, { "incidentId", incident.toStdString() },
        { "termination", terminationKindName(kind) },
        { "timestamp", juce::Time::getCurrentTime().toISO8601(true).toStdString() },
        { "productVersion", REVERB_PRODUCT_VERSION }, { "buildCommit", REVERB_BUILD_COMMIT },
        { "platform", juce::SystemStats::getOperatingSystemName().toStdString() },
        { "mode", standalone_ ? "standalone" : "vst3" },
        { "phase", bounded(context_.phase).toStdString() },
        { "activeFactory", bounded(context_.activeFactory).toStdString() },
        { "graphHash", bounded(context_.graphHash).toStdString() },
        { "graphRevision", context_.graphRevision },
        { "audio", { { "sampleRate", context_.sampleRate }, { "blockSize", context_.blockSize } } },
        { "safetyStatus", bounded(context_.safetyStatus).toStdString() },
        { "publicationStatus", bounded(context_.publicationStatus).toStdString() },
        { "detail", bounded(detail).toStdString() }, { "minidumpComplete", dumpComplete },
        { "privacy", { { "automaticUpload", false }, { "sourceAudioIncluded", false },
            { "patchContentIncluded", false }, { "pathsIncluded", false } } },
        { "breadcrumbs", nlohmann::json::array() },
    };
    for (const auto& breadcrumb : breadcrumbs_)
        summary["breadcrumbs"].push_back(breadcrumb.toStdString());
    const auto summaryComplete = replaceAtomically(
        reportsDirectory_.getChildFile(stem + ".txt"), juce::String(summary.dump(2)));
    if (standalone_) {
        const nlohmann::json marker {
            { "schemaVersion", 1 }, { "incidentId", incident.toStdString() },
            { "abnormalTermination", terminationKindName(kind) },
        };
        static_cast<void>(replaceAtomically(sessionMarker(), marker.dump(2)));
    }
    retainNewestIncidents();
    handlerActive_.store(false, std::memory_order_release);
    return dumpComplete && summaryComplete;
}

void CrashRecovery::retainNewestIncidents()
{
    auto summaries = reportsDirectory_.findChildFiles(juce::File::findFiles, false,
        "reverb-playground-*.txt");
    std::sort(summaries.begin(), summaries.end(), [](const auto& a, const auto& b) {
        return a.getLastModificationTime() > b.getLastModificationTime();
    });
    for (int index = maximumRetainedIncidents; index < summaries.size(); ++index) {
        const auto stem = summaries[index].getFileNameWithoutExtension();
        summaries[index].deleteFile();
        reportsDirectory_.getChildFile(stem + ".dmp").deleteFile();
        reportsDirectory_.getChildFile(stem + ".dmp.partial").deleteFile();
    }
}

} // namespace reverb::app
