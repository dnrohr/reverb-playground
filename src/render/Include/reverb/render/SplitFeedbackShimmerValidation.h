#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace reverb::render {

struct SplitShimmerSpectralWindow final {
    double startSeconds {};
    double sourceDbfs {};
    double octave12FrequencyHertz {};
    double octave12CentsError {};
    double octave12Dbfs {};
    double octave24FrequencyHertz {};
    double octave24CentsError {};
    double octave24Dbfs {};
};

struct SplitShimmerAutomationMetrics final {
    double sampleRate {};
    bool finite {};
    double peak {};
    double maximumAdjacentStep {};
    std::uint64_t processedFrames {};
    std::uint64_t successfulEdits {};
    std::uint64_t completedCrossfades {};
};

struct SplitFeedbackShimmerValidationReport final {
    double sampleRate {};
    double sourceFrequencyHertz {};
    double windowSeconds {};
    SplitShimmerSpectralWindow early;
    SplitShimmerSpectralWindow late;
    double lateOctave24GrowthDb {};
    double splitLateOctave24RelativeTo12Db {};
    double parallelLateOctave24RelativeTo12Db {};
    double feedbackVsParallelOctave24ContrastDb {};
    double lowShiftedFeedback {};
    double highShiftedFeedback {};
    double lowFeedbackLateOctave24Dbfs {};
    double highFeedbackLateOctave24Dbfs {};
    double shiftedFeedbackOctave24IncreaseDb {};
    double lowNormalFeedback {};
    double highNormalFeedback {};
    double normalFeedbackTailEnergyIncreaseDb {};
    double strongestForwardGrainSidebandRelativeDb {};
    double foldedAliasRelativeToFirstOctaveDb {};
    double lowDampingHertz {};
    double highDampingHertz {};
    double lowDampingOctave24LossDb {};
    double stereoCorrelation {};
    std::vector<SplitShimmerAutomationMetrics> automation;
};

[[nodiscard]] SplitFeedbackShimmerValidationReport measureSplitFeedbackShimmerValidation();
[[nodiscard]] std::string writeSplitFeedbackShimmerValidationJson(
    const SplitFeedbackShimmerValidationReport& report);

} // namespace reverb::render
