add_executable(reverb_tests
    BarrReferenceTests.cpp
    DspGainTests.cpp
    DspPrimitivesTests.cpp
    LiveReferenceHarnessTests.cpp
    OfflineRendererTests.cpp
    ResponseMeasurementsTests.cpp
    RuntimeSnapshotTests.cpp
    NumericalSafetyGuardTests.cpp
    GraphDocumentTests.cpp)

target_link_libraries(reverb_tests
    PRIVATE
        reverb_project_options
        reverb_dsp
        reverb_graph
        reverb_render
        Catch2::Catch2WithMain)

target_compile_definitions(reverb_tests
    PRIVATE
        REVERB_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

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
