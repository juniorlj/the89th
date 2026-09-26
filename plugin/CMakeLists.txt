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
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Fx Delay
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    # Installed by the in-place rsync below instead of JUCE's wipe-and-replace.
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

# Per-build stamp: regenerate every build, then force Version.cpp to recompile
# so the banner in the editor always matches the binary on disk.
set(_stamp_h "${CMAKE_CURRENT_BINARY_DIR}/BuildStamp.h")
add_custom_target(the89th_build_stamp
    COMMAND ${CMAKE_COMMAND}
        -DTHE89TH_VERSION=${PROJECT_VERSION}
        -DTHE89TH_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DSTAMP_OUT=${_stamp_h}
        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateBuildStamp.cmake
    BYPRODUCTS ${_stamp_h}
    COMMENT "Generating THE89TH build stamp"
    VERBATIM
)

set_source_files_properties(src/Version.cpp PROPERTIES OBJECT_DEPENDS "${_stamp_h}")

target_sources(the89th_plugin PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/Presets.cpp
    src/Version.cpp
    src/gui/Theme.cpp
    src/gui/Controls.cpp
    src/gui/MemoryRing.cpp
    src/gui/PresetBar.cpp
)

target_include_directories(the89th_plugin PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(the89th_plugin the89th_build_stamp)

target_compile_definitions(the89th_plugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
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

# Install without deleting the destination bundle first, so the bundle path stays
# stable. rsync writes a new file and renames it over the old one, which is safe
# while a host has the previous build loaded.
#
# A running host cannot pick up the new build. Measured: once macOS has loaded
# this image into a process it stays resident after the host unloads it, and a
# later load returns the old image even though the file on disk has a new UUID.
# Rescan, "Reload plugin" and remove/re-add all hand back the first build.
# Quit and reopen the host, or iterate DSP in the Standalone (run_the89th).
if(APPLE)
    set(_vst3_install_dir "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/THE89TH.vst3")
    add_custom_command(TARGET the89th_plugin_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DSRC=$<TARGET_BUNDLE_DIR:the89th_plugin_VST3>"
            "-DDEST=${_vst3_install_dir}"
            -P "${CMAKE_SOURCE_DIR}/cmake/InstallVst3InPlace.cmake"
        COMMAND ${CMAKE_COMMAND} -E echo
            "THE89TH: installed. A running host keeps the build it already loaded: quit and reopen FL to hear this one. Check the build stamp in the editor."
        VERBATIM
    )
endif()

# Optional host-free loop: rebuild and restart the Standalone app. macOS only:
# it opens the app bundle.
if(APPLE)
    add_custom_target(run_the89th
        DEPENDS the89th_plugin_Standalone
        COMMAND ${CMAKE_COMMAND} -E echo "Restarting THE89TH standalone..."
        COMMAND sh -c "pkill -f 'THE89TH.app/Contents/MacOS/THE89TH' 2>/dev/null || true"
        COMMAND ${CMAKE_COMMAND} -E sleep 0.2
        COMMAND open "$<TARGET_BUNDLE_DIR:the89th_plugin_Standalone>"
        VERBATIM
        USES_TERMINAL
    )
endif()

# The generic editor drives the AudioProcessor directly, so exercising that
# class is exercising what the controls actually do. Links the plugin's shared
# code rather than going through the VST3 wrapper.
if(THE89TH_BUILD_TESTS)
    add_executable(the89th_plugin_tests tests/test_plugin.cpp)

    # Needed to pump the message thread so AsyncUpdater callbacks land, which is
    # how Init hands off from whichever thread moved the control.
    target_compile_definitions(the89th_plugin_tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

    target_link_libraries(the89th_plugin_tests PRIVATE
        the89th_plugin
        the89th::core
        juce::juce_audio_utils
        Catch2::Catch2WithMain
        juce::juce_recommended_config_flags
    )

    catch_discover_tests(the89th_plugin_tests)
endif()

# Renders the editor to a PNG without a host, so the GUI can be checked on
# every build without quitting and reopening FL.
#   the89th-snapshot out.png [width] [preset]
add_executable(the89th_snapshot tools/Snapshot.cpp)
target_link_libraries(the89th_snapshot PRIVATE
    the89th_plugin
    the89th::core
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
)
set_target_properties(the89th_snapshot PROPERTIES OUTPUT_NAME the89th-snapshot)
