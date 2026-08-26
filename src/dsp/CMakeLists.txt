add_library(reverb_dsp STATIC
    Source/Allpass.cpp
    Source/BarrReference.cpp
    Source/BarrReferenceRuntime.cpp
    Source/BlockKernels.cpp
    Source/Delay.cpp
    Source/EnergyTelemetry.cpp
    Source/EnvelopeFollower.cpp
    Source/Gain.cpp
    Source/HoldGate.cpp
    Source/ImpulseCapture.cpp
    Source/LiveReferenceHarness.cpp
    Source/NumericalSafetyGuard.cpp
    Source/OnePoleLowPass.cpp
    Source/PitchShift.cpp
    Source/RuntimeDiagnostics.cpp
    Source/Sum.cpp)

target_include_directories(reverb_dsp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include)

target_link_libraries(reverb_dsp
    PUBLIC
        reverb_project_options)

set_target_properties(reverb_dsp PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
