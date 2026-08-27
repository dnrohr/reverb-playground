#include <reverb/render/DensityMeasurements.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <numeric>
#include <numbers>
#include <stdexcept>

namespace reverb::render {
namespace {

constexpr double epsilon = 1.0e-20;
constexpr double gaussianTailProbability = 0.31731050786291415;

double correlation(const std::span<const float> a, const std::span<const float> b)
{
    double aa = 0.0, bb = 0.0, ab = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        aa += static_cast<double>(a[i]) * a[i];
        bb += static_cast<double>(b[i]) * b[i];
        ab += static_cast<double>(a[i]) * b[i];
    }
    return aa > epsilon && bb > epsilon ? std::clamp(ab / std::sqrt(aa * bb), -1.0, 1.0) : 0.0;
}

std::pair<double, std::size_t> recurrence(const std::span<const double> mono, const double sampleRate)
{
    const auto minimumLag = std::max<std::size_t>(1, static_cast<std::size_t>(sampleRate * 0.001));
    const auto maximumLag = std::min(mono.size() / 2, static_cast<std::size_t>(sampleRate * 0.030));
    double best = 0.0;
    std::size_t bestLag = 0;
    for (auto lag = minimumLag; lag <= maximumLag; ++lag) {
        double x = 0.0, y = 0.0, xy = 0.0;
        for (std::size_t i = lag; i < mono.size(); ++i) {
            x += mono[i] * mono[i];
            y += mono[i - lag] * mono[i - lag];
            xy += mono[i] * mono[i - lag];
        }
        const auto value = x > epsilon && y > epsilon ? std::abs(xy) / std::sqrt(x * y) : 0.0;
        if (value > best) { best = value; bestLag = lag; }
    }
    return { best, bestLag };
}

double spectralFlatness(const std::span<const double> mono)
{
    constexpr std::size_t bins = 64;
    const auto length = std::min<std::size_t>(512, mono.size());
    if (length < 8) return 0.0;
    double logSum = 0.0, linearSum = 0.0;
    for (std::size_t bin = 1; bin <= bins; ++bin) {
        std::complex<double> value {};
        for (std::size_t n = 0; n < length; ++n) {
            const auto window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * n / static_cast<double>(length - 1));
            const auto phase = -2.0 * std::numbers::pi * bin * n / static_cast<double>(length);
            value += mono[n] * window * std::complex<double>(std::cos(phase), std::sin(phase));
        }
        const auto power = std::norm(value) + epsilon;
        logSum += std::log(power);
        linearSum += power;
    }
    return std::clamp(std::exp(logSum / bins) / (linearSum / bins), 0.0, 1.0);
}

DensityWindow measureWindow(
    const std::span<const float> left, const std::span<const float> right,
    const double sampleRate, const std::size_t start)
{
    std::vector<double> mono(left.size());
    double energy = 0.0, fourth = 0.0, peak = 0.0;
    for (std::size_t i = 0; i < mono.size(); ++i) {
        mono[i] = 0.5 * (static_cast<double>(left[i]) + right[i]);
        const auto square = mono[i] * mono[i];
        energy += square; fourth += square * square; peak = std::max(peak, std::abs(mono[i]));
    }
    const auto meanSquare = energy / static_cast<double>(mono.size());
    const auto rms = std::sqrt(meanSquare);
    std::size_t above = 0, peaks = 0;
    for (std::size_t i = 0; i < mono.size(); ++i) {
        if (std::abs(mono[i]) > rms) ++above;
        if (i > 0 && i + 1 < mono.size() && std::abs(mono[i]) > rms * 0.5
            && std::abs(mono[i]) >= std::abs(mono[i - 1]) && std::abs(mono[i]) > std::abs(mono[i + 1]))
            ++peaks;
    }
    constexpr std::size_t slices = 8;
    std::array<double, slices> sliceEnergy {};
    for (std::size_t i = 0; i < mono.size(); ++i)
        sliceEnergy[std::min(slices - 1, i * slices / mono.size())] += mono[i] * mono[i];
    const auto sliceMean = std::accumulate(sliceEnergy.begin(), sliceEnergy.end(), 0.0) / slices;
    double sliceVariance = 0.0;
    for (const auto value : sliceEnergy) sliceVariance += (value - sliceMean) * (value - sliceMean);
    const auto [recurrenceValue, lag] = recurrence(mono, sampleRate);
    return {
        static_cast<double>(start) / sampleRate,
        std::clamp((static_cast<double>(above) / mono.size()) / gaussianTailProbability, 0.0, 1.0),
        peaks,
        rms > epsilon ? peak / rms : 0.0,
        meanSquare > epsilon ? fourth / static_cast<double>(mono.size()) / (meanSquare * meanSquare) : 0.0,
        sliceMean > epsilon ? std::sqrt(sliceVariance / slices) / sliceMean : 0.0,
        recurrenceValue,
        1000.0 * static_cast<double>(lag) / sampleRate,
        spectralFlatness(mono),
        correlation(left, right),
    };
}

DensityRegion summarize(const std::vector<DensityWindow>& windows, const std::string& name,
    const double start, const double end, const double windowSeconds)
{
    DensityRegion result; result.name = name; result.startSeconds = start; result.endSeconds = end;
    std::size_t count = 0, peaks = 0;
    double recurrenceLagWeighted = 0.0;
    for (const auto& point : windows) {
        if (point.startSeconds + windowSeconds <= start || point.startSeconds >= end) continue;
        ++count; peaks += point.activePeakCount;
        result.echoDensity += point.echoDensity; result.crestFactor += point.crestFactor;
        result.kurtosis += point.kurtosis; result.energyVariation += point.energyVariation;
        result.recurrence += point.recurrence; recurrenceLagWeighted += point.recurrence * point.recurrenceMilliseconds;
        result.spectralFlatness += point.spectralFlatness; result.stereoCorrelation += point.stereoCorrelation;
    }
    if (count == 0) return result;
    const auto scale = 1.0 / static_cast<double>(count);
    result.echoDensity *= scale; result.crestFactor *= scale; result.kurtosis *= scale;
    result.energyVariation *= scale; result.recurrence *= scale; result.spectralFlatness *= scale;
    result.stereoCorrelation *= scale;
    result.recurrenceMilliseconds = result.recurrence > epsilon
        ? recurrenceLagWeighted / (result.recurrence * count) : 0.0;
    result.activePeaksPerSecond = static_cast<double>(peaks) / (count * windowSeconds);
    return result;
}

} // namespace

DensityMeasurements measureDensity(const std::span<const float> left, const std::span<const float> right,
    const double sampleRate, const double windowMilliseconds, const double hopMilliseconds)
{
    if (left.size() != right.size() || left.empty()) throw std::invalid_argument("density channels must have equal nonzero frame counts");
    if (sampleRate <= 0.0 || windowMilliseconds <= 0.0 || hopMilliseconds <= 0.0)
        throw std::invalid_argument("density rate and windows must be positive");
    DensityMeasurements result;
    result.sampleRate = sampleRate; result.frameCount = left.size();
    result.windowMilliseconds = windowMilliseconds; result.hopMilliseconds = hopMilliseconds;
    const auto windowFrames = std::max<std::size_t>(8, static_cast<std::size_t>(std::llround(sampleRate * windowMilliseconds / 1000.0)));
    const auto hopFrames = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(sampleRate * hopMilliseconds / 1000.0)));
    for (std::size_t start = 0; start + windowFrames <= left.size(); start += hopFrames)
        result.windows.push_back(measureWindow(left.subspan(start, windowFrames), right.subspan(start, windowFrames), sampleRate, start));
    const auto duration = static_cast<double>(left.size()) / sampleRate;
    const auto third = duration / 3.0;
    const auto windowSeconds = windowMilliseconds / 1000.0;
    result.regions.push_back(summarize(result.windows, "early", 0.0, third, windowSeconds));
    result.regions.push_back(summarize(result.windows, "middle", third, 2.0 * third, windowSeconds));
    result.regions.push_back(summarize(result.windows, "late", 2.0 * third, duration, windowSeconds));
    return result;
}

std::string writeDensityMeasurementsJson(const DensityMeasurements& value)
{
    nlohmann::ordered_json windows = nlohmann::ordered_json::array();
    for (const auto& p : value.windows) windows.push_back({
        {"startSeconds", p.startSeconds}, {"echoDensity", p.echoDensity}, {"activePeakCount", p.activePeakCount},
        {"crestFactor", p.crestFactor}, {"kurtosis", p.kurtosis}, {"energyVariation", p.energyVariation},
        {"recurrence", p.recurrence}, {"recurrenceMilliseconds", p.recurrenceMilliseconds},
        {"spectralFlatness", p.spectralFlatness}, {"stereoCorrelation", p.stereoCorrelation} });
    nlohmann::ordered_json regions = nlohmann::ordered_json::array();
    for (const auto& p : value.regions) regions.push_back({
        {"name", p.name}, {"startSeconds", p.startSeconds}, {"endSeconds", p.endSeconds},
        {"echoDensity", p.echoDensity}, {"activePeaksPerSecond", p.activePeaksPerSecond},
        {"crestFactor", p.crestFactor}, {"kurtosis", p.kurtosis}, {"energyVariation", p.energyVariation},
        {"recurrence", p.recurrence}, {"recurrenceMilliseconds", p.recurrenceMilliseconds},
        {"spectralFlatness", p.spectralFlatness}, {"stereoCorrelation", p.stereoCorrelation} });
    return nlohmann::ordered_json({ {"measurementVersion", 1}, {"engineVersion", value.engineVersion},
        {"patchId", value.patchId}, {"sampleRate", value.sampleRate}, {"frameCount", value.frameCount},
        {"windowMilliseconds", value.windowMilliseconds}, {"hopMilliseconds", value.hopMilliseconds},
        {"windows", std::move(windows)}, {"regions", std::move(regions)} }).dump(2) + "\n";
}

} // namespace reverb::render
