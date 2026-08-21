add_library(reverb_audio STATIC
    Source/PreparedAudioFileSource.cpp)

target_include_directories(reverb_audio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include)

target_link_libraries(reverb_audio
    PUBLIC
        reverb_project_options
        juce::juce_audio_formats)

set_target_properties(reverb_audio PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
