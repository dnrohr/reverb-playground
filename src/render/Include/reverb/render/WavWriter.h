#pragma once

#include <span>
#include <string>

namespace reverb::render {

void writeStereoPcm16Wav(
    const std::string& path,
    double sampleRate,
    std::span<const float> left,
    std::span<const float> right);

} // namespace reverb::render
