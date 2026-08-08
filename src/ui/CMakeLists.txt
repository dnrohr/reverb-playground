add_library(reverb_ui STATIC
    Source/EditorShell.cpp)

target_include_directories(reverb_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include)

target_compile_definitions(reverb_ui
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(reverb_ui
    PUBLIC
        reverb_project_options
        reverb_graph
        juce::juce_gui_basics)

set_target_properties(reverb_ui PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
