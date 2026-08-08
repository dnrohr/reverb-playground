add_library(reverb_dsp STATIC
    Source/Gain.cpp)

target_include_directories(reverb_dsp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include)

target_link_libraries(reverb_dsp
    PUBLIC
        reverb_project_options)

set_target_properties(reverb_dsp PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
