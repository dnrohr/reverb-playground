#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/BarrReference.h>
#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/render/OfflineRenderer.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("could not read golden WAV: " + path.string());
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

std::int16_t readI16(const std::vector<std::uint8_t>& bytes, const std::size_t offset)
{
    const auto value = static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
    return static_cast<std::int16_t>(value);
}

std::int16_t quantize(const float sample)
{
    return static_cast<std::int16_t>(std::lrint(std::clamp(sample, -1.0F, 1.0F) * 32'767.0F));
}

void requireMatchesGolden(
    const reverb::render::RenderResult& actual,
    const std::filesystem::path& goldenPath)
{
    const auto bytes = readBytes(goldenPath);
    REQUIRE(bytes.size() == 44 + actual.left.size() * 4);
    REQUIRE(std::string(bytes.begin(), bytes.begin() + 4) == "RIFF");
    REQUIRE(std::string(bytes.begin() + 8, bytes.begin() + 12) == "WAVE");

    for (std::size_t frame = 0; frame < actual.left.size(); ++frame) {
        for (std::size_t channel = 0; channel < 2; ++channel) {
            const auto expected = readI16(bytes, 44 + frame * 4 + channel * 2);
            const auto value = quantize(channel == 0 ? actual.left[frame] : actual.right[frame]);
            const auto difference = std::abs(static_cast<int>(value) - static_cast<int>(expected));
            INFO("golden mismatch at frame " << frame << ", channel " << (channel == 0 ? "left" : "right")
                 << ": expected PCM16 " << expected << ", actual " << value << ", difference " << difference);
            REQUIRE(difference <= 1);
        }
    }
}

reverb::render::RenderRequest requestFor(const reverb::render::InputKind input)
{
    return { reverb::graph::makeBarrReferenceGraph(), input, 48'000.0, 12'000 };
}

} // namespace

TEST_CASE("Offline golden renders cover silence impulse and bounded noise")
{
    const auto fixtureRoot = std::filesystem::path { REVERB_TEST_FIXTURES_DIR } / "golden" / "m1-3";
    for (const auto [kind, name] : {
             std::pair { reverb::render::InputKind::silence, "silence-48k-250ms.wav" },
             std::pair { reverb::render::InputKind::impulse, "impulse-48k-250ms.wav" },
             std::pair { reverb::render::InputKind::boundedNoise, "noise-48k-250ms.wav" },
         }) {
        const auto request = requestFor(kind);
        const auto result = reverb::render::renderOffline(request);
        requireMatchesGolden(result, fixtureRoot / name);
    }
}

TEST_CASE("Offline render is deterministic across reset and serialized patch reload")
{
    const auto originalRequest = requestFor(reverb::render::InputKind::impulse);
    const auto first = reverb::render::renderOffline(originalRequest);

    std::vector<float> inputLeft(originalRequest.frameCount, 0.0F);
    std::vector<float> inputRight(originalRequest.frameCount, 0.0F);
    std::vector<float> afterResetLeft(originalRequest.frameCount, 0.0F);
    std::vector<float> afterResetRight(originalRequest.frameCount, 0.0F);
    inputLeft.front() = 1.0F;
    reverb::dsp::BarrReference engine;
    engine.prepare(originalRequest.sampleRate);
    engine.process(inputLeft, inputRight, afterResetLeft, afterResetRight);
    engine.reset();
    std::ranges::fill(afterResetLeft, 0.0F);
    std::ranges::fill(afterResetRight, 0.0F);
    engine.process(inputLeft, inputRight, afterResetLeft, afterResetRight);
    REQUIRE(afterResetLeft == first.left);
    REQUIRE(afterResetRight == first.right);

    auto reloadedRequest = originalRequest;
    reloadedRequest.patch = reverb::graph::parsePatchJson(
        reverb::graph::writePatchJson(originalRequest.patch));
    const auto afterReload = reverb::render::renderOffline(reloadedRequest);
    REQUIRE(afterReload.left == first.left);
    REQUIRE(afterReload.right == first.right);
}

TEST_CASE("Offline render honors the persisted Pitch Shift quality policy")
{
    const auto patchPath = std::filesystem::path { REVERB_FACTORY_PATCH_DIR }
        / "safe-parallel-shimmer.rvp.json";
    std::ifstream stream(patchPath);
    REQUIRE(stream.good());
    const std::string patchJson {
        std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
    auto patch = reverb::graph::parsePatchJson(patchJson);

    const auto render = [&](const reverb::graph::QualityPolicy policy) {
        patch.qualityPolicy = policy;
        return reverb::render::renderOffline({ patch, reverb::render::InputKind::boundedNoise, 48'000.0, 36'000 });
    };
    const auto draft = render(reverb::graph::QualityPolicy::draft);
    const auto normal = render(reverb::graph::QualityPolicy::normal);
    const auto high = render(reverb::graph::QualityPolicy::high);
    REQUIRE(draft.left != normal.left);
    REQUIRE(high.left != normal.left);
    REQUIRE(normal.left == render(reverb::graph::QualityPolicy::normal).left);
}

TEST_CASE("Analysis is stable and machine readable")
{
    const auto request = requestFor(reverb::render::InputKind::impulse);
    const auto result = reverb::render::renderOffline(request);
    const auto analysis = reverb::render::analyse(request, result);
    const auto json = reverb::render::writeAnalysisJson(analysis);

    REQUIRE(analysis.engineVersion == "0.1");
    REQUIRE(analysis.frameCount == 12'000);
    REQUIRE(analysis.left.pcm16Fnv1a == 1'943'302'843'915'056'533ULL);
    REQUIRE(analysis.right.pcm16Fnv1a == 3'929'471'513'242'435'597ULL);
    const auto parsed = nlohmann::json::parse(json);
    REQUIRE(parsed.at("engineVersion") == "0.1");
    REQUIRE(parsed.at("left").at("pcm16Fnv1a") == "1943302843915056533");
}
