CPMAddPackage(
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 8.0.15
    GIT_SHALLOW YES
    EXCLUDE_FROM_ALL YES
)

juce_add_plugin(the89th_plugin
    PRODUCT_NAME                "THE89TH"
    COMPANY_NAME                "THE89TH"
    BUNDLE_ID                   "com.the89th.the89th"
    PLUGIN_MANUFACTURER_CODE    Lj89
    PLUGIN_CODE                 T89t
    FORMATS                     VST3
    VST3_CATEGORIES             Fx Delay
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
)

target_sources(the89th_plugin PRIVATE src/PluginProcessor.cpp)

target_compile_definitions(the89th_plugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    # Permitted under the GPLv3 branch of JUCE's licence, which is the basis
    # this project builds on: private and never distributed.
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(the89th_plugin
    PRIVATE
        the89th::core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
