#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/Delay.h>
#include <reverb/dsp/NumericalSafetyGuard.h>
#include <reverb/dsp/PitchShift.h>
#include <reverb/graph/AcyclicRuntime.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <numbers>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace {

double tonePower(const std::span<const float> samples, const double sampleRate, const double frequency)
{
    auto real = 0.0;
    auto imaginary = 0.0;
    const auto step = 2.0 * std::numbers::pi * frequency / sampleRate;
    for (std::size_t frame = 0; frame < samples.size(); ++frame) {
        const auto window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi
            * static_cast<double>(frame) / static_cast<double>(samples.size() - 1));
        const auto phase = step * static_cast<double>(frame);
        real += static_cast<double>(samples[frame]) * window * std::cos(phase);
        imaginary -= static_cast<double>(samples[frame]) * window * std::sin(phase);
    }
    return real * real + imaginary * imaginary;
}

double bandPeakPower(
    const std::span<const float> samples,
    const double sampleRate,
    const double centerFrequency,
    const double relativeHalfWidth = 0.04)
{
    auto peak = 0.0;
    constexpr auto steps = 80;
    for (auto index = 0; index <= steps; ++index) {
        const auto position = -1.0 + 2.0 * static_cast<double>(index) / steps;
        peak = std::max(peak, tonePower(samples, sampleRate,
            centerFrequency * (1.0 + relativeHalfWidth * position)));
    }
    return peak;
}

std::vector<float> renderPitch(
    const double sampleRate,
    const std::span<const double> frequencies,
    const double semitones,
    const reverb::dsp::pitch_shift::GrainDirection direction)
{
    reverb::dsp::PitchShift shifter;
    shifter.prepare(sampleRate, { semitones, 60.0, 0.5, direction });
    const auto frames = shifter.latencySamples() + static_cast<std::size_t>(sampleRate);
    std::vector<float> output(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (const auto frequency : frequencies)
            output[frame] += static_cast<float>(0.25 * std::sin(
                2.0 * std::numbers::pi * frequency * static_cast<double>(frame) / sampleRate));
    }
    shifter.process(output);
    return output;
}

using namespace reverb::graph;
Port audioInput(std::string id = "in") { return { std::move(id), SignalType::audio, PortDirection::input }; }
Port audioOutput(std::string id = "out") { return { std::move(id), SignalType::audio, PortDirection::output }; }
Port controlInput(std::string id) { return { std::move(id), SignalType::control, PortDirection::input }; }
Node stereoInput() { return { "input", "stereo-input", { audioOutput("out-l"), audioOutput("out-r") }, {} }; }
Node stereoOutput() { return { "output", "stereo-output", { audioInput("in-l"), audioInput("in-r") }, {} }; }
Node sumNode() { return { "sum", "sum", { audioInput("in-a"), audioInput("in-b"), audioOutput() }, {} }; }
Node delayNode() { return { "delay", "delay", { audioInput(), audioOutput() }, { { "delay", 11.0, "milliseconds" } } }; }
Node gainNode(const double value) { return { "feedback", "gain", { audioInput(), audioOutput() }, { { "gain", value, "linear" } } }; }
Node pitchNode(const reverb::dsp::pitch_shift::GrainDirection direction)
{
    return { "pitch", "pitch-shift", {
        audioInput(), controlInput("semitones-mod"), controlInput("grain-mod"),
        controlInput("overlap-mod"), audioOutput(),
    }, {
        { "semitones", 12.0, "semitones" }, { "grain", 60.0, "milliseconds" },
        { "overlap", 0.5, "normalized" },
        { "direction", direction == reverb::dsp::pitch_shift::GrainDirection::reverse ? 1.0 : 0.0, "direction" },
    } };
}
Connection cable(std::string id, std::string fromNode, std::string fromPort, std::string toNode, std::string toPort)
{
    return { std::move(id), { std::move(fromNode), std::move(fromPort) },
        { std::move(toNode), std::move(toPort) } };
}

GraphDocument pitchFeedbackGraph(
    const reverb::dsp::pitch_shift::GrainDirection direction,
    const double feedback = 0.35)
{
    GraphDocument graph;
    graph.nodes = { stereoInput(), sumNode(), pitchNode(direction), gainNode(feedback), delayNode(), stereoOutput() };
    graph.connections = {
        cable("input-sum", "input", "out-l", "sum", "in-a"),
        cable("delay-sum", "delay", "out", "sum", "in-b"),
        cable("sum-pitch", "sum", "out", "pitch", "in"),
        cable("pitch-gain", "pitch", "out", "feedback", "in"),
        cable("gain-delay", "feedback", "out", "delay", "in"),
        cable("delay-left", "delay", "out", "output", "in-l"),
        cable("input-right", "input", "out-r", "output", "in-r"),
    };
    return graph;
}

} // namespace

TEST_CASE("Pitch Shift moves representative tones and chords to musical-ratio bands")
{
    using reverb::dsp::pitch_shift::GrainDirection;
    constexpr std::array chord { 220.0, 277.182631, 329.627557 };
    for (const auto sampleRate : reverb::dsp::pitch_shift::qualificationSampleRates) {
        const auto output = renderPitch(sampleRate, chord, 12.0, GrainDirection::forward);
        const auto latency = reverb::dsp::pitch_shift::reportedLatencySamples(sampleRate);
        const auto analysed = std::span<const float>(output).subspan(
            latency + static_cast<std::size_t>(0.2 * sampleRate),
            static_cast<std::size_t>(0.6 * sampleRate));
        for (const auto inputFrequency : chord) {
            CAPTURE(sampleRate, inputFrequency);
            const auto targetPower = bandPeakPower(analysed, sampleRate, inputFrequency * 2.0);
            const auto unshiftedPower = bandPeakPower(analysed, sampleRate, inputFrequency);
            REQUIRE(targetPower > unshiftedPower * 4.0);
        }
    }
}

TEST_CASE("Musical Pitch Shift is unlike fixed-hertz translation and moving-Delay Doppler")
{
    using reverb::dsp::pitch_shift::GrainDirection;
    constexpr auto sampleRate = 48'000.0;
    constexpr std::array lower { 330.0 };
    constexpr std::array upper { 550.0 };
    const auto shiftedLower = renderPitch(sampleRate, lower, 12.0, GrainDirection::forward);
    const auto shiftedUpper = renderPitch(sampleRate, upper, 12.0, GrainDirection::forward);
    const auto latency = reverb::dsp::pitch_shift::reportedLatencySamples(sampleRate);
    const auto range = [latency](const std::vector<float>& samples) {
        return std::span<const float>(samples).subspan(latency + 9'600, 28'800);
    };
    REQUIRE(bandPeakPower(range(shiftedLower), sampleRate, 660.0)
        > bandPeakPower(range(shiftedLower), sampleRate, 880.0) * 100.0);
    REQUIRE(bandPeakPower(range(shiftedUpper), sampleRate, 1'100.0)
        > bandPeakPower(range(shiftedUpper), sampleRate, 880.0) * 100.0);
    // The two shifts are +330 and +550 Hz, not one fixed translation.

    constexpr std::size_t frames = 48'000;
    std::vector<float> doppler(frames), storage(481);
    std::vector<double> modulation(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        doppler[frame] = static_cast<float>(std::sin(2.0 * std::numbers::pi * 440.0
            * static_cast<double>(frame) / sampleRate));
        modulation[frame] = 5.0 + std::sin(2.0 * std::numbers::pi * 3.0
            * static_cast<double>(frame) / sampleRate);
    }
    reverb::dsp::Delay delay;
    delay.prepareModulated(sampleRate, 5.0, 10.0, storage);
    delay.processModulated(doppler, modulation);
    const auto dopplerRange = std::span<const float>(doppler).subspan(9'600, 28'800);
    REQUIRE(bandPeakPower(range(renderPitch(sampleRate, std::array { 440.0 }, 12.0, GrainDirection::forward)), sampleRate, 880.0)
        > bandPeakPower(dopplerRange, sampleRate, 880.0) * 100.0);
    REQUIRE(bandPeakPower(dopplerRange, sampleRate, 440.0)
        > bandPeakPower(dopplerRange, sampleRate, 880.0) * 100.0);
}

TEST_CASE("Conservative delayed Pitch Shift feedback remains finite for both grain directions")
{
    using reverb::dsp::pitch_shift::GrainDirection;
    constexpr std::size_t blockSize = 128;
    for (const auto sampleRate : reverb::dsp::pitch_shift::qualificationSampleRates) {
        for (const auto direction : { GrainDirection::forward, GrainDirection::reverse }) {
            auto compiled = compileFeedbackGraph(pitchFeedbackGraph(direction), sampleRate, blockSize);
            CAPTURE(sampleRate, direction, compiled.errors);
            REQUIRE(compiled.valid());
            REQUIRE(compiled.feedbackComponents == std::vector<std::vector<std::string>> {
                { "delay", "feedback", "pitch", "sum" },
            });
            std::mt19937 generator(0x50495443U);
            std::uniform_real_distribution<float> noise(-0.1F, 0.1F);
            std::array<float, blockSize> input {}, silence {}, left {}, right {};
            const auto blocks = static_cast<std::size_t>(std::ceil(sampleRate * 2.0 / blockSize));
            auto peak = 0.0F;
            for (std::size_t block = 0; block < blocks; ++block) {
                for (auto& sample : input) sample = block == 0 ? 0.0F : noise(generator);
                if (block == 0) input[0] = 0.1F;
                compiled.runtime->process(input, silence, left, right);
                for (const auto sample : left) {
                    REQUIRE(std::isfinite(sample));
                    peak = std::max(peak, std::abs(sample));
                }
            }
            REQUIRE(peak > 0.0F);
            REQUIRE(peak < 1.0F);
        }
    }
}

TEST_CASE("Pitch Shift feedback topology crossfades and recovers from a latched safety mute")
{
    using reverb::dsp::pitch_shift::GrainDirection;
    constexpr auto sampleRate = 48'000.0;
    constexpr std::size_t blockSize = 64;
    AcyclicRuntimeHost host;
    REQUIRE(host.compileFeedbackAndPublish(pitchFeedbackGraph(GrainDirection::forward), sampleRate, blockSize).valid());
    std::array<float, blockSize> input {}, silence {}, left {}, right {};
    input.fill(0.05F);
    host.process(input, silence, left, right);

    REQUIRE(host.compileFeedbackAndPublish(pitchFeedbackGraph(GrainDirection::reverse), sampleRate, blockSize).valid());
    host.process(input, silence, left, right);
    auto publication = host.publicationSnapshot();
    REQUIRE(publication.crossfadeFromRevision == 1);
    REQUIRE(publication.crossfadeTotalSamples == 480);
    REQUIRE(std::ranges::all_of(left, [](const auto sample) { return std::isfinite(sample); }));

    reverb::dsp::NumericalSafetyGuard guard { 0.01F, 0.005F, 1.0 };
    guard.prepare(sampleRate);
    reverb::dsp::SafetyStatus status;
    for (std::size_t block = 0; block < 1'000 && status.violation == reverb::dsp::SafetyViolation::none; ++block) {
        host.process(input, silence, left, right);
        status = guard.inspectAndMute(left);
    }
    REQUIRE(status.violation == reverb::dsp::SafetyViolation::runawayLevel);
    REQUIRE(guard.isMuted());
    REQUIRE(std::ranges::all_of(left, [](const auto sample) { return sample == 0.0F; }));

    host.resetActiveRuntimes();
    guard.reset();
    input.fill(0.0F);
    host.process(input, silence, left, right);
    REQUIRE_FALSE(guard.isMuted());
    REQUIRE(std::ranges::all_of(left, [](const auto sample) { return sample == 0.0F; }));
    publication = host.publicationSnapshot();
    REQUIRE(publication.completedCrossfades == 1);
}

TEST_CASE("Checked Pitch Shift validation records rate quality latency storage CPU and aliasing")
{
    std::ifstream stream(std::string(REVERB_MEASUREMENTS_DIR) + "/pitch-shift-validation-v1.json");
    REQUIRE(stream.good());
    const auto report = nlohmann::json::parse(
        std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()));
    REQUIRE(report.at("formatVersion") == 1);
    REQUIRE(report.at("measurement") == "pitch-shift-validation");
    REQUIRE(report.at("quality").at("id") == "dual-grain-linear-v1");
    REQUIRE(report.at("quality").at("interpolation") == "linear");
    REQUIRE(report.at("rates").size() == reverb::dsp::pitch_shift::qualificationSampleRates.size());
    for (std::size_t index = 0; index < report.at("rates").size(); ++index) {
        const auto& rate = report.at("rates").at(index);
        const auto sampleRate = reverb::dsp::pitch_shift::qualificationSampleRates[index];
        CAPTURE(sampleRate);
        REQUIRE(rate.at("sampleRate") == sampleRate);
        REQUIRE(rate.at("latencySamples") == reverb::dsp::pitch_shift::reportedLatencySamples(sampleRate));
        REQUIRE(rate.at("storageBytes") == reverb::dsp::pitch_shift::preparedStorageBytes(sampleRate));
        REQUIRE(rate.at("directions").size() == 2);
        for (const auto& direction : rate.at("directions")) {
            REQUIRE(std::abs(direction.at("measuredOctaveCents").get<double>()) <= 15.0);
            REQUIRE(std::isfinite(direction.at("foldedAliasDbfs").get<double>()));
            REQUIRE(direction.at("aliasRelativeToReferenceDb").get<double>() <= 0.0);
            REQUIRE(direction.at("measuredCpuRealtimeLoadPercent").get<double>() > 0.0);
            REQUIRE(direction.at("measuredCpuRealtimeLoadPercent").get<double>() < 10.0);
            REQUIRE(direction.at("processedFrames") == static_cast<std::uint64_t>(sampleRate));
        }
    }
}
