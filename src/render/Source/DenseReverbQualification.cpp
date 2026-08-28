#include <reverb/render/DenseReverbQualification.h>

#include <reverb/graph/AcyclicRuntime.h>
#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/DenseFigureEightGraph.h>
#include <reverb/graph/FourLineFdnGraph.h>
#include <reverb/graph/GravityDiffusionGraph.h>
#include <reverb/render/DensityMeasurements.h>
#include <reverb/render/ResponseMeasurements.h>
#include <reverb/render/WavWriter.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <fstream>
#include <numbers>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace reverb::render {
namespace {

constexpr double sampleRate = 48'000.0;
constexpr std::size_t blockSize = 128;
constexpr double renderSeconds = 3.2;
constexpr double targetRmsDb = -24.0;

struct ProgramSetting final { DenseProgramKind program; DenseSettingKind setting; };
constexpr std::array programSettings {
    ProgramSetting { DenseProgramKind::percussion, DenseSettingKind::room },
    ProgramSetting { DenseProgramKind::speech, DenseSettingKind::room },
    ProgramSetting { DenseProgramKind::piano, DenseSettingKind::hall },
    ProgramSetting { DenseProgramKind::pad, DenseSettingKind::hall },
    ProgramSetting { DenseProgramKind::noise, DenseSettingKind::dark },
    ProgramSetting { DenseProgramKind::speech, DenseSettingKind::dark },
    ProgramSetting { DenseProgramKind::piano, DenseSettingKind::modulated },
    ProgramSetting { DenseProgramKind::pad, DenseSettingKind::modulated },
};
constexpr std::array designIds { "barr-reference", "gravity-diffusion", "dense-figure-eight", "four-line-fdn" };

double db(const double linear) noexcept
{
    return linear > 0.0 ? 20.0 * std::log10(linear) : -160.0;
}

double rms(const std::span<const float> left, const std::span<const float> right) noexcept
{
    double sum {};
    for (std::size_t index = 0; index < std::min(left.size(), right.size()); ++index)
        sum += (static_cast<double>(left[index]) * left[index]
            + static_cast<double>(right[index]) * right[index]) * 0.5;
    return left.empty() ? 0.0 : std::sqrt(sum / static_cast<double>(left.size()));
}

std::pair<std::vector<float>, std::vector<float>> programInput(
    const DenseProgramKind program, const std::size_t frames)
{
    std::vector<float> left(frames), right(frames);
    std::uint32_t random = 0x71ac903dU;
    auto noise = [&]() {
        random = random * 1'664'525U + 1'013'904'223U;
        return (static_cast<double>(random >> 8U) / 16'777'215.0) * 2.0 - 1.0;
    };
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const auto seconds = static_cast<double>(frame) / sampleRate;
        double value {};
        if (program == DenseProgramKind::percussion) {
            constexpr std::array hits { 0.0, 0.18, 0.43, 0.69, 0.94 };
            for (const auto hit : hits) {
                const auto age = seconds - hit;
                if (age >= 0.0 && age < 0.05)
                    value += 0.12 * noise() * std::exp(-age * 95.0);
            }
        } else if (program == DenseProgramKind::speech) {
            constexpr std::array starts { 0.02, 0.19, 0.37, 0.61, 0.79 };
            constexpr std::array lengths { 0.12, 0.13, 0.17, 0.12, 0.18 };
            for (std::size_t syllable = 0; syllable < starts.size(); ++syllable) {
                const auto age = seconds - starts[syllable];
                if (age < 0.0 || age >= lengths[syllable]) continue;
                const auto envelope = std::sin(std::numbers::pi * age / lengths[syllable]);
                const auto fundamental = 118.0 + 17.0 * static_cast<double>(syllable);
                value += envelope * (0.055 * std::sin(2.0 * std::numbers::pi * fundamental * seconds)
                    + 0.025 * std::sin(2.0 * std::numbers::pi * fundamental * 2.1 * seconds)
                    + (age < 0.018 ? 0.035 * noise() : 0.0));
            }
        } else if (program == DenseProgramKind::piano) {
            constexpr std::array starts { 0.0, 0.24, 0.48, 0.72 };
            constexpr std::array frequencies { 220.0, 277.18, 329.63, 440.0 };
            for (std::size_t note = 0; note < starts.size(); ++note) {
                const auto age = seconds - starts[note];
                if (age < 0.0 || age >= 0.55) continue;
                const auto envelope = std::min(1.0, age * 300.0) * std::exp(-age * 5.2);
                value += envelope * (0.07 * std::sin(2.0 * std::numbers::pi * frequencies[note] * seconds)
                    + 0.025 * std::sin(2.0 * std::numbers::pi * frequencies[note] * 2.01 * seconds)
                    + 0.012 * std::sin(2.0 * std::numbers::pi * frequencies[note] * 3.98 * seconds));
            }
        } else if (program == DenseProgramKind::pad) {
            if (seconds < 1.15) {
                const auto envelope = std::min(1.0, seconds / 0.28)
                    * std::min(1.0, (1.15 - seconds) / 0.22);
                for (const auto frequency : { 146.83, 220.0, 277.18, 329.63 })
                    value += 0.018 * envelope * std::sin(2.0 * std::numbers::pi * frequency * seconds);
            }
        } else if (seconds < 0.7) {
            const auto envelope = std::min(1.0, seconds * 30.0)
                * std::min(1.0, (0.7 - seconds) * 20.0);
            value = 0.065 * envelope * noise();
        }
        left[frame] = right[frame] = static_cast<float>(value);
    }
    return { std::move(left), std::move(right) };
}

void setBarrParameter(reverb::graph::GraphDocument& graph, const std::string_view type,
    const std::string_view parameterId, const double factor, const double maximum)
{
    for (auto& node : graph.nodes) {
        if (node.type != type) continue;
        for (auto& parameter : node.parameters) if (parameter.id == parameterId)
            parameter.value = std::clamp(parameter.value * factor, 0.01, maximum);
    }
}

reverb::graph::GraphDocument graphFor(
    const std::string_view design, const DenseSettingKind setting)
{
    if (design == "barr-reference") {
        auto graph = reverb::graph::makeBarrReferenceGraph();
        if (setting == DenseSettingKind::room) {
            setBarrParameter(graph, "delay", "delay", 0.72, 2'000.0);
            setBarrParameter(graph, "allpass", "delay", 0.72, 100.0);
        } else if (setting == DenseSettingKind::hall) {
            setBarrParameter(graph, "delay", "delay", 1.18, 2'000.0);
            setBarrParameter(graph, "allpass", "delay", 1.18, 100.0);
        } else if (setting == DenseSettingKind::dark) {
            setBarrParameter(graph, "lowpass", "cutoff", 0.42, 18'000.0);
        }
        return graph;
    }
    if (design == "gravity-diffusion") {
        reverb::graph::GravityDiffusionControls controls;
        controls.size = setting == DenseSettingKind::room ? -0.65 : setting == DenseSettingKind::hall ? 0.65 : 0.0;
        controls.feedback = setting == DenseSettingKind::room ? -0.45 : setting == DenseSettingKind::hall ? 0.55 : 0.0;
        controls.damping = setting == DenseSettingKind::dark ? -0.85 : 0.0;
        controls.modulation = setting == DenseSettingKind::modulated ? 0.85 : -0.4;
        return reverb::graph::makeGravityDiffusionGraph(controls);
    }
    if (design == "dense-figure-eight") {
        reverb::graph::DenseFigureEightControls controls;
        controls.rt60Seconds = setting == DenseSettingKind::room ? 0.9 : setting == DenseSettingKind::hall ? 4.0 : 2.4;
        controls.dampingHertz = setting == DenseSettingKind::dark ? 2'400.0 : 6'200.0;
        controls.modulationDepthMilliseconds = setting == DenseSettingKind::modulated ? 1.1 : 0.2;
        return reverb::graph::makeDenseFigureEightGraph(controls);
    }
    reverb::graph::FourLineFdnControls controls;
    controls.rt60Seconds = setting == DenseSettingKind::room ? 0.9 : setting == DenseSettingKind::hall ? 4.0 : 2.1;
    controls.dampingHertz = setting == DenseSettingKind::dark ? 2'400.0 : 7'000.0;
    controls.modulationDepthMilliseconds = setting == DenseSettingKind::modulated ? 1.0 : 0.15;
    return reverb::graph::makeFourLineFdnGraph(controls);
}

RenderResult render(const reverb::graph::GraphDocument& graph,
    const std::span<const float> inputLeft, const std::span<const float> inputRight)
{
    auto compiled = reverb::graph::compileFeedbackGraph(graph, sampleRate, blockSize);
    if (!compiled.valid()) throw std::runtime_error("dense qualification graph did not compile");
    RenderResult output { std::vector<float>(inputLeft.size()), std::vector<float>(inputRight.size()) };
    for (std::size_t offset = 0; offset < inputLeft.size(); offset += blockSize) {
        const auto count = std::min(blockSize, inputLeft.size() - offset);
        compiled.runtime->process(inputLeft.subspan(offset, count), inputRight.subspan(offset, count),
            std::span(output.left).subspan(offset, count), std::span(output.right).subspan(offset, count));
    }
    return output;
}

struct ResponseProfile final { DensityRegion density; double spectralRippleDb {}; };

double detrendedSpectralRippleDb(
    const std::span<const float> left, const std::span<const float> right)
{
    constexpr std::size_t transformFrames = 2'048;
    constexpr std::size_t bins = 128;
    if (left.size() < transformFrames || right.size() < transformFrames) return 0.0;
    auto start = left.size() > transformFrames ? (left.size() - transformFrames) / 2 : 0;
    double strongestEnergy {};
    for (auto candidate = start; candidate + transformFrames <= left.size();
         candidate += transformFrames / 2) {
        double energy {};
        for (std::size_t frame = 0; frame < transformFrames; ++frame) {
            const auto mono = 0.5 * (static_cast<double>(left[candidate + frame])
                + right[candidate + frame]);
            energy += mono * mono;
        }
        if (energy > strongestEnergy) {
            strongestEnergy = energy;
            start = candidate;
        }
    }
    if (strongestEnergy <= 1.0e-20) return 0.0;
    std::array<double, bins> powerDb {};
    for (std::size_t bin = 1; bin <= bins; ++bin) {
        std::complex<double> value {};
        for (std::size_t frame = 0; frame < transformFrames; ++frame) {
            const auto mono = 0.5 * (static_cast<double>(left[start + frame])
                + right[start + frame]);
            const auto window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi
                * static_cast<double>(frame) / static_cast<double>(transformFrames - 1));
            const auto phase = -2.0 * std::numbers::pi * static_cast<double>(bin * frame)
                / static_cast<double>(transformFrames);
            value += mono * window * std::complex<double>(std::cos(phase), std::sin(phase));
        }
        powerDb[bin - 1] = 10.0 * std::log10(std::norm(value) + 1.0e-20);
    }
    double maximumPositiveRipple {};
    for (std::size_t bin = 4; bin + 4 < bins; ++bin) {
        double localMean {};
        for (std::size_t neighbor = bin - 4; neighbor <= bin + 4; ++neighbor)
            localMean += powerDb[neighbor];
        localMean /= 9.0;
        maximumPositiveRipple = std::max(maximumPositiveRipple, powerDb[bin] - localMean);
    }
    return maximumPositiveRipple;
}

ResponseProfile activeResponseProfile(const RenderResult& response)
{
    const auto measured = measureResponse(response.left, response.right, sampleRate);
    const auto minimumFrames = static_cast<std::size_t>(sampleRate * 0.25);
    const auto activeFrames = std::min(response.left.size(), std::max(minimumFrames,
        measured.lastActiveFrame.value_or(response.left.size() - 1) + 1));
    const auto left = std::span(response.left).first(activeFrames);
    const auto right = std::span(response.right).first(activeFrames);
    return { measureDensity(left, right, sampleRate).regions.back(),
        detrendedSpectralRippleDb(left, right) };
}

double normalize(RenderResult& audio) noexcept
{
    const auto found = rms(audio.left, audio.right);
    const auto requested = found > 0.0 ? std::pow(10.0, targetRmsDb / 20.0) / found : 1.0;
    double peak {};
    for (const auto channel : { std::span(audio.left), std::span(audio.right) })
        for (const auto sample : channel) peak = std::max(peak, std::abs(static_cast<double>(sample)));
    const auto gain = std::min(requested, peak > 0.0 ? 0.5 / peak : requested);
    for (auto& sample : audio.left) sample *= static_cast<float>(gain);
    for (auto& sample : audio.right) sample *= static_cast<float>(gain);
    return gain;
}

DenseQualificationCase analyse(const DenseProgramKind program, const DenseSettingKind setting,
    const std::string& design, const std::span<const float> sourceLeft,
    const std::span<const float> sourceRight, const ResponseProfile& response, RenderResult& audio)
{
    DenseQualificationCase result;
    result.program = program;
    result.setting = setting;
    result.designId = design;
    result.sourceRmsDb = db(rms(sourceLeft, sourceRight));
    const auto rawRms = rms(audio.left, audio.right);
    const auto gain = normalize(audio);
    result.renderedRmsDb = db(rms(audio.left, audio.right));
    result.normalizationGainDb = db(gain);
    for (const auto channel : { std::span(audio.left), std::span(audio.right) })
        for (const auto sample : channel) result.normalizedPeak = std::max(
            result.normalizedPeak, std::abs(static_cast<double>(sample)));
    result.finite = rawRms > 0.0 && std::ranges::all_of(audio.left,
        [](const auto value) { return std::isfinite(value); }) && std::ranges::all_of(audio.right,
        [](const auto value) { return std::isfinite(value); });
    result.echoDensity = response.density.echoDensity;
    result.recurrence = response.density.recurrence;
    result.spectralFlatness = response.density.spectralFlatness;
    result.spectralRippleDb = response.spectralRippleDb;
    const auto programDensity = measureDensity(audio.left, audio.right, sampleRate);
    result.crestFactor = programDensity.regions.back().crestFactor;
    result.stereoCorrelation = response.density.stereoCorrelation;
    double stereoEnergy {}, monoEnergy {};
    for (std::size_t frame = 0; frame < audio.left.size(); ++frame) {
        const auto left = static_cast<double>(audio.left[frame]);
        const auto right = static_cast<double>(audio.right[frame]);
        stereoEnergy += left * left + right * right;
        const auto mono = 0.5 * (left + right);
        monoEnergy += 2.0 * mono * mono;
    }
    result.monoEnergyRatio = stereoEnergy > 0.0 ? monoEnergy / stereoEnergy : 0.0;
    result.repeatLikePass = result.echoDensity >= 0.65 && result.recurrence <= 0.85;
    result.colorationPass = result.spectralRippleDb <= 12.0;
    result.smearingPass = result.crestFactor >= 1.05;
    result.monoCompatibilityPass = result.monoEnergyRatio >= 0.08
        && std::abs(result.stereoCorrelation) <= 0.995;
    return result;
}

} // namespace

const char* denseProgramName(const DenseProgramKind program) noexcept
{
    switch (program) {
    case DenseProgramKind::percussion: return "percussion";
    case DenseProgramKind::speech: return "speech";
    case DenseProgramKind::piano: return "piano";
    case DenseProgramKind::pad: return "pad";
    case DenseProgramKind::noise: return "noise";
    }
    return "unknown";
}

const char* denseSettingName(const DenseSettingKind setting) noexcept
{
    switch (setting) {
    case DenseSettingKind::room: return "room";
    case DenseSettingKind::hall: return "hall";
    case DenseSettingKind::dark: return "dark";
    case DenseSettingKind::modulated: return "modulated";
    }
    return "unknown";
}

DenseQualificationReport qualifyDenseReverbs()
{
    DenseQualificationReport report;
    report.sampleRate = sampleRate;
    report.blockSize = blockSize;
    report.targetRmsDb = targetRmsDb;
    const auto frames = static_cast<std::size_t>(sampleRate * renderSeconds);
    for (const auto [program, setting] : programSettings) {
        const auto [sourceLeft, sourceRight] = programInput(program, frames);
        for (const auto* design : designIds) {
            const auto graph = graphFor(design, setting);
            std::vector<float> impulseLeft(frames), impulseRight(frames);
            impulseLeft.front() = impulseRight.front() = 0.1F;
            const auto responseAudio = render(graph, impulseLeft, impulseRight);
            const auto response = activeResponseProfile(responseAudio);
            auto audio = render(graph, sourceLeft, sourceRight);
            report.cases.push_back(analyse(
                program, setting, design, sourceLeft, sourceRight, response, audio));
        }
    }
    return report;
}

std::string denseQualificationJson(
    const DenseQualificationReport& report, std::string buildCommit)
{
    nlohmann::ordered_json cases = nlohmann::ordered_json::array();
    for (const auto& measured : report.cases) cases.push_back({
        { "program", denseProgramName(measured.program) },
        { "setting", denseSettingName(measured.setting) }, { "designId", measured.designId },
        { "levels", { { "sourceRmsDb", measured.sourceRmsDb },
            { "renderedRmsDb", measured.renderedRmsDb },
            { "normalizationGainDb", measured.normalizationGainDb },
            { "normalizedPeak", measured.normalizedPeak } } },
        { "objective", { { "echoDensity", measured.echoDensity },
            { "recurrence", measured.recurrence }, { "spectralFlatness", measured.spectralFlatness },
            { "spectralRippleDb", measured.spectralRippleDb },
            { "crestFactor", measured.crestFactor }, { "stereoCorrelation", measured.stereoCorrelation },
            { "monoEnergyRatio", measured.monoEnergyRatio }, { "finite", measured.finite } } },
        { "passes", { { "repeatLike", measured.repeatLikePass },
            { "coloration", measured.colorationPass }, { "smearing", measured.smearingPass },
            { "monoCompatibility", measured.monoCompatibilityPass } } },
    });
    return nlohmann::ordered_json {
        { "formatVersion", 1 }, { "measurement", "dense-reverb-product-qualification" },
        { "buildCommit", std::move(buildCommit) }, { "sampleRate", report.sampleRate },
        { "blockSize", report.blockSize }, { "targetRmsDb", report.targetRmsDb },
        { "normalization", "integrated stereo RMS target with a 0.5 peak ceiling; objective metrics use the normalized renders" },
        { "listeningOrder", designIds },
        { "thresholds", { { "minimumEchoDensity", 0.65 }, { "maximumRecurrence", 0.85 },
            { "maximumSpectralRippleDb", 12.0 }, { "minimumCrestFactor", 1.05 },
            { "minimumMonoEnergyRatio", 0.08 }, { "maximumAbsoluteStereoCorrelation", 0.995 } } },
        { "cases", std::move(cases) },
    }.dump(2) + "\n";
}

void writeDenseQualificationArtifacts(
    const std::filesystem::path& outputDirectory, std::string buildCommit)
{
    std::filesystem::create_directories(outputDirectory);
    DenseQualificationReport report;
    report.sampleRate = sampleRate;
    report.blockSize = blockSize;
    report.targetRmsDb = targetRmsDb;
    const auto frames = static_cast<std::size_t>(sampleRate * renderSeconds);
    const auto gap = static_cast<std::size_t>(sampleRate * 0.2);
    for (const auto [program, setting] : programSettings) {
        const auto [sourceLeft, sourceRight] = programInput(program, frames);
        std::vector<float> reelLeft, reelRight;
        for (const auto* design : designIds) {
            const auto graph = graphFor(design, setting);
            std::vector<float> impulseLeft(frames), impulseRight(frames);
            impulseLeft.front() = impulseRight.front() = 0.1F;
            const auto responseAudio = render(graph, impulseLeft, impulseRight);
            const auto response = activeResponseProfile(responseAudio);
            auto audio = render(graph, sourceLeft, sourceRight);
            report.cases.push_back(analyse(
                program, setting, design, sourceLeft, sourceRight, response, audio));
            reelLeft.insert(reelLeft.end(), audio.left.begin(), audio.left.end());
            reelRight.insert(reelRight.end(), audio.right.begin(), audio.right.end());
            reelLeft.insert(reelLeft.end(), gap, 0.0F);
            reelRight.insert(reelRight.end(), gap, 0.0F);
        }
        const auto stem = std::string(denseProgramName(program)) + "-" + denseSettingName(setting);
        writeStereoPcm16Wav((outputDirectory / (stem + "-comparison.wav")).string(),
            sampleRate, reelLeft, reelRight);
    }
    std::ofstream stream(outputDirectory / "objective-report.json", std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("could not open dense qualification report");
    stream << denseQualificationJson(report, std::move(buildCommit));
    if (!stream) throw std::runtime_error("could not write dense qualification report");
}

} // namespace reverb::render
