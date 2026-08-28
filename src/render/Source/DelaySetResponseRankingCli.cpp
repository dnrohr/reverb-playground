#include <reverb/graph/DelaySetTuning.h>
#include <reverb/graph/FourLineFdnGraph.h>
#include <reverb/graph/PatchJson.h>
#include <reverb/render/DelaySetResponseRanking.h>
#include <reverb/render/WavWriter.h>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

void writeText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
    if (!stream) throw std::runtime_error("could not write " + path.string());
}

} // namespace

int main(int argc, char** argv)
{
    std::filesystem::path outputDirectory;
    for (int index = 1; index + 1 < argc; ++index)
        if (std::string_view(argv[index]) == "--output-directory") outputDirectory = argv[++index];
    if (outputDirectory.empty()) {
        std::cerr << "usage: reverb_delay_set_response_cli --output-directory <directory>\n";
        return 2;
    }
    try {
        std::filesystem::create_directories(outputDirectory);
        const auto search = reverb::graph::searchDelaySets({});
        const auto ranking = reverb::render::rankRenderedDelaySets(search);
        writeText(outputDirectory / "ranking.json",
            reverb::render::writeDelaySetResponseRankingJson(ranking));
        std::size_t published {};
        for (const auto& ranked : ranking.ranked) {
            if (!ranked.passes.all() || published >= 3) continue;
            ++published;
            const auto stem = "candidate-" + std::to_string(published);
            reverb::graph::FourLineFdnControls controls;
            controls.rt60Seconds = ranking.targetRt60Seconds;
            controls.delayMilliseconds = ranked.candidate.delayMilliseconds;
            writeText(outputDirectory / (stem + ".rvp.json"),
                reverb::graph::writePatchJson(reverb::graph::makeFourLineFdnGraph(controls)));
            for (std::size_t fixture = 0;
                fixture < static_cast<std::size_t>(reverb::render::TuningFixture::count); ++fixture) {
                const auto kind = static_cast<reverb::render::TuningFixture>(fixture);
                auto audio = reverb::render::renderTuningFixture(
                    ranked.candidate, kind, ranking.sampleRate, ranking.targetRt60Seconds);
                reverb::render::normalizeListeningFixture(audio);
                reverb::render::writeStereoPcm16Wav(
                    (outputDirectory / (stem + "-" + reverb::render::tuningFixtureName(kind) + ".wav")).string(),
                    ranking.sampleRate, audio.left, audio.right);
            }
        }
        if (published == 0) throw std::runtime_error("no candidate passed every dimension");
        std::cout << "Wrote rendered ranking and " << published << " ordinary patch candidates\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
