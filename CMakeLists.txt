cmake_minimum_required(VERSION 3.25)

project(
    ReverbPlayground
    VERSION 0.1.0
    DESCRIPTION "A visual instrument for constructing and understanding algorithmic reverbs"
    LANGUAGES CXX)

set(REVERB_BUILD_COMMIT "" CACHE STRING "Source commit embedded in packaged artifacts")
execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE REVERB_DETECTED_BUILD_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT REVERB_DETECTED_BUILD_COMMIT STREQUAL "")
    # A Git checkout is authoritative. FORCE prevents a commit passed by a
    # previous packaging configure from leaking into later development builds.
    set(REVERB_BUILD_COMMIT "${REVERB_DETECTED_BUILD_COMMIT}" CACHE STRING
        "Source commit embedded in packaged artifacts" FORCE)
endif()
if(REVERB_BUILD_COMMIT STREQUAL "")
    set(REVERB_BUILD_COMMIT "unknown")
endif()
if(NOT REVERB_BUILD_COMMIT MATCHES "^[0-9A-Za-z._-]+$")
    message(FATAL_ERROR "REVERB_BUILD_COMMIT contains unsupported characters")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
include(FetchContent)

option(REVERB_BUILD_APP "Build the standalone application and plugin" ON)
option(REVERB_BUILD_TESTS "Build native tests" ${BUILD_TESTING})

if(WIN32)
    FetchContent_Declare(
        MicrosoftWebView2
        URL https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.4078.44
        URL_HASH SHA256=dc4d1d9168df26b830398303e50210b6e1729f6ce5a7ac69d2c766852f489962
        SOURCE_DIR ${FETCHCONTENT_BASE_DIR}/Microsoft.Web.WebView2.1.0.4078.44)
    FetchContent_MakeAvailable(MicrosoftWebView2)
    set(JUCE_WEBVIEW2_PACKAGE_LOCATION ${FETCHCONTENT_BASE_DIR})
endif()

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.13
    GIT_SHALLOW TRUE
    PATCH_COMMAND
        ${CMAKE_COMMAND}
        -DJUCE_SOURCE_DIR=<SOURCE_DIR>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PatchJuceWebView2Bounds.cmake)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(nlohmann_json)

add_library(reverb_project_options INTERFACE)
target_compile_features(reverb_project_options INTERFACE cxx_std_20)
target_compile_definitions(reverb_project_options INTERFACE
    REVERB_PRODUCT_VERSION="${PROJECT_VERSION}"
    REVERB_BUILD_COMMIT="${REVERB_BUILD_COMMIT}")

if(MSVC)
    target_compile_options(reverb_project_options INTERFACE /W4 /WX /permissive- /Zc:__cplusplus)
else()
    target_compile_options(reverb_project_options INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

add_subdirectory(src/dsp)
add_subdirectory(src/graph)
add_subdirectory(src/render)
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
