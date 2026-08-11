set(webview_source
    "${JUCE_SOURCE_DIR}/modules/juce_gui_extra/native/juce_WebBrowserComponent_windows.cpp")

if(NOT EXISTS "${webview_source}")
    message(FATAL_ERROR "JUCE WebView2 source was not found at ${webview_source}")
endif()

file(READ "${webview_source}" contents)

set(old_code [=[
           #if JUCE_WIN_PER_MONITOR_DPI_AWARE
            if (auto* peer = owner.getTopLevelComponent()->getPeer())
                newBounds = (newBounds.toDouble() * peer->getPlatformScaleFactor()).toNearestInt();
           #endif
]=])
set(new_code [=[
            // getAreaCoveredBy already supplies WebView2 controller coordinates. Scaling these
            // bounds again makes the child larger than its JUCE component on HiDPI displays.
]=])

string(FIND "${contents}" "${new_code}" already_patched)
if(NOT already_patched EQUAL -1)
    message(STATUS "JUCE WebView2 HiDPI bounds patch already applied")
    return()
endif()

string(FIND "${contents}" "${old_code}" patch_location)
if(patch_location EQUAL -1)
    message(FATAL_ERROR
        "JUCE WebView2 bounds implementation changed; review the HiDPI patch before updating JUCE")
endif()

string(REPLACE "${old_code}" "${new_code}" contents "${contents}")
file(WRITE "${webview_source}" "${contents}")
message(STATUS "Applied JUCE WebView2 HiDPI bounds patch")
