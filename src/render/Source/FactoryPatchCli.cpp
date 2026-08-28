#include <reverb/graph/PatchJson.h>
#include <reverb/graph/DenseFigureEightGraph.h>
#include <reverb/graph/FourLineFdnGraph.h>
#include <reverb/graph/ReverseCosmicShimmerGraph.h>
#include <reverb/graph/SafeParallelShimmerGraph.h>
#include <reverb/graph/SplitFeedbackShimmerGraph.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

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
        if (argc != 4 || std::string_view(argv[1]) != "--export")
            throw std::invalid_argument("usage: reverb_factory_patch_cli --export <patch-id> <path>");
        const auto patchId = std::string_view(argv[2]);
        if (patchId == "four-line-fdn") {
            writeText(argv[3], reverb::graph::writePatchJson(
                reverb::graph::makeFourLineFdnGraph()));
        } else if (patchId == "dense-figure-eight") {
            writeText(argv[3], reverb::graph::writePatchJson(
                reverb::graph::makeDenseFigureEightGraph()));
        } else if (patchId == "safe-parallel-shimmer") {
            writeText(argv[3], reverb::graph::writePatchJson(
                reverb::graph::makeSafeParallelShimmerGraph()));
        } else if (patchId == "split-feedback-shimmer") {
            writeText(argv[3], reverb::graph::writePatchJson(
                reverb::graph::makeSplitFeedbackShimmerGraph()));
        } else if (patchId == "reverse-cosmic-shimmer") {
            writeText(argv[3], reverb::graph::writePatchJson(
                reverb::graph::makeReverseCosmicShimmerGraph()));
        } else {
            throw std::invalid_argument("unknown factory patch id '" + std::string(patchId) + "'");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Factory patch generation failed: " << error.what() << '\n';
        return 1;
    }
}
