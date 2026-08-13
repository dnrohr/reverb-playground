set(webview_source
    "${JUCE_SOURCE_DIR}/modules/juce_gui_extra/native/juce_WebBrowserComponent_windows.cpp")

if(NOT EXISTS "${webview_source}")
    message(FATAL_ERROR "JUCE WebView2 source was not found at ${webview_source}")
endif()

file(READ "${webview_source}" contents)

set(required_code [=[
           #if JUCE_WIN_PER_MONITOR_DPI_AWARE
            if (auto* peer = owner.getTopLevelComponent()->getPeer())
                newBounds = (newBounds.toDouble() * peer->getPlatformScaleFactor()).toNearestInt();
           #endif
]=])
set(obsolete_patch [=[
            // getAreaCoveredBy already supplies WebView2 controller coordinates. Scaling these
            // bounds again makes the child larger than its JUCE component on HiDPI displays.
]=])

string(FIND "${contents}" "${required_code}" already_correct)
if(NOT already_correct EQUAL -1)
    message(STATUS "JUCE WebView2 physical-pixel bounds scaling is present")
    return()
endif()

string(FIND "${contents}" "${obsolete_patch}" obsolete_patch_location)
if(obsolete_patch_location EQUAL -1)
    message(FATAL_ERROR
        "JUCE WebView2 bounds implementation changed; review physical-pixel scaling before updating JUCE")
endif()

string(REPLACE "${obsolete_patch}" "${required_code}" contents "${contents}")
file(WRITE "${webview_source}" "${contents}")
message(STATUS "Restored JUCE WebView2 physical-pixel bounds scaling")
