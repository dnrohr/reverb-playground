#include <reverb/render/ReverseCosmicShimmerValidation.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(const int argc, char** argv)
{
    try {
        if (argc != 5 || std::string(argv[1]) != "--output-directory"
            || std::string(argv[3]) != "--report") {
            throw std::invalid_argument(
                "usage: reverb_reverse_cosmic_shimmer_validation_cli --output-directory <path> --report <path>");
        }
        const auto report = reverb::render::measureReverseCosmicShimmerValidation();
        reverb::render::writeReverseCosmicShimmerFixtures(report, argv[2]);
        const auto reportPath = std::filesystem::path(argv[4]);
        std::filesystem::create_directories(reportPath.parent_path());
        std::ofstream stream(reportPath, std::ios::binary);
        if (!stream) throw std::runtime_error("could not open report output");
        stream << reverb::render::writeReverseCosmicShimmerValidationJson(report) << '\n';
        std::cout << reportPath.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Reverse Cosmic Shimmer validation failed: " << error.what() << '\n';
        return 1;
    }
}
