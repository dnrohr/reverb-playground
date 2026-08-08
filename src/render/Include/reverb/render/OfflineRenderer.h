#pragma once

#include <reverb/graph/GraphDocument.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace reverb::render {

enum class InputKind {
    silence,
    impulse,
    boundedNoise,
};

struct RenderRequest final {
    reverb::graph::GraphDocument patch;
    InputKind input { InputKind::impulse };
    double sampleRate { 48'000.0 };
    std::size_t frameCount { 48'000 };
};

struct RenderResult final {
    std::vector<float> left;
    std::vector<float> right;
};

struct ChannelAnalysis final {
    double peak {};
    std::size_t firstNonZeroFrame {};
    std::uint64_t pcm16Fnv1a {};
};

struct RenderAnalysis final {
    std::string engineVersion;
    double sampleRate {};
    std::size_t frameCount {};
    std::string input;
    ChannelAnalysis left;
    ChannelAnalysis right;
};

[[nodiscard]] RenderResult renderOffline(const RenderRequest& request);
[[nodiscard]] RenderAnalysis analyse(const RenderRequest& request, const RenderResult& result);
[[nodiscard]] std::string writeAnalysisJson(const RenderAnalysis& analysis);
[[nodiscard]] InputKind parseInputKind(const std::string& value);
[[nodiscard]] std::string inputKindName(InputKind input);

} // namespace reverb::render
