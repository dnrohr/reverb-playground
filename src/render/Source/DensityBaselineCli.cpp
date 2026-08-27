#include <reverb/graph/BarrReferenceGraph.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/render/DensityMeasurements.h>
#include <reverb/render/OfflineRenderer.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string readText(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("could not read '" + path.string() + "'");
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

std::string patchId(std::filesystem::path path)
{
    auto name = path.filename().string();
    constexpr std::string_view suffix { ".rvp.json" };
    if (name.ends_with(suffix)) name.erase(name.size() - suffix.size());
    return name;
}

} // namespace

int main(const int argc, char** argv)
{
    try {
        std::filesystem::path directory, output;
        auto durationSeconds = 2.0;
        bool smoke = false;
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if ((argument == "--factory-directory" || argument == "--output") && index + 1 >= argc)
                throw std::invalid_argument(argument + " requires a value");
            if (argument == "--factory-directory") directory = argv[++index];
            else if (argument == "--output") output = argv[++index];
            else if (argument == "--smoke") smoke = true;
            else throw std::invalid_argument("unknown argument '" + argument + "'");
        }
        if (directory.empty() || output.empty())
            throw std::invalid_argument("usage: reverb_density_baseline_cli --factory-directory <path> --output <path> [--smoke]");

        std::vector<std::filesystem::path> paths;
        for (const auto& item : std::filesystem::directory_iterator(directory))
            if (item.is_regular_file() && item.path().filename().string().ends_with(".rvp.json")) paths.push_back(item.path());
        std::ranges::sort(paths);
        if (smoke && paths.size() > 1) paths.resize(1);

        nlohmann::ordered_json entries = nlohmann::ordered_json::array();
        const std::vector<double> rates = smoke ? std::vector<double> { 48'000.0 }
                                                : std::vector<double> { 44'100.0, 48'000.0, 96'000.0 };
        const auto render = [&](const reverb::graph::GraphDocument& graph, const std::string& id, const double rate) {
            const auto frames = static_cast<std::size_t>(std::llround(rate * (smoke ? 0.3 : durationSeconds)));
            const reverb::render::RenderRequest request { graph, reverb::render::InputKind::impulse, rate, frames };
            const auto audio = reverb::render::renderOffline(request);
            auto measured = reverb::render::measureDensity(audio.left, audio.right, rate);
            measured.engineVersion = graph.engineVersion; measured.patchId = id;
            entries.push_back(nlohmann::ordered_json::parse(reverb::render::writeDensityMeasurementsJson(measured)));
        };
        for (const auto rate : rates) render(reverb::graph::makeBarrReferenceGraph(), "barr-reference", rate);
        for (const auto& path : paths) {
            const auto graph = reverb::graph::parsePatchJson(readText(path));
            for (const auto rate : rates) render(graph, patchId(path), rate);
        }
        nlohmann::ordered_json report {
            { "reportVersion", 1 }, { "analysis", "perceptual-density-v1" },
            { "durationSeconds", smoke ? 0.3 : durationSeconds }, { "entries", std::move(entries) },
        };
        std::ofstream stream(output, std::ios::binary);
        if (!stream) throw std::runtime_error("could not write '" + output.string() + "'");
        stream << report.dump(2) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Density baseline failed: " << error.what() << '\n';
        return 1;
    }
}
