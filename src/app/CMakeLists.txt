juce_add_plugin(ReverbPlayground
    COMPANY_NAME "Reverb Playground"
    BUNDLE_ID "org.reverbplayground.app"
    PLUGIN_MANUFACTURER_CODE RvPg
    PLUGIN_CODE Rv01
    NEEDS_WEBVIEW2 TRUE
    FORMATS Standalone VST3
    PRODUCT_NAME "Reverb Playground"
    VST3_CATEGORIES Fx Reverb)

juce_generate_juce_header(ReverbPlayground)

target_sources(ReverbPlayground
    PRIVATE
        PluginEditor.cpp
        PluginEditor.h
        PluginProcessor.cpp
        PluginProcessor.h)

target_compile_definitions(ReverbPlayground
    PUBLIC
        JUCE_REPORT_APP_USAGE=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=0)

target_link_libraries(ReverbPlayground
    PRIVATE
        reverb_project_options
        reverb_dsp
        reverb_graph
        reverb_ui
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
