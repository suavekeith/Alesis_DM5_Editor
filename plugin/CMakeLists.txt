cmake_minimum_required(VERSION 3.22)
project(DM5EditorPlugin VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(DM5Editor
    COMPANY_NAME "DM5 Editor"
    BUNDLE_ID "com.dm5editor.plugin"
    PLUGIN_MANUFACTURER_CODE Kwln
    PLUGIN_CODE Dm5e
    FORMATS AU Standalone
    PRODUCT_NAME "DM5 Editor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    NEEDS_WEB_BROWSER TRUE
)

target_compile_definitions(DM5Editor
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_sources(DM5Editor
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_include_directories(DM5Editor
    PRIVATE
        Source
)

juce_generate_juce_header(DM5Editor)

target_link_libraries(DM5Editor
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
