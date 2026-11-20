# The Strange Drift plugin wrapper: what Logic sees (AU / VST3, a MIDI effect) and the panel.
# The chaos circuits and the CC stream live in ../engine.

# The Audio Unit format exists on macOS only; elsewhere (the Linux checking computer) only VST3 is built.
if(APPLE)
    set(STRANGEDRIFT_FORMATS AU VST3)
else()
    set(STRANGEDRIFT_FORMATS VST3)
endif()

# A MIDI effect (AU type aumi), set up like Ogdoad: no audio buses, MIDI in and MIDI out.
# These codes and the bundle id were chosen once and never change (saved projects find the plugin by them).
juce_add_plugin(StrangeDrift
    COMPANY_NAME                "particledots"
    BUNDLE_ID                   com.particledots.strangedrift
    PLUGIN_MANUFACTURER_CODE    Pdot
    PLUGIN_CODE                 Sdr1
    FORMATS                     ${STRANGEDRIFT_FORMATS}
    PRODUCT_NAME                "StrangeDrift"
    PLUGIN_NAME                 "Strange Drift"
    DESCRIPTION                 "Three slow chaos circuits as MIDI CC"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx
    AU_MAIN_TYPE                kAudioUnitType_MIDIProcessor
)

set(STRANGEDRIFT_PLUGIN_SOURCES
    Source/Parameters.h
    Source/Parameters.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

target_sources(StrangeDrift PRIVATE ${STRANGEDRIFT_PLUGIN_SOURCES})

target_compile_definitions(StrangeDrift PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(StrangeDrift
    PRIVATE
        strangedrift_engine
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Optional, off by default: a small program that draws the window to picture files and checks the controls, the saved
# state and the MIDI going in and out, without Logic. Build with -DSTRANGEDRIFT_BUILD_PREVIEW=ON.
option(STRANGEDRIFT_BUILD_PREVIEW "Build the offline window picture and plugin checks" OFF)
if(STRANGEDRIFT_BUILD_PREVIEW)
    juce_add_console_app(StrangeDriftPreview PRODUCT_NAME "StrangeDriftPreview")
    target_sources(StrangeDriftPreview PRIVATE preview/preview.cpp ${STRANGEDRIFT_PLUGIN_SOURCES})
    target_include_directories(StrangeDriftPreview PRIVATE Source)
    target_compile_definitions(StrangeDriftPreview PRIVATE
        JucePlugin_Name="Strange Drift"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(StrangeDriftPreview
        PRIVATE strangedrift_engine juce::juce_audio_processors juce::juce_gui_basics
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
