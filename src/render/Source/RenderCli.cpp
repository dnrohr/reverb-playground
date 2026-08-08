#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/render/OfflineRenderer.h>
#include <reverb/render/WavWriter.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct Options final {
    std::string patchPath;
    std::string outputPath;
    std::string analysisPath;
    std::string exportPatchPath;
    reverb::render::InputKind input { reverb::render::InputKind::impulse };
    double sampleRate { 48'000.0 };
    double durationMilliseconds { 1'000.0 };
};

std::string requireValue(const int argc, char** argv, int& index)
{
    if (++index >= argc)
        throw std::invalid_argument(std::string(argv[index - 1]) + " requires a value");
    return argv[index];
}

Options parseOptions(const int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--patch")
            options.patchPath = requireValue(argc, argv, index);
        else if (argument == "--output")
            options.outputPath = requireValue(argc, argv, index);
        else if (argument == "--analysis")
            options.analysisPath = requireValue(argc, argv, index);
        else if (argument == "--input")
            options.input = reverb::render::parseInputKind(requireValue(argc, argv, index));
        else if (argument == "--sample-rate")
            options.sampleRate = std::stod(requireValue(argc, argv, index));
        else if (argument == "--duration-ms")
            options.durationMilliseconds = std::stod(requireValue(argc, argv, index));
        else if (argument == "--export-patch")
            options.exportPatchPath = requireValue(argc, argv, index);
        else
            throw std::invalid_argument("unknown argument '" + argument + "'");
    }
    return options;
}

std::string readFile(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("could not open patch '" + path + "'");
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

void writeFile(const std::string& path, const std::string& content)
{
    std::ofstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("could not open output '" + path + "'");
    stream << content;
    if (!stream)
        throw std::runtime_error("failed while writing output '" + path + "'");
}

} // namespace

int main(const int argc, char** argv)
{
    try {
        const auto options = parseOptions(argc, argv);
        auto patch = reverb::graph::makeBarrReferenceGraph();
        if (!options.exportPatchPath.empty())
            writeFile(options.exportPatchPath, reverb::graph::writePatchJson(patch));
        if (!options.patchPath.empty())
            patch = reverb::graph::parsePatchJson(readFile(options.patchPath));
        if (options.outputPath.empty()) {
            if (!options.exportPatchPath.empty())
                return 0;
            throw std::invalid_argument("--output is required");
        }

        const auto frames = static_cast<std::size_t>(
            std::llround(options.sampleRate * options.durationMilliseconds / 1000.0));
        const reverb::render::RenderRequest request { patch, options.input, options.sampleRate, frames };
        const auto result = reverb::render::renderOffline(request);
        const auto analysis = reverb::render::analyse(request, result);
        reverb::render::writeStereoPcm16Wav(
            options.outputPath, request.sampleRate, result.left, result.right);

        const auto analysisJson = reverb::render::writeAnalysisJson(analysis);
        if (options.analysisPath.empty())
            std::cout << analysisJson;
        else
            writeFile(options.analysisPath, analysisJson);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "render failed: " << error.what() << '\n';
        return 1;
    }
}
