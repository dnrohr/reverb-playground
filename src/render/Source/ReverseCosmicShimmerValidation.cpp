#include <reverb/render/ReverseCosmicShimmerValidation.h>

#include <reverb/dsp/PitchShiftContract.h>
#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/ReverseCosmicShimmerGraph.h>
#include <reverb/render/WavWriter.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <span>
#include <vector>

namespace reverb::render {
namespace {

constexpr std::size_t blockSize = 256;
constexpr double fixtureDurationSeconds = 4.0;
enum class Fixture { impulse, chord, noise };
struct StereoRender final { std::vector<float> left; std::vector<float> right; };

StereoRender renderFixture(const double sampleRate, const Fixture fixture)
{
    auto compiled = reverb::graph::compileFeedbackGraph(
        reverb::graph::makeReverseCosmicShimmerGraph(), sampleRate, blockSize);
    if (!compiled.valid()) return {};
    const auto frames = static_cast<std::size_t>(std::llround(sampleRate * fixtureDurationSeconds));
    StereoRender render { std::vector<float>(frames), std::vector<float>(frames) };
    std::array<float, blockSize> input {}, silence {}, left {}, right {};
    std::uint32_t noiseState = 0x52435653U;
    constexpr std::array chord { 220.0, 277.18, 329.63 };
    for (std::size_t offset = 0; offset < frames; offset += blockSize) {
        const auto count = std::min(blockSize, frames - offset);
        input.fill(0.0F);
        for (std::size_t frame = 0; frame < count; ++frame) {
            const auto absolute = offset + frame;
            if (fixture == Fixture::impulse && absolute == 0) input[frame] = 0.1F;
            if (fixture == Fixture::chord && absolute < static_cast<std::size_t>(0.12 * sampleRate)) {
                auto sample = 0.0;
                for (const auto frequency : chord) sample += std::sin(
                    2.0 * std::numbers::pi * frequency * static_cast<double>(absolute) / sampleRate);
                input[frame] = static_cast<float>(0.018 * sample);
            }
            if (fixture == Fixture::noise && absolute < static_cast<std::size_t>(0.15 * sampleRate)) {
                noiseState = noiseState * 1'664'525U + 1'013'904'223U;
                input[frame] = static_cast<float>(
                    (static_cast<double>(noiseState) / 4'294'967'295.0 - 0.5) * 0.06);
            }
        }
        compiled.runtime->process(std::span(input).first(count), std::span(silence).first(count),
            std::span(left).first(count), std::span(right).first(count));
        std::ranges::copy(std::span(left).first(count), render.left.begin() + static_cast<std::ptrdiff_t>(offset));
        std::ranges::copy(std::span(right).first(count), render.right.begin() + static_cast<std::ptrdiff_t>(offset));
    }
    return render;
}

double energy(const StereoRender& render, const double sampleRate, const double start, const double duration)
{
    const auto first = static_cast<std::size_t>(start * sampleRate);
    const auto count = static_cast<std::size_t>(duration * sampleRate);
    auto result = 0.0;
    for (std::size_t frame = first; frame < std::min(first + count, render.left.size()); ++frame)
        result += static_cast<double>(render.left[frame]) * render.left[frame]
            + static_cast<double>(render.right[frame]) * render.right[frame];
    return result;
}

double amplitude(const std::span<const float> samples, const double sampleRate, const double frequency)
{
    auto real = 0.0;
    auto imaginary = 0.0;
    auto windowSum = 0.0;
    for (std::size_t frame = 0; frame < samples.size(); ++frame) {
        const auto window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi
            * static_cast<double>(frame) / static_cast<double>(samples.size() - 1));
        const auto phase = 2.0 * std::numbers::pi * frequency * static_cast<double>(frame) / sampleRate;
        real += samples[frame] * window * std::cos(phase);
        imaginary -= samples[frame] * window * std::sin(phase);
        windowSum += window;
    }
    return windowSum > 0.0 ? 2.0 * std::hypot(real, imaginary) / windowSum : 0.0;
}

double chordBandAmplitude(const StereoRender& render, const double sampleRate,
    const double start, const double seconds, const std::span<const double> targets)
{
    const auto first = static_cast<std::size_t>(start * sampleRate);
    const auto count = static_cast<std::size_t>(seconds * sampleRate);
    std::vector<float> mono(count);
    for (std::size_t frame = 0; frame < count; ++frame)
        mono[frame] = 0.5F * (render.left[first + frame] + render.right[first + frame]);
    auto squared = 0.0;
    for (const auto target : targets) {
        const auto value = amplitude(mono, sampleRate, target);
        squared += value * value;
    }
    return std::sqrt(squared);
}

double db(const double numerator, const double denominator = 1.0)
{
    return 20.0 * std::log10(std::max(numerator, 1.0e-15) / std::max(denominator, 1.0e-15));
}

double correlation(const StereoRender& render, const double sampleRate, const double start)
{
    const auto first = static_cast<std::size_t>(start * sampleRate);
    auto cross = 0.0;
    auto leftEnergy = 0.0;
    auto rightEnergy = 0.0;
    for (std::size_t frame = first; frame < render.left.size(); ++frame) {
        cross += static_cast<double>(render.left[frame]) * render.right[frame];
        leftEnergy += static_cast<double>(render.left[frame]) * render.left[frame];
        rightEnergy += static_cast<double>(render.right[frame]) * render.right[frame];
    }
    return cross / std::sqrt(std::max(leftEnergy * rightEnergy, 1.0e-30));
}

double monoCompatibility(const StereoRender& render, const double sampleRate, const double start)
{
    const auto first = static_cast<std::size_t>(start * sampleRate);
    auto stereoEnergy = 0.0;
    auto monoEnergy = 0.0;
    for (std::size_t frame = first; frame < render.left.size(); ++frame) {
        stereoEnergy += static_cast<double>(render.left[frame]) * render.left[frame]
            + static_cast<double>(render.right[frame]) * render.right[frame];
        const auto mono = 0.5 * (static_cast<double>(render.left[frame]) + render.right[frame]);
        monoEnergy += mono * mono;
    }
    return 2.0 * monoEnergy / std::max(stereoEnergy, 1.0e-30);
}

std::size_t onset(const StereoRender& render)
{
    for (std::size_t frame = 0; frame < render.left.size(); ++frame) {
        if (std::abs(render.left[frame]) > 1.0e-8F || std::abs(render.right[frame]) > 1.0e-8F)
            return frame;
    }
    return render.left.size();
}

double peakTime(const StereoRender& render, const double sampleRate)
{
    auto peak = 0.0F;
    auto frame = std::size_t {};
    for (std::size_t index = 0; index < render.left.size(); ++index) {
        const auto value = std::max(std::abs(render.left[index]), std::abs(render.right[index]));
        if (value > peak) { peak = value; frame = index; }
    }
    return 1'000.0 * static_cast<double>(frame) / sampleRate;
}

double peak(const StereoRender& render)
{
    auto value = 0.0;
    for (std::size_t frame = 0; frame < render.left.size(); ++frame)
        value = std::max({ value, std::abs(static_cast<double>(render.left[frame])),
            std::abs(static_cast<double>(render.right[frame])) });
    return value;
}

bool finite(const StereoRender& render)
{
    return std::ranges::all_of(render.left, [](const float sample) { return std::isfinite(sample); })
        && std::ranges::all_of(render.right, [](const float sample) { return std::isfinite(sample); });
}

std::uint64_t hash(const StereoRender& render)
{
    auto value = std::uint64_t { 1'469'598'103'934'665'603ULL };
    auto mix = [&](const float sample) {
        const auto quantized = static_cast<std::int16_t>(std::lrint(std::clamp(sample, -1.0F, 1.0F) * 32'767.0F));
        for (const auto byte : { static_cast<std::uint8_t>(quantized & 0xff),
                 static_cast<std::uint8_t>((static_cast<std::uint16_t>(quantized) >> 8) & 0xff) }) {
            value ^= byte;
            value *= 1'099'511'628'211ULL;
        }
    };
    for (std::size_t frame = 0; frame < render.left.size(); ++frame) {
        mix(render.left[frame]); mix(render.right[frame]);
    }
    return value;
}

std::string rateLabel(const double sampleRate)
{
    if (std::abs(sampleRate - 44'100.0) < 0.5) return "44k1";
    return std::to_string(static_cast<int>(std::llround(sampleRate / 1'000.0))) + "k";
}

} // namespace

ReverseCosmicShimmerValidationReport measureReverseCosmicShimmerValidation()
{
    constexpr std::array fundamentals { 220.0, 277.18, 329.63 };
    constexpr std::array octaves { 440.0, 554.36, 659.26 };
    ReverseCosmicShimmerValidationReport report { fixtureDurationSeconds, {} };
    for (const auto sampleRate : reverb::dsp::pitch_shift::qualificationSampleRates) {
        const auto impulse = renderFixture(sampleRate, Fixture::impulse);
        const auto chord = renderFixture(sampleRate, Fixture::chord);
        const auto noise = renderFixture(sampleRate, Fixture::noise);
        const auto onsetFrame = onset(impulse);
        const auto earlyFundamental = chordBandAmplitude(chord, sampleRate, 0.30, 0.30, fundamentals);
        const auto earlyChord = chordBandAmplitude(chord, sampleRate, 0.30, 0.30, octaves);
        const auto lateFundamental = chordBandAmplitude(chord, sampleRate, 1.30, 0.40, fundamentals);
        const auto lateChord = chordBandAmplitude(chord, sampleRate, 1.30, 0.40, octaves);
        const auto earlyRatio = db(earlyChord, earlyFundamental);
        const auto lateRatio = db(lateChord, lateFundamental);
        report.rates.push_back({
            sampleRate, onsetFrame, 1'000.0 * static_cast<double>(onsetFrame) / sampleRate,
            peakTime(impulse, sampleRate), energy(impulse, sampleRate, 0.20, 0.25),
            energy(impulse, sampleRate, 0.65, 0.45),
            db(std::sqrt(energy(impulse, sampleRate, 3.40, 0.50)),
                std::sqrt(energy(impulse, sampleRate, 2.00, 0.50))),
            db(earlyChord), db(lateChord), earlyRatio, lateRatio, lateRatio - earlyRatio,
            correlation(noise, sampleRate, 0.80), monoCompatibility(noise, sampleRate, 0.80),
            peak(impulse), peak(chord), peak(noise),
            finite(impulse) && finite(chord) && finite(noise), onsetFrame > 0,
            hash(impulse), hash(chord), hash(noise),
        });
    }
    return report;
}

std::string writeReverseCosmicShimmerValidationJson(
    const ReverseCosmicShimmerValidationReport& report)
{
    auto rates = nlohmann::ordered_json::array();
    for (const auto& rate : report.rates) {
        rates.push_back({
            { "sampleRate", rate.sampleRate }, { "onsetFrame", rate.onsetFrame },
            { "onsetMilliseconds", rate.onsetMilliseconds }, { "peakTimeMilliseconds", rate.peakTimeMilliseconds },
            { "earlyImpulseEnergy", rate.earlyImpulseEnergy }, { "lateImpulseEnergy", rate.lateImpulseEnergy },
            { "lateToEarlyImpulseEnergyDb", db(std::sqrt(rate.lateImpulseEnergy), std::sqrt(rate.earlyImpulseEnergy)) },
            { "finalToMidDecayDb", rate.finalToMidDecayDb },
            { "earlyChordOctaveDbfs", rate.earlyChordOctaveDbfs },
            { "lateChordOctaveDbfs", rate.lateChordOctaveDbfs },
            { "earlyOctaveToFundamentalDb", rate.earlyOctaveToFundamentalDb },
            { "lateOctaveToFundamentalDb", rate.lateOctaveToFundamentalDb },
            { "octaveGrowthDb", rate.octaveGrowthDb },
            { "stereoCorrelation", rate.stereoCorrelation }, { "monoCompatibility", rate.monoCompatibility },
            { "peaks", { { "impulse", rate.impulsePeak }, { "chord", rate.chordPeak }, { "boundedNoise", rate.noisePeak } } },
            { "finite", rate.finite }, { "causal", rate.causal },
            { "fixtures", {
                { "impulse", { { "file", "reverse-cosmic-shimmer-" + rateLabel(rate.sampleRate) + "-impulse.wav" }, { "pcm16Fnv1a", rate.impulseHash } } },
                { "chord", { { "file", "reverse-cosmic-shimmer-" + rateLabel(rate.sampleRate) + "-chord.wav" }, { "pcm16Fnv1a", rate.chordHash } } },
                { "boundedNoise", { { "file", "reverse-cosmic-shimmer-" + rateLabel(rate.sampleRate) + "-noise.wav" }, { "pcm16Fnv1a", rate.noiseHash } } },
            } },
        });
    }
    return nlohmann::ordered_json {
        { "formatVersion", 1 }, { "measurement", "reverse-cosmic-shimmer-validation" },
        { "durationSeconds", report.durationSeconds },
        { "input", {
            { "impulsePeak", 0.1 }, { "chordFrequenciesHertz", { 220.0, 277.18, 329.63 } },
            { "chordDurationSeconds", 0.12 }, { "noiseDurationSeconds", 0.15 },
            { "noiseGenerator", "LCG seed 0x52435653" },
        } },
        { "windows", {
            { "earlyImpulseSeconds", { 0.20, 0.45 } }, { "lateImpulseSeconds", { 0.65, 1.10 } },
            { "midDecaySeconds", { 2.00, 2.50 } }, { "finalDecaySeconds", { 3.40, 3.90 } },
            { "earlyChordSeconds", { 0.30, 0.60 } }, { "lateChordSeconds", { 1.30, 1.70 } },
            { "stereoStartSeconds", 0.80 }, { "window", "Hann for chord bands" },
        } },
        { "rates", std::move(rates) },
        { "warning", "Reverse grains reorder samples only inside causal buffered grains. The complete wet response is not reversed and cannot precede the input." },
    }.dump(2);
}

void writeReverseCosmicShimmerFixtures(
    const ReverseCosmicShimmerValidationReport& report,
    const std::string& outputDirectory)
{
    std::filesystem::create_directories(outputDirectory);
    for (const auto& rate : report.rates) {
        for (const auto fixture : { Fixture::impulse, Fixture::chord, Fixture::noise }) {
            const auto render = renderFixture(rate.sampleRate, fixture);
            const auto suffix = fixture == Fixture::impulse ? "impulse"
                : fixture == Fixture::chord ? "chord" : "noise";
            const auto path = std::filesystem::path(outputDirectory)
                / ("reverse-cosmic-shimmer-" + rateLabel(rate.sampleRate) + "-" + suffix + ".wav");
            writeStereoPcm16Wav(path.string(), rate.sampleRate, render.left, render.right);
        }
    }
}

} // namespace reverb::render
