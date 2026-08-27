#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/ReverseCosmicShimmerGraph.h>
#include <reverb/render/ProcessedFileExporter.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <limits>
#include <memory>
#include <thread>

namespace {

class ExportFixture final {
public:
    ExportFixture(const double sampleRate, const int frames, const double amplitude = 0.3, const bool floating = false)
        : file_(juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getNonexistentChildFile("reverb-export-" + juce::Uuid().toString(), ".wav", false))
    {
        juce::AudioBuffer<float> samples(2, frames);
        for (auto frame = 0; frame < frames; ++frame) {
            const auto sample = static_cast<float>(amplitude * std::sin(
                juce::MathConstants<double>::twoPi * 330.0 * frame / sampleRate));
            samples.setSample(0, frame, sample);
            samples.setSample(1, frame, -0.5F * sample);
        }
        std::unique_ptr<juce::OutputStream> stream = file_.createOutputStream();
        REQUIRE(stream != nullptr);
        juce::WavAudioFormat wav;
        auto options = juce::AudioFormatWriterOptions {}
                           .withSampleRate(sampleRate).withNumChannels(2).withBitsPerSample(floating ? 32 : 24);
        if (floating)
            options = options.withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = wav.createWriterFor(stream, options);
        REQUIRE(writer != nullptr);
        REQUIRE(writer->writeFromAudioSampleBuffer(samples, 0, frames));
    }

    ~ExportFixture() { static_cast<void>(file_.deleteFile()); }
    [[nodiscard]] const juce::File& file() const noexcept { return file_; }

private:
    juce::File file_;
};

juce::File destination(const juce::String& stem)
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile(stem + "-" + juce::Uuid().toString(), ".wav", false);
}

reverb::render::ProcessedFileExportSnapshot waitFor(reverb::render::ProcessedFileExporter& exporter)
{
    for (auto attempt = 0; attempt < 1'000; ++attempt) {
        const auto snapshot = exporter.snapshot();
        if (snapshot.state != reverb::render::FileExportState::rendering) {
            exporter.wait();
            return exporter.snapshot();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    exporter.cancel();
    exporter.wait();
    FAIL("export did not finish within the test deadline");
}

juce::MemoryBlock bytes(const juce::File& file)
{
    juce::MemoryBlock result;
    REQUIRE(file.loadFileAsData(result));
    return result;
}

} // namespace

TEST_CASE("Processed WAV export resamples deterministically captures a bounded tail and distinguishes mix mode")
{
    ExportFixture source(44'100.0, 4'410);
    const auto wetA = destination("reverb-wet-a");
    const auto wetB = destination("reverb-wet-b");
    const auto mix = destination("reverb-mix");
    std::string error;
    for (const auto& output : { wetA, wetB }) {
        reverb::render::ProcessedFileExporter exporter;
        REQUIRE(exporter.start({ source.file(), output, reverb::graph::makeBarrReferenceGraph(),
            reverb::render::FileExportMode::wetOnly, 48'000.0, 1.0, 2.5, -80.0 }, error));
        const auto result = waitFor(exporter);
        REQUIRE(result.state == reverb::render::FileExportState::complete);
        REQUIRE(result.progress == 1.0);
        REQUIRE(result.tailFrames >= 96'000);
        REQUIRE(result.tailFrames <= 120'000);
    }
    REQUIRE(bytes(wetA) == bytes(wetB));

    reverb::render::ProcessedFileExporter exporter;
    REQUIRE(exporter.start({ source.file(), mix, reverb::graph::makeBarrReferenceGraph(),
        reverb::render::FileExportMode::auditionMix, 48'000.0, 1.0, 2.5, -80.0 }, error));
    REQUIRE(waitFor(exporter).state == reverb::render::FileExportState::complete);
    REQUIRE(bytes(mix) != bytes(wetA));
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(mix));
    REQUIRE(reader != nullptr);
    REQUIRE(reader->sampleRate == 48'000.0);
    REQUIRE(reader->bitsPerSample == 24);
    REQUIRE(reader->numChannels == 2);
    REQUIRE(reader->lengthInSamples > 4'800);

    static_cast<void>(wetA.deleteFile());
    static_cast<void>(wetB.deleteFile());
    static_cast<void>(mix.deleteFile());
}

TEST_CASE("Processed WAV export cancels atomically and rejects unsafe or existing destinations")
{
    ExportFixture longSource(48'000.0, 480'000);
    const auto cancelled = destination("reverb-cancelled");
    std::string error;
    reverb::render::ProcessedFileExporter exporter;
    REQUIRE(exporter.start({ longSource.file(), cancelled, reverb::graph::makeReverseCosmicShimmerGraph(),
        reverb::render::FileExportMode::wetOnly, 48'000.0, 1.0, 10.0, -80.0 }, error));
    exporter.cancel();
    const auto cancelledResult = waitFor(exporter);
    REQUIRE(cancelledResult.state == reverb::render::FileExportState::cancelled);
    REQUIRE_FALSE(cancelled.exists());

    const auto existing = destination("reverb-existing");
    REQUIRE(existing.replaceWithText("keep me"));
    REQUIRE_FALSE(exporter.start({ longSource.file(), existing, reverb::graph::makeBarrReferenceGraph() }, error));
    REQUIRE(error.find("already exists") != std::string::npos);
    REQUIRE(existing.loadFileAsString() == "keep me");

    REQUIRE(exporter.start({
        .source = longSource.file(),
        .destination = existing,
        .patch = reverb::graph::makeBarrReferenceGraph(),
        .maximumTailSeconds = 0.5,
        .overwriteConfirmed = true,
    }, error));
    REQUIRE(waitFor(exporter).state == reverb::render::FileExportState::complete);
    REQUIRE(existing.loadFileAsString() != "keep me");
    static_cast<void>(existing.deleteFile());

    const auto invalid = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("missing-" + juce::Uuid().toString()).getChildFile("export.wav");
    REQUIRE(exporter.start({ longSource.file(), invalid, reverb::graph::makeBarrReferenceGraph(),
        reverb::render::FileExportMode::wetOnly, 48'000.0, 1.0, 0.5, -80.0 }, error));
    REQUIRE(waitFor(exporter).state == reverb::render::FileExportState::failed);
    REQUIRE_FALSE(invalid.exists());

    ExportFixture unsafeSource(48'000.0, 4'800, 100.0, true);
    const auto unsafe = destination("reverb-unsafe");
    REQUIRE(exporter.start({ unsafeSource.file(), unsafe, reverb::graph::makeBarrReferenceGraph(),
        reverb::render::FileExportMode::wetOnly, 48'000.0, 1.0, 2.0, -80.0 }, error));
    const auto unsafeResult = waitFor(exporter);
    REQUIRE(unsafeResult.state == reverb::render::FileExportState::failed);
    REQUIRE(unsafeResult.error.find("unsafe output") != std::string::npos);
    REQUIRE_FALSE(unsafe.exists());
}

TEST_CASE("Selected-loop export renders the source interval once and then its bounded tail")
{
    ExportFixture source(48'000.0, 48'000);
    const auto output = destination("reverb-selected-loop");
    reverb::render::ProcessedFileExporter exporter;
    std::string error;
    REQUIRE(exporter.start({
        .source = source.file(),
        .destination = output,
        .patch = reverb::graph::makeBarrReferenceGraph(),
        .maximumTailSeconds = 0.5,
        .wetGain = 1.0,
        .dryGain = 0.0,
        .range = reverb::render::FileExportRange::selectedLoop,
        .sourceStartFrame = 12'000,
        .sourceEndFrame = 24'000,
    }, error));
    const auto result = waitFor(exporter);
    REQUIRE(result.state == reverb::render::FileExportState::complete);
    REQUIRE(result.sourceFrames == 12'000);
    REQUIRE(result.renderedFrames == result.sourceFrames + result.tailFrames);

    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(output));
    REQUIRE(reader != nullptr);
    REQUIRE(reader->lengthInSamples == static_cast<juce::int64>(result.renderedFrames));
    static_cast<void>(output.deleteFile());
}
