#include <reverb/graph/DelaySetTuning.h>

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv)
{
    std::filesystem::path output;
    for (int index = 1; index + 1 < argc; ++index)
        if (std::string_view(argv[index]) == "--output") output = argv[++index];
    if (output.empty()) {
        std::cerr << "usage: reverb_delay_set_tuning_cli --output <report.json>\n";
        return 2;
    }
    const auto report = reverb::graph::searchDelaySets({});
    if (report.ranked.empty()) {
        std::cerr << "no eligible delay sets\n";
        return 1;
    }
    std::filesystem::create_directories(output.parent_path());
    std::ofstream stream(output, std::ios::binary | std::ios::trunc);
    stream << reverb::graph::writeDelaySetSearchJson(report);
    if (!stream) return 1;
    std::cout << "Wrote " << output.string() << " with " << report.ranked.size()
              << " ranked candidates\n";
    return 0;
}
