#include <catch2/catch_test_macros.hpp>

#include <reverb/dsp/LiveReferenceHarness.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct StereoBlock final {
    std::vector<float> inputLeft { std::vector<float>(12'000, 0.0F) };
    std::vector<float> inputRight { std::vector<float>(12'000, 0.0F) };
    std::vector<float> outputLeft { std::vector<float>(12'000, 0.0F) };
    std::vector<float> outputRight { std::vector<float>(12'000, 0.0F) };
};

void process(reverb::dsp::LiveReferenceHarness& harness, StereoBlock& block)
{
    harness.process(block.inputLeft, block.inputRight, block.outputLeft, block.outputRight);
}

} // namespace

TEST_CASE("Live harness triggers audible impulse with master gain")
{
    reverb::dsp::LiveReferenceHarness full;
    reverb::dsp::LiveReferenceHarness half;
    full.prepare(48'000.0);
    half.prepare(48'000.0);
    full.setMasterGain(1.0F);
    half.setMasterGain(0.5F);
    full.triggerImpulse();
    half.triggerImpulse();
    StereoBlock fullBlock;
    StereoBlock halfBlock;

    process(full, fullBlock);
    process(half, halfBlock);

    REQUIRE(std::ranges::any_of(fullBlock.outputLeft, [](const float sample) { return sample != 0.0F; }));
    REQUIRE(std::ranges::all_of(fullBlock.outputLeft, [](const float sample) { return std::isfinite(sample); }));
    for (std::size_t frame = 0; frame < fullBlock.outputLeft.size(); ++frame) {
        REQUIRE(halfBlock.outputLeft[frame] == fullBlock.outputLeft[frame] * 0.5F);
        REQUIRE(halfBlock.outputRight[frame] == fullBlock.outputRight[frame] * 0.5F);
    }
}

TEST_CASE("Emergency mute and safety latch emit silence until explicitly cleared")
{
    reverb::dsp::LiveReferenceHarness harness;
    harness.prepare(48'000.0);
    StereoBlock block;
    harness.triggerImpulse();
    harness.setEmergencyMuted(true);
    process(harness, block);
    REQUIRE(std::ranges::all_of(block.outputLeft, [](const float sample) { return sample == 0.0F; }));

    harness.setEmergencyMuted(false);
    process(harness, block);
    REQUIRE(std::ranges::any_of(block.outputLeft, [](const float sample) { return sample != 0.0F; }));

    std::ranges::fill(block.inputLeft, 1.0e10F);
    std::ranges::fill(block.inputRight, 1.0e10F);
    process(harness, block);
    REQUIRE(harness.isSafetyLatched());
    REQUIRE(std::ranges::all_of(block.outputLeft, [](const float sample) { return sample == 0.0F; }));
    REQUIRE(std::ranges::all_of(block.outputRight, [](const float sample) { return sample == 0.0F; }));

    harness.requestSafetyReset();
    std::ranges::fill(block.inputLeft, 0.0F);
    std::ranges::fill(block.inputRight, 0.0F);
    process(harness, block);
    REQUIRE_FALSE(harness.isSafetyLatched());
}

TEST_CASE("Device sample-rate changes reprepare and clear live state")
{
    reverb::dsp::LiveReferenceHarness harness;
    harness.prepare(44'100.0);
    harness.triggerImpulse();
    StereoBlock block;
    process(harness, block);

    harness.prepare(96'000.0);
    REQUIRE(harness.sampleRate() == 96'000.0);
    std::ranges::fill(block.outputLeft, 1.0F);
    std::ranges::fill(block.outputRight, 1.0F);
    process(harness, block);
    REQUIRE(std::ranges::all_of(block.outputLeft, [](const float sample) { return sample == 0.0F; }));
    REQUIRE(std::ranges::all_of(block.outputRight, [](const float sample) { return sample == 0.0F; }));
}

TEST_CASE("Live harness without a prepared audio device emits deterministic silence")
{
    reverb::dsp::LiveReferenceHarness harness;
    StereoBlock block;
    std::ranges::fill(block.inputLeft, 1.0F);
    std::ranges::fill(block.inputRight, -1.0F);
    std::ranges::fill(block.outputLeft, 1.0F);
    std::ranges::fill(block.outputRight, 1.0F);
    harness.triggerImpulse();

    process(harness, block);

    REQUIRE(std::ranges::all_of(block.outputLeft, [](const float sample) { return sample == 0.0F; }));
    REQUIRE(std::ranges::all_of(block.outputRight, [](const float sample) { return sample == 0.0F; }));
}

TEST_CASE("Runtime parameter edits publish lock-free and affect the next audio block")
{
    reverb::dsp::LiveReferenceHarness original;
    reverb::dsp::LiveReferenceHarness edited;
    original.prepare(48'000.0);
    edited.prepare(48'000.0);
    edited.setRuntimeParameter(reverb::dsp::BarrParameterId::diffuserOneCoefficient, -0.8);
    REQUIRE(edited.runtimeParameter(reverb::dsp::BarrParameterId::diffuserOneCoefficient) == -0.8);
    original.triggerImpulse();
    edited.triggerImpulse();
    StereoBlock originalBlock;
    StereoBlock editedBlock;
    process(original, originalBlock);
    process(edited, editedBlock);

    REQUIRE(originalBlock.outputLeft != editedBlock.outputLeft);
    REQUIRE(std::ranges::all_of(editedBlock.outputLeft, [](const float sample) { return std::isfinite(sample); }));
    REQUIRE(std::ranges::all_of(editedBlock.outputRight, [](const float sample) { return std::isfinite(sample); }));
}
