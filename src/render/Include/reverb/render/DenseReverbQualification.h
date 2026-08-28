#pragma once

#include <reverb/render/OfflineRenderer.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace reverb::render {

enum class DenseProgramKind { percussion, speech, piano, pad, noise };
enum class DenseSettingKind { room, hall, dark, modulated };

struct DenseQualificationCase final {
    DenseProgramKind program {};
    DenseSettingKind setting {};
    std::string designId;
    double sourceRmsDb {};
    double renderedRmsDb {};
    double normalizationGainDb {};
    double normalizedPeak {};
    double echoDensity {};
    double recurrence {};
    double spectralFlatness {};
    double spectralRippleDb {};
    double crestFactor {};
    double stereoCorrelation {};
    double monoEnergyRatio {};
    bool finite {};
    bool repeatLikePass {};
    bool colorationPass {};
    bool smearingPass {};
    bool monoCompatibilityPass {};
};

struct DenseQualificationReport final {
    double sampleRate { 48'000.0 };
    std::size_t blockSize { 128 };
    double targetRmsDb { -24.0 };
    std::vector<DenseQualificationCase> cases;
};

[[nodiscard]] DenseQualificationReport qualifyDenseReverbs();
[[nodiscard]] std::string denseQualificationJson(
    const DenseQualificationReport& report, std::string buildCommit);
void writeDenseQualificationArtifacts(
    const std::filesystem::path& outputDirectory, std::string buildCommit);
[[nodiscard]] const char* denseProgramName(DenseProgramKind program) noexcept;
[[nodiscard]] const char* denseSettingName(DenseSettingKind setting) noexcept;

} // namespace reverb::render
