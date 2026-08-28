#include <reverb/render/DenseReverbQualification.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv)
{
    try {
        std::filesystem::path output;
        for (auto index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--output" && index + 1 < argc) output = argv[++index];
            else throw std::invalid_argument(
                "usage: reverb_dense_qualification_cli --output <directory>");
        }
        if (output.empty()) throw std::invalid_argument("--output is required");
#if defined(REVERB_BUILD_COMMIT)
        const std::string commit = REVERB_BUILD_COMMIT;
#else
        const std::string commit = "unreported";
#endif
        reverb::render::writeDenseQualificationArtifacts(output, commit);
        std::cout << "wrote dense qualification artifacts to " << output.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
