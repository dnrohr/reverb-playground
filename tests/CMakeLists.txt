add_executable(reverb_tests
    DspGainTests.cpp
    GraphDocumentTests.cpp)

target_link_libraries(reverb_tests
    PRIVATE
        reverb_project_options
        reverb_dsp
        reverb_graph
        Catch2::Catch2WithMain)

target_compile_definitions(reverb_tests
    PRIVATE
        REVERB_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

include(Catch)
catch_discover_tests(reverb_tests)
