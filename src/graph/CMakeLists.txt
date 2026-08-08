add_library(reverb_graph STATIC
    Source/GraphDocument.cpp)

target_include_directories(reverb_graph
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include)

target_link_libraries(reverb_graph
    PUBLIC
        reverb_project_options
        reverb_dsp)

set_target_properties(reverb_graph PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
