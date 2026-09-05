#include "PluginProcessor.h"

#include <reverb/audio/PreparedAudioFileSource.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <memory>
#include <limits>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

class AudioFixture final {
public:
    AudioFixture(
        juce::AudioFormat& format,
        const juce::String& extension,
        const int channels,
        const double sampleRate,
        const int frames,
        const int bits,
        const bool floatingPoint = false,
        const double amplitude = 0.35,
        const bool injectNonFinite = false)
        : file_(juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getNonexistentChildFile(
                        "reverb-playground-audio-source-" + juce::Uuid().toString(),
                        extension,
                        false))
    {
        juce::AudioBuffer<float> buffer(channels, frames);
        for (auto channel = 0; channel < channels; ++channel) {
            for (auto frame = 0; frame < frames; ++frame) {
                const auto phase = static_cast<double>(frame) / sampleRate;
                const auto base = amplitude * std::sin(juce::MathConstants<double>::twoPi * 220.0 * phase);
                const auto value = channel == 0 ? base : -0.5 * base + 0.05;
                buffer.setSample(channel, frame, static_cast<float>(value));
            }
        }
        if (injectNonFinite)
            buffer.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());

        std::unique_ptr<juce::OutputStream> stream = file_.createOutputStream();
        REQUIRE(stream != nullptr);
        auto options = juce::AudioFormatWriterOptions {}
                           .withSampleRate(sampleRate)
                           .withNumChannels(channels)
                           .withBitsPerSample(bits);
        if (floatingPoint)
            options = options.withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = format.createWriterFor(stream, options);
        REQUIRE(writer != nullptr);
        REQUIRE(writer->writeFromAudioSampleBuffer(buffer, 0, frames));
    }

    ~AudioFixture() { static_cast<void>(file_.deleteFile()); }

    [[nodiscard]] const juce::File& file() const noexcept { return file_; }

private:
    juce::File file_;
};

std::vector<float> render(
    reverb::audio::PreparedAudioFileSource& source,
    const std::size_t frames,
    const std::span<const std::size_t> partitions)
{
    std::vector<float> result(frames * 2, 0.0F);
    std::size_t cursor = 0;
    std::size_t partition = 0;
    while (cursor < frames) {
        const auto count = std::min(partitions[partition % partitions.size()], frames - cursor);
        source.process(
            std::span<float>(result).subspan(cursor, count),
            std::span<float>(result).subspan(frames + cursor, count));
        cursor += count;
        ++partition;
    }
    return result;
}

bool allFinite(const std::span<const float> samples)
{
    return std::ranges::all_of(samples, [](const float sample) { return std::isfinite(sample); });
}

static_assert(noexcept(std::declval<reverb::audio::PreparedAudioFileSource&>().process(
    std::declval<std::span<float>>(),
    std::declval<std::span<float>>())));

} // namespace

TEST_CASE("Prepared audio source accepts WAV AIFF and FLAC with explicit channel policy")
{
    juce::WavAudioFormat wav;
    juce::AiffAudioFormat aiff;
    juce::FlacAudioFormat flac;
    AudioFixture monoWav(wav, ".wav", 1, 48'000.0, 24'000, 16);
    AudioFixture floatWav(wav, ".wav", 2, 48'000.0, 24'000, 32, true);
    AudioFixture stereoAiff(aiff, ".aiff", 2, 44'100.0, 22'050, 24);
    AudioFixture stereoFlac(flac, ".flac", 2, 96'000.0, 48'000, 24);
    AudioFixture surroundWav(wav, ".wav", 3, 48'000.0, 24'000, 16);

    reverb::audio::PreparedAudioFileSource source;
    source.prepare(48'000.0, 128);
    std::string error;
    for (const auto* fixture : { &monoWav, &floatWav, &stereoAiff, &stereoFlac }) {
        REQUIRE(source.loadFile(fixture->file(), error));
        REQUIRE(error.empty());
        const auto snapshot = source.snapshot();
        REQUIRE(snapshot.prepared);
        REQUIRE(snapshot.channels >= 1);
        REQUIRE(snapshot.channels <= 2);
        REQUIRE(snapshot.frameCount > 0);
        REQUIRE(snapshot.sourceSampleRate > 0.0);
        REQUIRE(snapshot.preparedBytes <= reverb::audio::PreparedAudioFileSource::maximumPreparedBytes);
    }

    const auto retainedGeneration = source.snapshot().generation;
    REQUIRE_FALSE(source.loadFile(surroundWav.file(), error));
    REQUIRE(error.find("one or two channels") != std::string::npos);
    REQUIRE(source.snapshot().generation == retainedGeneration);

    const auto corrupt = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getNonexistentChildFile("reverb-playground-corrupt", ".wav", false);
    REQUIRE(corrupt.replaceWithText("not audio"));
    REQUIRE_FALSE(source.loadFile(corrupt, error));
    REQUIRE(error.find("Unsupported or corrupt") != std::string::npos);
    static_cast<void>(corrupt.deleteFile());

    REQUIRE(source.loadFile(monoWav.file(), error));
    source.play();
    constexpr std::array partitions { std::size_t { 64 } };
    const auto mono = render(source, 4'096, partitions);
    REQUIRE(allFinite(mono));
    for (std::size_t index = 0; index < 4'096; ++index)
        REQUIRE(mono[index] == mono[4'096 + index]);

    REQUIRE(source.loadFile(floatWav.file(), error));
    source.play();
    const auto stereo = render(source, 4'096, partitions);
    REQUIRE(allFinite(stereo));
    auto distinct = false;
    for (std::size_t index = 0; index < 4'096; ++index)
        distinct = distinct || std::abs(stereo[index] - stereo[4'096 + index]) > 1.0e-4F;
    REQUIRE(distinct);
}

TEST_CASE("Prepared audio transport is partition deterministic and obeys pause seek stop and loop")
{
    juce::WavAudioFormat wav;
    AudioFixture fixture(wav, ".wav", 2, 44'100.0, 44'100, 24);
    std::string error;
    reverb::audio::PreparedAudioFileSource first;
    reverb::audio::PreparedAudioFileSource second;
    first.prepare(48'000.0, 128);
    second.prepare(48'000.0, 128);
    REQUIRE(first.loadFile(fixture.file(), error));
    REQUIRE(second.loadFile(fixture.file(), error));
    first.play();
    second.play();
    constexpr std::array partitionsA { std::size_t { 64 } };
    constexpr std::array partitionsB { std::size_t { 17 }, std::size_t { 111 }, std::size_t { 29 } };
    const auto a = render(first, 8'000, partitionsA);
    const auto b = render(second, 8'000, partitionsB);
    REQUIRE(a == b);

    first.pause();
    const auto pausedCursor = first.snapshot().cursorSourceFrame;
    const auto paused = render(first, 512, partitionsA);
    REQUIRE(std::ranges::all_of(paused, [](const float value) { return value == 0.0F; }));
    REQUIRE(first.snapshot().cursorSourceFrame == pausedCursor);

    REQUIRE(first.seek(22'050, error));
    REQUIRE(first.snapshot().state == reverb::audio::AudioFileTransportState::paused);
    first.play();
    const auto sought = render(first, 512, partitionsA);
    REQUIRE(sought != std::vector<float>(sought.size(), 0.0F));

    REQUIRE_FALSE(first.setLoop(true, 100, 200, error));
    REQUIRE(error.find("at least 20 ms") != std::string::npos);
    REQUIRE(first.setLoop(true, 4'410, 8'820, error));
    first.play();
    const auto looped = render(first, 8'000, partitionsB);
    REQUIRE(allFinite(looped));
    const auto loopSnapshot = first.snapshot();
    REQUIRE(loopSnapshot.state == reverb::audio::AudioFileTransportState::playing);
    REQUIRE(loopSnapshot.cursorSourceFrame >= 4'410);
    REQUIRE(loopSnapshot.cursorSourceFrame < 8'820);

    first.stop();
    REQUIRE(first.snapshot().state == reverb::audio::AudioFileTransportState::ready);
    REQUIRE(first.snapshot().cursorSourceFrame == 0);
    const auto stopped = render(first, 256, partitionsA);
    REQUIRE(std::ranges::all_of(stopped, [](const float value) { return value == 0.0F; }));

    for (std::int64_t edit = 0; edit < 100; ++edit) {
        REQUIRE(first.seek((edit * 337) % 40'000, error));
        first.play();
        const auto edited = render(first, 64, partitionsB);
        REQUIRE(allFinite(edited));
        first.pause();
    }
    REQUIRE(first.snapshot().preparedBytes <= reverb::audio::PreparedAudioFileSource::maximumPreparedBytes);
}

TEST_CASE("Prepared audio source emits silence and monotonic diagnostics on underrun")
{
    juce::WavAudioFormat wav;
    AudioFixture fixture(wav, ".wav", 2, 48'000.0, 48'000, 16);
    reverb::audio::PreparedAudioFileSource source;
    source.prepare(48'000.0, 64);
    std::string error;
    REQUIRE(source.loadFile(fixture.file(), error));
    source.setWorkerStarvedForTesting(true);
    source.play();
    constexpr std::array partitions { std::size_t { 64 } };
    const auto first = render(source, 256, partitions);
    REQUIRE(std::ranges::all_of(first, [](const float value) { return value == 0.0F; }));
    const auto firstSnapshot = source.snapshot();
    REQUIRE(firstSnapshot.underrunEvents > 0);
    REQUIRE(firstSnapshot.underrunFrames == 256);

    const auto second = render(source, 128, partitions);
    REQUIRE(std::ranges::all_of(second, [](const float value) { return value == 0.0F; }));
    const auto secondSnapshot = source.snapshot();
    REQUIRE(secondSnapshot.underrunEvents > firstSnapshot.underrunEvents);
    REQUIRE(secondSnapshot.underrunFrames == 384);
    source.setWorkerStarvedForTesting(false);
}

TEST_CASE("Prepared audio source reaches tail reprepares safely and contains non-finite source data")
{
    juce::WavAudioFormat wav;
    AudioFixture shortFixture(wav, ".wav", 2, 48'000.0, 1'000, 32, true, 0.35, true);
    reverb::audio::PreparedAudioFileSource source;
    source.prepare(48'000.0, 128);
    std::string error;
    REQUIRE(source.loadFile(shortFixture.file(), error));
    source.play();
    constexpr std::array partitions { std::size_t { 31 }, std::size_t { 97 } };
    const auto complete = render(source, 2'000, partitions);
    REQUIRE(allFinite(complete));
    REQUIRE(source.snapshot().state == reverb::audio::AudioFileTransportState::tail);
    REQUIRE(source.snapshot().sanitizedSourceSamples > 0);

    source.play();
    static_cast<void>(render(source, 256, partitions));
    const auto cursorBeforeRateChange = source.snapshot().cursorSourceFrame;
    source.prepare(96'000.0, 256);
    const auto reprepared = source.snapshot();
    REQUIRE(reprepared.prepared);
    REQUIRE(reprepared.outputSampleRate == 96'000.0);
    REQUIRE(reprepared.state == reverb::audio::AudioFileTransportState::paused);
    REQUIRE(std::abs(reprepared.cursorSourceFrame - cursorBeforeRateChange) <= 1);
    REQUIRE(reprepared.preparedBytes <= reverb::audio::PreparedAudioFileSource::maximumPreparedBytes);

    source.prepare(1'234'567.8, 256);
    const auto validatorRate = source.snapshot();
    REQUIRE(validatorRate.prepared);
    REQUIRE(validatorRate.outputSampleRate == Catch::Approx(1'234'567.8));
    REQUIRE(validatorRate.preparedBytes <= reverb::audio::PreparedAudioFileSource::maximumPreparedBytes);

    source.reset();
    REQUIRE(source.snapshot().state == reverb::audio::AudioFileTransportState::ready);
    REQUIRE(source.snapshot().cursorSourceFrame == 0);
    REQUIRE_THROWS(source.prepare(48'000.0, reverb::audio::PreparedAudioFileSource::maximumBlockSize + 1));
}

TEST_CASE("Plugin routes prepared file audio through the graph without persisting transport state")
{
    juce::WavAudioFormat wav;
    AudioFixture fixture(wav, ".wav", 2, 48'000.0, 24'000, 16);
    ReverbPlaygroundProcessor processor;
    processor.prepareToPlay(48'000.0, 128);
    processor.setMasterGain(1.0F);
    std::string error;
    REQUIRE(processor.loadAudioFile(fixture.file(), error));
    processor.playAudioFile();
    REQUIRE(processor.auditionSourceMode() == reverb::audio::AuditionSourceMode::audioFile);

    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer midi;
    auto heardWetOutput = false;
    for (auto block = 0; block < 40; ++block) {
        buffer.clear();
        processor.processBlock(buffer, midi);
        heardWetOutput = heardWetOutput || buffer.getMagnitude(0, 0, buffer.getNumSamples()) > 1.0e-6F
            || buffer.getMagnitude(1, 0, buffer.getNumSamples()) > 1.0e-6F;
    }
    REQUIRE(heardWetOutput);

    const auto transport = nlohmann::json::parse(processor.audioFileTransportJson().toStdString());
    REQUIRE(transport.at("sourceMode") == "audio-file");
    REQUIRE(transport.at("fileName") == fixture.file().getFileName().toStdString());
    REQUIRE(transport.at("underrunEvents").get<std::uint64_t>() == 0);

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    const auto restored = juce::ValueTree::readFromData(state.getData(), state.getSize());
    REQUIRE(restored.isValid());
    REQUIRE_FALSE(restored.hasProperty("audioFilePath"));
    REQUIRE_FALSE(restored.hasProperty("audioFileTransport"));
    REQUIRE_FALSE(restored.hasProperty("auditionSourceMode"));
    REQUIRE_FALSE(restored.toXmlString().contains(fixture.file().getFullPathName()));
}

TEST_CASE("Audio-file input remains behind the plugin numerical safety latch")
{
    juce::WavAudioFormat wav;
    AudioFixture unsafe(wav, ".wav", 2, 48'000.0, 12'000, 32, true, 100.0);
    ReverbPlaygroundProcessor processor;
    processor.prepareToPlay(48'000.0, 128);
    processor.setMasterGain(1.0F);
    std::string error;
    REQUIRE(processor.loadAudioFile(unsafe.file(), error));
    processor.playAudioFile();

    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer midi;
    for (auto block = 0; block < 100 && !processor.isSafetyLatched(); ++block) {
        buffer.clear();
        processor.processBlock(buffer, midi);
    }
    REQUIRE(processor.isSafetyLatched());
    REQUIRE(buffer.getMagnitude(0, 0, buffer.getNumSamples()) == 0.0F);
    REQUIRE(buffer.getMagnitude(1, 0, buffer.getNumSamples()) == 0.0F);
}

TEST_CASE("Audition source switching is ten-millisecond click-safe and dry bypass is explicit")
{
    juce::WavAudioFormat wav;
    AudioFixture fixture(wav, ".wav", 2, 48'000.0, 12'000, 16);
    ReverbPlaygroundProcessor processor;
    processor.prepareToPlay(48'000.0, 120);
    processor.setMasterGain(1.0F);
    processor.setProcessedAudition(false);
    REQUIRE_FALSE(processor.isProcessedAudition());
    std::string error;
    REQUIRE(processor.loadAudioFile(fixture.file(), error));
    processor.playAudioFile();

    juce::AudioBuffer<float> buffer(2, 120);
    juce::MidiBuffer midi;
    for (auto block = 0; block < 5; ++block) {
        buffer.clear();
        processor.processBlock(buffer, midi);
    }

    processor.setAuditionSourceMode(reverb::audio::AuditionSourceMode::liveInput);
    auto previous = 0.0F;
    auto maximumStep = 0.0F;
    auto havePrevious = false;
    for (auto block = 0; block < 4; ++block) {
        buffer.clear();
        for (auto channel = 0; channel < 2; ++channel)
            for (auto frame = 0; frame < buffer.getNumSamples(); ++frame)
                buffer.setSample(channel, frame, 0.25F);
        processor.processBlock(buffer, midi);
        for (auto frame = 0; frame < buffer.getNumSamples(); ++frame) {
            if (havePrevious)
                maximumStep = std::max(maximumStep, std::abs(buffer.getSample(0, frame) - previous));
            previous = buffer.getSample(0, frame);
            havePrevious = true;
        }
    }
    REQUIRE(maximumStep < 0.26F);
    REQUIRE(buffer.getSample(0, 119) == Catch::Approx(0.25F).margin(1.0e-6F));
    REQUIRE(nlohmann::json::parse(processor.audioFileTransportJson().toStdString()).at("state") == "paused");

    processor.setAuditionSourceMode(reverb::audio::AuditionSourceMode::audioFile);
    for (auto block = 0; block < 5; ++block) {
        buffer.clear();
        processor.processBlock(buffer, midi);
    }
    REQUIRE(nlohmann::json::parse(processor.audioFileTransportJson().toStdString()).at("state") == "playing");

    processor.triggerImpulse();
    for (auto block = 0; block < 4; ++block) {
        buffer.clear();
        for (auto channel = 0; channel < 2; ++channel)
            for (auto frame = 0; frame < buffer.getNumSamples(); ++frame)
                buffer.setSample(channel, frame, 0.25F);
        processor.processBlock(buffer, midi);
    }
    buffer.clear();
    processor.processBlock(buffer, midi);
    REQUIRE(buffer.getMagnitude(0, 0, buffer.getNumSamples()) == 0.0F);
    REQUIRE(processor.auditionSourceMode() == reverb::audio::AuditionSourceMode::testImpulse);
}

TEST_CASE("Plugin exports the loaded source without persisting export state")
{
    juce::WavAudioFormat wav;
    AudioFixture fixture(wav, ".wav", 2, 44'100.0, 4'410, 24);
    const auto destination = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                 .getNonexistentChildFile("reverb-plugin-export-" + juce::Uuid().toString(), ".wav", false);
    ReverbPlaygroundProcessor processor;
    processor.prepareToPlay(48'000.0, 128);
    std::string error;
    REQUIRE(processor.loadAudioFile(fixture.file(), error));
    REQUIRE(processor.startProcessedFileExport(
        destination, reverb::render::FileExportMode::wetOnly, false, error));

    nlohmann::json status;
    for (auto attempt = 0; attempt < 1'000; ++attempt) {
        status = nlohmann::json::parse(processor.processedFileExportJson().toStdString());
        if (status.at("state") != "rendering") break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(status.at("state") == "complete");
    REQUIRE(status.at("sampleRate") == 48'000);
    REQUIRE(status.at("bitsPerSample") == 24);
    REQUIRE(destination.existsAsFile());

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    const auto restored = juce::ValueTree::readFromData(state.getData(), state.getSize());
    REQUIRE_FALSE(restored.toXmlString().contains(destination.getFullPathName()));
    REQUIRE_FALSE(restored.hasProperty("fileExport"));
    static_cast<void>(destination.deleteFile());
}

TEST_CASE("Emergency Mute refuses export and remains an explicit latch")
{
    juce::WavAudioFormat wav;
    AudioFixture fixture(wav, ".wav", 2, 48'000.0, 480, 24);
    const auto destination = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                 .getNonexistentChildFile("reverb-muted-export-" + juce::Uuid().toString(), ".wav", false);
    ReverbPlaygroundProcessor processor;
    processor.prepareToPlay(48'000.0, 128);
    std::string error;
    REQUIRE(processor.loadAudioFile(fixture.file(), error));
    processor.setEmergencyMuted(true);
    REQUIRE_FALSE(processor.startProcessedFileExport(
        destination, reverb::render::FileExportMode::wetOnly, false, error));
    CHECK(error.find("Emergency Mute") != std::string::npos);
    CHECK(processor.isEmergencyMuted());
    CHECK_FALSE(destination.existsAsFile());
}

TEST_CASE("Standalone transport survives graph state reload but not processor restart")
{
    juce::WavAudioFormat wav;
    AudioFixture fixture(wav, ".wav", 2, 48'000.0, 24'000, 24);
    juce::MemoryBlock savedState;
    std::int64_t cursorBeforeReload = 0;
    {
        ReverbPlaygroundProcessor processor;
        processor.prepareToPlay(48'000.0, 128);
        std::string error;
        REQUIRE(processor.loadAudioFile(fixture.file(), error));
        processor.playAudioFile();
        juce::AudioBuffer<float> buffer(2, 128);
        juce::MidiBuffer midi;
        for (auto block = 0; block < 5; ++block) {
            buffer.clear();
            processor.processBlock(buffer, midi);
        }
        auto before = nlohmann::json::parse(processor.audioFileTransportJson().toStdString());
        cursorBeforeReload = before.at("cursorSourceFrame");
        REQUIRE(cursorBeforeReload > 0);
        processor.getStateInformation(savedState);
        processor.setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
        const auto after = nlohmann::json::parse(processor.audioFileTransportJson().toStdString());
        REQUIRE(after.at("state") == "playing");
        REQUIRE(after.at("cursorSourceFrame").get<std::int64_t>() >= cursorBeforeReload);
    }

    ReverbPlaygroundProcessor restarted;
    restarted.prepareToPlay(48'000.0, 128);
    restarted.setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
    const auto cleanTransport = nlohmann::json::parse(restarted.audioFileTransportJson().toStdString());
    REQUIRE(cleanTransport.at("sourceMode") == "live-input");
    REQUIRE(cleanTransport.at("state") == "empty");
    REQUIRE(cleanTransport.at("fileName") == "");
}
