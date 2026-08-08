add_library(reverb_render STATIC
    Source/OfflineRenderer.cpp
    Source/WavWriter.cpp)

target_include_directories(reverb_render
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include)

target_link_libraries(reverb_render
    PUBLIC
        reverb_project_options
        reverb_dsp
        reverb_graph
        nlohmann_json::nlohmann_json)

set_target_properties(reverb_render PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

add_executable(reverb_render_cli
    Source/RenderCli.cpp)

target_link_libraries(reverb_render_cli
    PRIVATE
        reverb_project_options
        reverb_render)
