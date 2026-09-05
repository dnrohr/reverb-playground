#include "CrashRecovery.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <set>

TEST_CASE("termination outcomes remain explicitly distinguishable")
{
    using reverb::app::TerminationKind;
    const std::set<std::string> names {
        reverb::app::terminationKindName(TerminationKind::forcedCrash),
        reverb::app::terminationKindName(TerminationKind::unhandledException),
        reverb::app::terminationKindName(TerminationKind::assertionFailure),
        reverb::app::terminationKindName(TerminationKind::startupFailure),
        reverb::app::terminationKindName(TerminationKind::cleanExit),
    };
    CHECK(names.size() == 5);
}

TEST_CASE("diagnostic fixture writes a matched private bounded summary and minidump")
{
    auto& recovery = reverb::app::CrashRecovery::instance();
    reverb::app::CrashContext context;
    context.phase = "runtime";
    context.activeFactory = "barr-reference";
    context.graphHash = "0123456789abcdef";
    context.graphRevision = 42;
    context.sampleRate = 48'000.0;
    context.blockSize = 512;
    context.safetyStatus = "clear";
    context.publicationStatus = "active";
    recovery.updateContext(context);

    const auto directory = recovery.reportsDirectory();
    const auto before = directory.findChildFiles(juce::File::findFiles, false, "reverb-playground-*.txt");
    REQUIRE(recovery.writeDiagnosticFixture(
        reverb::app::TerminationKind::forcedCrash, "deterministic-test-fixture"));
    const auto after = directory.findChildFiles(juce::File::findFiles, false, "reverb-playground-*.txt");
    REQUIRE(after.size() >= 1);
    auto summary = after[0];
    for (const auto& candidate : after)
        if (!before.contains(candidate) && candidate.getLastModificationTime() >= summary.getLastModificationTime())
            summary = candidate;
    const auto parsed = nlohmann::json::parse(summary.loadFileAsString().toStdString());
    CHECK(parsed.at("termination") == "forced-crash");
    CHECK(parsed.at("graphRevision") == 42);
    CHECK(parsed.at("privacy").at("automaticUpload") == false);
    CHECK(parsed.at("privacy").at("sourceAudioIncluded") == false);
    CHECK(parsed.at("privacy").at("patchContentIncluded") == false);
    CHECK(parsed.at("privacy").at("pathsIncluded") == false);
    CHECK(parsed.at("breadcrumbs").size() <= reverb::app::CrashRecovery::maximumBreadcrumbs);
    const auto dump = summary.getSiblingFile(summary.getFileNameWithoutExtension() + ".dmp");
    CHECK(dump.existsAsFile());
    CHECK(dump.getSize() > 0);

    // The deterministic fixture owns and removes only its newly-created pair.
    if (!before.contains(summary)) {
        summary.deleteFile();
        dump.deleteFile();
    }
}
