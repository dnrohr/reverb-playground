#include <reverb/render/WavWriter.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>

namespace reverb::render {
namespace {

void writeU16(std::ostream& stream, const std::uint16_t value)
{
    stream.put(static_cast<char>(value & 0xffU));
    stream.put(static_cast<char>((value >> 8U) & 0xffU));
}

void writeU32(std::ostream& stream, const std::uint32_t value)
{
    writeU16(stream, static_cast<std::uint16_t>(value & 0xffffU));
    writeU16(stream, static_cast<std::uint16_t>(value >> 16U));
}

std::int16_t quantize(const float sample) noexcept
{
    return static_cast<std::int16_t>(std::lrint(std::clamp(sample, -1.0F, 1.0F) * 32'767.0F));
}

} // namespace

void writeStereoPcm16Wav(
    const std::string& path,
    const double sampleRate,
    const std::span<const float> left,
    const std::span<const float> right)
{
    if (left.size() != right.size())
        throw std::invalid_argument("WAV channels must have equal frame counts");
    if (sampleRate < 1.0 || sampleRate > 384'000.0)
        throw std::invalid_argument("WAV sample rate is outside the supported range");

    constexpr std::uint32_t channelCount = 2;
    constexpr std::uint32_t bytesPerSample = 2;
    const auto frames = static_cast<std::uint32_t>(left.size());
    const auto dataBytes = frames * channelCount * bytesPerSample;
    const auto rate = static_cast<std::uint32_t>(std::llround(sampleRate));

    std::ofstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("could not open WAV output '" + path + "'");

    stream.write("RIFF", 4);
    writeU32(stream, 36U + dataBytes);
    stream.write("WAVEfmt ", 8);
    writeU32(stream, 16U);
    writeU16(stream, 1U);
    writeU16(stream, static_cast<std::uint16_t>(channelCount));
    writeU32(stream, rate);
    writeU32(stream, rate * channelCount * bytesPerSample);
    writeU16(stream, static_cast<std::uint16_t>(channelCount * bytesPerSample));
    writeU16(stream, 16U);
    stream.write("data", 4);
    writeU32(stream, dataBytes);

    for (std::size_t frame = 0; frame < left.size(); ++frame) {
        writeU16(stream, static_cast<std::uint16_t>(quantize(left[frame])));
        writeU16(stream, static_cast<std::uint16_t>(quantize(right[frame])));
    }
    if (!stream)
        throw std::runtime_error("failed while writing WAV output '" + path + "'");
}

} // namespace reverb::render
