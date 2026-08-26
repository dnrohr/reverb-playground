#include <reverb/render/PerformanceMatrix.h>

#include <array>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string machineLabel()
{
#if defined(_WIN32)
    char* processor = nullptr;
    std::size_t length = 0;
    static_cast<void>(_dupenv_s(&processor, &length, "PROCESSOR_IDENTIFIER"));
    const std::string identity = processor != nullptr ? processor : "unreported processor";
    std::free(processor);
#else
    const auto* processor = std::getenv("PROCESSOR_IDENTIFIER");
    const std::string identity = processor != nullptr ? processor : "unreported processor";
#endif
    return identity
        + "; logical threads " + std::to_string(std::thread::hardware_concurrency());
}

std::string toolchain()
{
#if defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return "Clang " __clang_version__;
#elif defined(__GNUC__)
    return "GCC " __VERSION__;
#else
    return "unknown C++ toolchain";
#endif
}

std::string buildCommit()
{
#if defined(REVERB_BUILD_COMMIT)
    return REVERB_BUILD_COMMIT;
#else
    return "unreported";
#endif
}

} // namespace

int main(int argc, char** argv)
{
    try {
        std::filesystem::path output;
        auto smoke = false;
        std::size_t measuredBlocks = 200;
        for (auto index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--output" && index + 1 < argc) output = argv[++index];
            else if (argument == "--smoke") smoke = true;
            else if (argument == "--blocks" && index + 1 < argc)
                measuredBlocks = static_cast<std::size_t>(std::stoull(argv[++index]));
            else throw std::invalid_argument("usage: reverb_performance_matrix_cli --output <json> [--smoke] [--blocks N]");
        }
        if (output.empty()) throw std::invalid_argument("--output is required");

        const std::array graphs {
            "barr-reference", "gravity-diffusion", "safe-parallel-shimmer",
            "split-feedback-shimmer", "reverse-cosmic-shimmer",
        };
        const std::array rates { 44'100.0, 48'000.0, 96'000.0 };
        const std::array<std::size_t, 5> blocks { 32, 64, 128, 256, 512 };
        std::vector<reverb::render::PerformanceCaseResult> results;
        if (smoke) {
            results.push_back(reverb::render::measurePerformanceCase(
                { "barr-reference", 48'000.0, 128, std::max<std::size_t>(5, measuredBlocks) }));
        } else {
            results.reserve(graphs.size() * rates.size() * blocks.size());
            for (const auto* graph : graphs)
                for (const auto rate : rates)
                    for (const auto block : blocks) {
                        std::cout << graph << " / " << rate << " / " << block << std::endl;
                        results.push_back(reverb::render::measurePerformanceCase(
                            { graph, rate, block, measuredBlocks }));
                    }
        }
        reverb::render::writePerformanceMatrix(
            output, results, machineLabel(), toolchain(), buildCommit());
        std::cout << "wrote " << results.size() << " performance cases to " << output.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
