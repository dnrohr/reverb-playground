add_executable(reverb_tests
    AcyclicRuntimeTests.cpp
    AuditionDeckLayoutTests.cpp
    AuditionWaveformStyleTests.cpp
    AudioFileWorkflowQualificationTests.cpp
    BarrReferenceTests.cpp
    ControlRateTests.cpp
    ControlModulationTests.cpp
    DelayMemoryPlanTests.cpp
    DenseFigureEightTests.cpp
    DelaySetTuningTests.cpp
    DspGainTests.cpp
    DspPrimitivesTests.cpp
    DensityMeasurementsTests.cpp
    EnergyTelemetryTests.cpp
    EnvelopeGateTests.cpp
    EnvelopeGateRuntimeTests.cpp
    FactoryPatchTests.cpp
    FeedbackRuntimeTests.cpp
    FourLineFdnTests.cpp
    GravityReferenceTests.cpp
    ImpulseCaptureTests.cpp
    LiveReferenceHarnessTests.cpp
    OfflineRendererTests.cpp
    PerformanceMatrixTests.cpp
    PreparedAudioFileSourceTests.cpp
    ProcessedFileExporterTests.cpp
    ResponseMeasurementsTests.cpp
    RuntimeDiagnosticsTests.cpp
    RuntimeSnapshotTests.cpp
    ReverseCosmicShimmerTests.cpp
    SafeParallelShimmerTests.cpp
    SplitFeedbackShimmerTests.cpp
    StartupProgressTests.cpp
    NumericalSafetyGuardTests.cpp
    PitchShiftContractTests.cpp
    PitchShiftTests.cpp
    PitchShiftValidationTests.cpp
    GraphDocumentTests.cpp
    HostPatchStateTests.cpp
    PluginHostStateTests.cpp)

target_link_libraries(reverb_tests
    PRIVATE
        reverb_project_options
        reverb_audio
        reverb_dsp
        reverb_graph
        reverb_render
        reverb_ui
        ReverbPlayground
        juce::juce_audio_processors
        Catch2::Catch2WithMain)

target_include_directories(reverb_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)

target_compile_definitions(reverb_tests
    PRIVATE
        REVERB_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        REVERB_FACTORY_PATCH_DIR="${CMAKE_SOURCE_DIR}/factory-patches"
        REVERB_MEASUREMENTS_DIR="${CMAKE_SOURCE_DIR}/artifacts/measurements")

include(Catch)
catch_discover_tests(reverb_tests)

add_test(
    NAME "Offline renderer CLI writes WAV and analysis"
    COMMAND reverb_render_cli
        --input impulse
        --sample-rate 48000
        --duration-ms 50
        --output ${CMAKE_CURRENT_BINARY_DIR}/cli-smoke.wav
        --analysis ${CMAKE_CURRENT_BINARY_DIR}/cli-smoke.json
        --measurements ${CMAKE_CURRENT_BINARY_DIR}/cli-smoke-measurements.json)

add_test(
    NAME "Pitch Shift validation CLI writes measurements"
    COMMAND reverb_pitch_shift_validation_cli
        --output ${CMAKE_CURRENT_BINARY_DIR}/pitch-shift-validation-smoke.json)

add_test(
    NAME "Split Feedback Shimmer validation CLI writes measurements"
    COMMAND reverb_split_feedback_shimmer_validation_cli
        --output ${CMAKE_CURRENT_BINARY_DIR}/split-feedback-shimmer-validation-smoke.json)

add_test(
    NAME "Reverse Cosmic Shimmer validation CLI writes fixtures"
    COMMAND reverb_reverse_cosmic_shimmer_validation_cli
        --output-directory ${CMAKE_CURRENT_BINARY_DIR}/reverse-cosmic-shimmer-validation-smoke
        --report ${CMAKE_CURRENT_BINARY_DIR}/reverse-cosmic-shimmer-validation-smoke.json)

add_test(
    NAME "Performance matrix CLI writes a smoke report"
    COMMAND reverb_performance_matrix_cli
        --output ${CMAKE_CURRENT_BINARY_DIR}/performance-matrix-smoke.json
        --smoke
        --blocks 5)

add_test(
    NAME "Density baseline CLI writes a smoke report"
    COMMAND reverb_density_baseline_cli
        --factory-directory ${CMAKE_SOURCE_DIR}/factory-patches
        --output ${CMAKE_CURRENT_BINARY_DIR}/density-baseline-smoke.json
        --smoke)

add_test(
    NAME "Delay-set tuning CLI writes deterministic candidates"
    COMMAND reverb_delay_set_tuning_cli
        --output ${CMAKE_CURRENT_BINARY_DIR}/delay-set-candidates-smoke.json)
