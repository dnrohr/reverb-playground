add_library(reverb_graph STATIC
    Source/AcyclicRuntime.cpp
    Source/BarrReferenceGraph.cpp
    Source/ControlRate.cpp
    Source/ControlModulation.cpp
    Source/GraphDocument.cpp
    Source/GravityDiffusionGraph.cpp
    Source/HostPatchState.cpp
    Source/PatchJson.cpp
    Source/RuntimeSnapshot.cpp
    Source/SafeParallelShimmerGraph.cpp
    Source/SplitFeedbackShimmerGraph.cpp)

target_include_directories(reverb_graph
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include)

target_link_libraries(reverb_graph
    PUBLIC
        reverb_project_options
        reverb_dsp
        nlohmann_json::nlohmann_json)

set_target_properties(reverb_graph PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
