cmake_minimum_required(VERSION 3.25)

project(
    ReverbPlayground
    VERSION 0.1.0
    DESCRIPTION "A visual instrument for constructing and understanding algorithmic reverbs"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
include(FetchContent)

option(REVERB_BUILD_APP "Build the standalone application and plugin" ON)
option(REVERB_BUILD_TESTS "Build native tests" ${BUILD_TESTING})

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.13
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

add_library(reverb_project_options INTERFACE)
target_compile_features(reverb_project_options INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(reverb_project_options INTERFACE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(reverb_project_options INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src/dsp)
add_subdirectory(src/graph)
add_subdirectory(src/ui)

if(REVERB_BUILD_APP)
    add_subdirectory(src/app)
endif()

if(REVERB_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.15.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
    add_subdirectory(tests)
endif()
