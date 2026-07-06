cmake_minimum_required(VERSION 3.20)
project(ADM-OSC_Music-Panner VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-specific plugin formats and toolchain settings.
if(APPLE)
    enable_language(OBJC OBJCXX)

    # Universal macOS (Arm + Intel)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary" FORCE)
endif()

# 🔴 Désactiver juceaide & extras (plus stable sur macOS 15)
set(JUCE_ENABLE_JUCEAIDE OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS   OFF CACHE BOOL "" FORCE)

# === JUCE via FetchContent (comme ton autre projet) ===
include(FetchContent)
FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.8      # même version qui marche chez toi
)
FetchContent_MakeAvailable(juce)

if(APPLE)
    juce_add_plugin(ADM_OSC_Music_Panner
        COMPANY_NAME "Pierrot's Tools"
        BUNDLE_ID    com.Pierrot.adm_osc_music_panner
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES "Fx"
        AU_MAIN_TYPE kAudioUnitType_Effect
        COPY_PLUGIN_AFTER_BUILD TRUE
        FORMATS VST3 AU Standalone
        PRODUCT_NAME "ADM-OSC Panner"
    )
else()
    juce_add_plugin(ADM_OSC_Music_Panner
        COMPANY_NAME "Pierrot's Tools"
        BUNDLE_ID    com.Pierrot.adm_osc_music_panner
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES "Fx"
        COPY_PLUGIN_AFTER_BUILD FALSE
        FORMATS VST3
        PRODUCT_NAME "ADM-OSC Panner"
    )
endif()

target_sources(ADM_OSC_Music_Panner PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(ADM_OSC_Music_Panner PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ADM_OSC_Music_Panner,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=1
)

target_link_libraries(ADM_OSC_Music_Panner PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_osc
    juce::juce_core
)

# Artefacts dans un dossier clair (comme ton autre projet)
set_target_properties(ADM_OSC_Music_Panner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ADM_OSC_Music_Panner_artefacts/$<CONFIG>"
)
