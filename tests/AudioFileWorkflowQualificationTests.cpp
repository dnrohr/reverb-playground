#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/DenseFigureEightGraph.h>
#include <reverb/graph/FourLineFdnGraph.h>
#include <reverb/graph/GravityDiffusionGraph.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/graph/ReverseCosmicShimmerGraph.h>
#include <reverb/graph/SafeParallelShimmerGraph.h>
#include <reverb/graph/SplitFeedbackShimmerGraph.h>
#include <reverb/render/ProcessedFileExporter.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

namespace {

enum class ProgramKind { speechPercussion, sustainedChord, fullMix };

juce::File writeProgram(const ProgramKind kind)
{
    constexpr auto sampleRate = 48'000.0;
    constexpr auto frames = 48'000;
    juce::AudioBuffer<float> samples(2, frames);
    std::uint32_t noise = 0x13579bdfU;
    for (auto frame = 0; frame < frames; ++frame) {
        const auto time = frame / sampleRate;
        noise ^= noise << 13U;
        noise ^= noise >> 17U;
        noise ^= noise << 5U;
        const auto random = static_cast<float>(noise) / static_cast<float>(std::numeric_limits<std::uint32_t>::max());
        const auto boundedNoise = (random * 2.0F - 1.0F) * 0.04F;
        const auto chord = 0.08F * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 220.0 * time)
            + std::sin(juce::MathConstants<double>::twoPi * 277.18 * time)
            + std::sin(juce::MathConstants<double>::twoPi * 329.63 * time));
        const auto pulsePhase = frame % 6'000;
        const auto pulse = pulsePhase < 480
            ? 0.35F * static_cast<float>(std::exp(-static_cast<double>(pulsePhase) / 90.0)) : 0.0F;
        const auto syllable = (frame / 2'400) % 2 == 0 ? boundedNoise : 0.2F * boundedNoise;
        auto left = kind == ProgramKind::speechPercussion ? pulse + syllable
            : kind == ProgramKind::sustainedChord ? chord
            : 0.65F * chord + 0.5F * pulse + syllable;
        auto right = kind == ProgramKind::speechPercussion ? -0.7F * pulse + syllable
            : kind == ProgramKind::sustainedChord ? 0.85F * chord
            : 0.55F * chord - 0.4F * pulse - syllable;
        samples.setSample(0, frame, left);
        samples.setSample(1, frame, right);
    }
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getNonexistentChildFile("reverb-qualification-" + juce::Uuid().toString(), ".wav", false);
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions {}
        .withSampleRate(sampleRate).withNumChannels(2).withBitsPerSample(24));
    REQUIRE(writer != nullptr);
    REQUIRE(writer->writeFromAudioSampleBuffer(samples, 0, frames));
    return file;
}

reverb::render::ProcessedFileExportSnapshot waitFor(reverb::render::ProcessedFileExporter& exporter)
{
    for (auto attempt = 0; attempt < 2'000; ++attempt) {
        const auto snapshot = exporter.snapshot();
        if (snapshot.state != reverb::render::FileExportState::rendering) {
            exporter.wait();
            return exporter.snapshot();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    exporter.cancel();
    exporter.wait();
    FAIL("qualification export exceeded its deadline");
}

reverb::graph::GraphDocument loadFactory(const char* filename)
{
    const auto file = juce::File(REVERB_FACTORY_PATCH_DIR).getChildFile(filename);
    return reverb::graph::parsePatchJson(file.loadFileAsString().toStdString());
}

} // namespace

TEST_CASE("Representative program material qualifies every milestone fourteen reverb family")
{
    const std::array programs {
        writeProgram(ProgramKind::speechPercussion),
        writeProgram(ProgramKind::sustainedChord),
        writeProgram(ProgramKind::fullMix),
    };
    const std::array graphs {
        reverb::graph::makeBarrReferenceGraph(),
        loadFactory("modulated-cosmic-reverse.rvp.json"),
        reverb::graph::makeGravityDiffusionGraph(),
        reverb::graph::makeDenseFigureEightGraph(),
        reverb::graph::makeFourLineFdnGraph(),
        reverb::graph::makeSafeParallelShimmerGraph(),
        reverb::graph::makeSplitFeedbackShimmerGraph(),
        reverb::graph::makeReverseCosmicShimmerGraph(),
    };

    for (std::size_t program = 0; program < programs.size(); ++program) {
        for (std::size_t topology = 0; topology < graphs.size(); ++topology) {
            const auto destination = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                         .getNonexistentChildFile("reverb-qualified-" + juce::Uuid().toString(), ".wav", false);
            reverb::render::ProcessedFileExporter exporter;
            std::string error;
            INFO("program " << program << ", topology " << topology);
            REQUIRE(exporter.start({
                .source = programs[program],
                .destination = destination,
                .patch = graphs[topology],
                .mode = reverb::render::FileExportMode::wetOnly,
                .outputSampleRate = 48'000.0,
                .auditionGain = 0.5,
                .maximumTailSeconds = 0.5,
                .silenceThresholdDb = -80.0,
            }, error));
            const auto result = waitFor(exporter);
            REQUIRE(result.state == reverb::render::FileExportState::complete);
            REQUIRE(result.error.empty());
            REQUIRE(result.renderedFrames >= 48'000);
            REQUIRE(result.renderedFrames <= 72'000);
            REQUIRE(destination.existsAsFile());
            juce::AudioFormatManager manager;
            manager.registerBasicFormats();
            auto reader = std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(destination));
            REQUIRE(reader != nullptr);
            REQUIRE(reader->numChannels == 2);
            REQUIRE(reader->sampleRate == 48'000.0);
            REQUIRE(reader->lengthInSamples >= 48'000);
            REQUIRE(reader->lengthInSamples <= 72'000);
            static_cast<void>(destination.deleteFile());
        }
    }
    for (const auto& program : programs) static_cast<void>(program.deleteFile());
}
