#include <reverb/render/OfflineRenderer.h>

#include <reverb/dsp/BarrReference.h>
#include <reverb/graph/BarrReferenceGraph.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace reverb::render {
namespace {

std::int16_t quantizePcm16(const float sample) noexcept
{
    const auto clipped = std::clamp(sample, -1.0F, 1.0F);
    return static_cast<std::int16_t>(std::lrint(clipped * 32'767.0F));
}

ChannelAnalysis analyseChannel(const std::vector<float>& samples)
{
    ChannelAnalysis result;
    result.firstNonZeroFrame = samples.size();
    result.pcm16Fnv1a = 14'695'981'039'346'656'037ULL;

    for (std::size_t frame = 0; frame < samples.size(); ++frame) {
        result.peak = std::max(result.peak, static_cast<double>(std::abs(samples[frame])));
        if (result.firstNonZeroFrame == samples.size() && samples[frame] != 0.0F)
            result.firstNonZeroFrame = frame;

        const auto quantized = static_cast<std::uint16_t>(quantizePcm16(samples[frame]));
        for (const auto byte : { static_cast<std::uint8_t>(quantized & 0xffU), static_cast<std::uint8_t>(quantized >> 8U) }) {
            result.pcm16Fnv1a ^= byte;
            result.pcm16Fnv1a *= 1'099'511'628'211ULL;
        }
    }
    return result;
}

void makeInput(const InputKind kind, std::vector<float>& left, std::vector<float>& right)
{
    if (kind == InputKind::impulse && !left.empty()) {
        left.front() = 1.0F;
        return;
    }
    if (kind != InputKind::boundedNoise)
        return;

    std::uint32_t state = 0x6d2b79f5U;
    for (std::size_t frame = 0; frame < left.size(); ++frame) {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        const auto normalized = static_cast<float>(state) / static_cast<float>(std::numeric_limits<std::uint32_t>::max());
        left[frame] = (normalized * 2.0F - 1.0F) * 0.125F;
        right[frame] = left[frame] * 0.25F;
    }
}

} // namespace

RenderResult renderOffline(const RenderRequest& request)
{
    if (request.sampleRate <= 0.0 || request.frameCount == 0)
        throw std::invalid_argument("render requires positive sample rate and frame count");
    if (request.patch != reverb::graph::makeBarrReferenceGraph())
        throw std::invalid_argument("offline renderer currently supports the canonical Barr reference patch only");

    std::vector<float> inputLeft(request.frameCount, 0.0F);
    std::vector<float> inputRight(request.frameCount, 0.0F);
    makeInput(request.input, inputLeft, inputRight);

    RenderResult result { std::vector<float>(request.frameCount), std::vector<float>(request.frameCount) };
    reverb::dsp::BarrReference engine;
    engine.prepare(request.sampleRate);
    engine.process(inputLeft, inputRight, result.left, result.right);
    return result;
}

RenderAnalysis analyse(const RenderRequest& request, const RenderResult& result)
{
    return {
        request.patch.engineVersion,
        request.sampleRate,
        request.frameCount,
        inputKindName(request.input),
        analyseChannel(result.left),
        analyseChannel(result.right),
    };
}

std::string writeAnalysisJson(const RenderAnalysis& analysis)
{
    const auto channel = [](const ChannelAnalysis& value) {
        return nlohmann::ordered_json {
            { "peak", value.peak },
            { "firstNonZeroFrame", value.firstNonZeroFrame },
            { "pcm16Fnv1a", std::to_string(value.pcm16Fnv1a) },
        };
    };
    const nlohmann::ordered_json json {
        { "engineVersion", analysis.engineVersion },
        { "sampleRate", analysis.sampleRate },
        { "frameCount", analysis.frameCount },
        { "input", analysis.input },
        { "left", channel(analysis.left) },
        { "right", channel(analysis.right) },
    };
    return json.dump(2) + "\n";
}

InputKind parseInputKind(const std::string& value)
{
    if (value == "silence")
        return InputKind::silence;
    if (value == "impulse")
        return InputKind::impulse;
    if (value == "noise")
        return InputKind::boundedNoise;
    throw std::invalid_argument("input must be silence, impulse, or noise");
}

std::string inputKindName(const InputKind input)
{
    switch (input) {
    case InputKind::silence: return "silence";
    case InputKind::impulse: return "impulse";
    case InputKind::boundedNoise: return "noise";
    }
    throw std::invalid_argument("unknown input kind");
}

} // namespace reverb::render
