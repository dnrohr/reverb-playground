#include <reverb/render/SplitFeedbackShimmerValidation.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

int main(const int argc, char** argv)
{
    try {
        if (argc != 3 || std::string_view(argv[1]) != "--output")
            throw std::invalid_argument(
                "usage: reverb_split_feedback_shimmer_validation_cli --output <path>");
        const std::filesystem::path output = argv[2];
        if (!output.parent_path().empty()) std::filesystem::create_directories(output.parent_path());
        std::ofstream stream(output, std::ios::binary);
        if (!stream) throw std::runtime_error("could not open output file");
        stream << reverb::render::writeSplitFeedbackShimmerValidationJson(
            reverb::render::measureSplitFeedbackShimmerValidation()) << '\n';
        std::cout << output.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Split Feedback Shimmer validation failed: " << error.what() << '\n';
        return 1;
    }
}
