add_executable(reverb_tests
    DspGainTests.cpp
    GraphDocumentTests.cpp)

target_link_libraries(reverb_tests
    PRIVATE
        reverb_project_options
        reverb_dsp
        reverb_graph
        Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(reverb_tests)
