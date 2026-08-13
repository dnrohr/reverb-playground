#include <reverb/render/GravityReference.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/render/WavWriter.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void writeText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("could not write '" + path.string() + "'");
    stream << text;
}

} // namespace

int main(const int argc, char** argv)
{
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--export-factory-patch") {
            writeText(argv[2], reverb::graph::writePatchJson(
                reverb::graph::makeGravityDiffusionGraph(reverb::render::gravityBloomReferenceControls)));
            return 0;
        }
        if (argc != 3 || std::string_view(argv[1]) != "--output-dir")
            throw std::invalid_argument("usage: reverb_gravity_reference_cli --output-dir <directory> | --export-factory-patch <path>");
        const std::filesystem::path outputDirectory = argv[2];
        std::filesystem::create_directories(outputDirectory);
        constexpr double sampleRate = 48'000.0;
        constexpr double seconds = 5.0;
        const auto references = reverb::render::renderGravityReferences(sampleRate, seconds);
        for (const auto& reference : references) {
            const auto stem = "gravity-" + reference.id + "-48k";
            reverb::render::writeStereoPcm16Wav(
                (outputDirectory / (stem + ".wav")).string(), sampleRate,
                reference.loudnessMatched.left, reference.loudnessMatched.right);
            writeText(outputDirectory / (stem + "-measurements.json"),
                reverb::render::writeGravityReferenceJson(
                    reference, sampleRate, reference.loudnessMatched.left.size()));
            std::cout << stem << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Gravity reference generation failed: " << error.what() << '\n';
        return 1;
    }
}
