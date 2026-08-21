juce_add_binary_data(reverb_editor_assets
    SOURCES
        WebAssets/index.html
        WebAssets/editor.css
        WebAssets/editor.js)

if(WIN32)
    list(APPEND CMAKE_MODULE_PATH "${JUCE_SOURCE_DIR}/extras/Build/CMake")
    find_package(WebView2 REQUIRED)
endif()

add_library(reverb_ui STATIC
    Source/EditorShell.cpp)

target_include_directories(reverb_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include)

target_compile_definitions(reverb_ui
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=0)

target_link_libraries(reverb_ui
    PUBLIC
        reverb_project_options
        reverb_graph
        reverb_editor_assets
        $<$<PLATFORM_ID:Windows>:juce::juce_webview2>
        juce::juce_audio_utils
        juce::juce_gui_extra)

set_target_properties(reverb_ui PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
