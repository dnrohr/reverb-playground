add_library(reverb_render STATIC
    Source/EnvelopeMeasurements.cpp
    Source/GravityReference.cpp
    Source/OfflineRenderer.cpp
    Source/PitchShiftValidation.cpp
    Source/ResponseMeasurements.cpp
    Source/SplitFeedbackShimmerValidation.cpp
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

add_executable(reverb_gravity_reference_cli
    Source/GravityReferenceCli.cpp)

target_link_libraries(reverb_gravity_reference_cli
    PRIVATE
        reverb_project_options
        reverb_render)

add_executable(reverb_pitch_shift_validation_cli
    Source/PitchShiftValidationCli.cpp)

target_link_libraries(reverb_pitch_shift_validation_cli
    PRIVATE
        reverb_project_options
        reverb_render)

add_executable(reverb_factory_patch_cli
    Source/FactoryPatchCli.cpp)

target_link_libraries(reverb_factory_patch_cli
    PRIVATE
        reverb_project_options
        reverb_graph)

add_executable(reverb_split_feedback_shimmer_validation_cli
    Source/SplitFeedbackShimmerValidationCli.cpp)

target_link_libraries(reverb_split_feedback_shimmer_validation_cli
    PRIVATE
        reverb_project_options
        reverb_render)
