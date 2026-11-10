cmake_minimum_required(VERSION 3.22)
# the version: one line in VERSION (the plugin, the installers, the site and the updater read it)
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION IMPAKT_VERSION LIMIT_COUNT 1)
# macOS 11 at least (Apple Silicon); the release builds are universal (arm64 + x86_64: -DCMAKE_OSX_ARCHITECTURES)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Oldest macOS the plugin runs on")
project(IMPAKT VERSION ${IMPAKT_VERSION} LANGUAGES C CXX)

# the release builds (the CI) leave the system's plugin folders alone: their installers put the plugins there
option(IMPAKT_COPY_AFTER_BUILD "Copy the plugins into the system's plugin folders after a build" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: pass -DJUCE_PATH=/path/to/JUCE to use a local copy, otherwise it is downloaded.
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.3
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# Plugin formats per platform. AUv3 needs the Xcode generator.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IMPAKT_FORMATS AUv3 Standalone)
elseif(APPLE)
    # On the Mac the AU covers every host. A Mac AUv3 would register a second component with the same
    # codes, and some hosts (Ableton Live) then load it in-process on a background thread.
    option(IMPAKT_MAC_AUV3 "Also build the AUv3 on macOS (Xcode generator only)" OFF)
    if(IMPAKT_MAC_AUV3 AND CMAKE_GENERATOR STREQUAL "Xcode")
        set(IMPAKT_FORMATS VST3 AU AUv3 Standalone)
    else()
        set(IMPAKT_FORMATS VST3 AU Standalone)
    endif()
else()
    set(IMPAKT_FORMATS VST3 Standalone)
endif()

juce_add_plugin(IMPAKT
    PRODUCT_NAME "IMPAKT"
    ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/../brand/impakt-icon-1024.png
    ICON_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/../brand/impakt-icon-256.png
    COMPANY_NAME "Impakt Audio"
    BUNDLE_ID "com.impaktaudio.impakt"
    PLUGIN_MANUFACTURER_CODE Impk
    PLUGIN_CODE Imp8
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${IMPAKT_COPY_AFTER_BUILD}
    NEEDS_WEB_BROWSER TRUE
    NEEDS_CURL TRUE
    NEEDS_WEBVIEW2 TRUE
    BACKGROUND_AUDIO_ENABLED TRUE
    REQUIRES_FULL_SCREEN FALSE
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationPortrait UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationPortrait UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Drum
    FORMATS ${IMPAKT_FORMATS})

juce_add_binary_data(IMPAKT_UI SOURCES Source/ui/index.html)

# Mutable Instruments code (core/mi, MIT): the MACRO machine (Plaits) and the GRAIN reverb (Beads)
file(GLOB_RECURSE IMPAKT_MI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/mi/plaits/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/mi/beads/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/mi/warps/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/mi/stmlib/*.cc)
list(FILTER IMPAKT_MI_SOURCES EXCLUDE REGEX ".*/atan\\.cc$")
# the MI sources are third-party code: no warnings-as-errors fuss on them
set_source_files_properties(${IMPAKT_MI_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")

target_sources(IMPAKT PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/RadioNet.cpp
    Source/Updater.cpp
    ../core/sk_engine.cpp
    ../core/sk_kit.cpp
    ../core/sk_mod.cpp
    ../core/sk_glitch.cpp
    ../core/sk_halo.cpp
    ../core/sk_strum.cpp
    ../core/sk_choir.cpp
    ../core/sk_alloy.cpp
    ../core/sk_chimera.cpp
    ../core/sk_cube.cpp
    ../core/sk_west.cpp
    ../core/sk_howl.cpp
    ../core/sk_mantra.cpp
    ../core/sk_radio.cpp
    ../core/sk_radio_dec.cpp
    ../core/sk_stencil.cpp
    ../core/sk_spring.cpp
    ../core/sk_weft.cpp
    ../core/sk_bank.cpp
    ../core/sk_break.cpp
    ../core/sk_fx.cpp
    ../core/sk_fuzz.cpp
    ../core/sk_send.cpp
    ../core/sk_box.cpp
    ../core/sk_spec.cpp
    ../core/sk_org.cpp
    ../core/sk_master.cpp
    ../core/sk_rev.cpp
    ../core/sk_vintage.cpp
    ../core/sk_mix.cpp
    ../core/sk_macro.cpp
    ../core/sk_grain.cpp
    ../core/sk_ott.cpp
    ../core/sk_pat.cpp
    ../core/sk_lfo_tables.cpp
    ${IMPAKT_MI_SOURCES})

target_include_directories(IMPAKT PRIVATE ../core ../core/mi)

# CHIMERA's bank (v66: bank/, made by tools/make_bank.py) where the processor reads it (PluginProcessor.cpp bankFolder):
# on the Mac in each bundle (Contents/Resources/bank); on Windows and Linux in the VST3 bundle (Contents/Resources/bank,
# beside Contents/<arch>) and beside the standalone app's executable; elsewhere the user's IMPAKT/bank folder or $IMPAKT_BANK
set(IMPAKT_BANK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bank)
if(EXISTS ${IMPAKT_BANK_DIR}/chimera.idx)
    if(APPLE)
        foreach(fmt IN LISTS IMPAKT_FORMATS)
            if(TARGET IMPAKT_${fmt})
                juce_add_bundle_resources_directory(IMPAKT_${fmt} ${IMPAKT_BANK_DIR})
            endif()
        endforeach()
    else()
        if(TARGET IMPAKT_VST3)
            get_target_property(impakt_vst3_bundle IMPAKT_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
            add_custom_command(TARGET IMPAKT_VST3 PRE_LINK
                COMMAND ${CMAKE_COMMAND} -E copy_directory ${IMPAKT_BANK_DIR} "${impakt_vst3_bundle}/Contents/Resources/bank"
                VERBATIM)
        endif()
        if(TARGET IMPAKT_Standalone)
            add_custom_command(TARGET IMPAKT_Standalone POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory ${IMPAKT_BANK_DIR} "$<TARGET_FILE_DIR:IMPAKT_Standalone>/bank"
                VERBATIM)
        endif()
    endif()
endif()

target_compile_definitions(IMPAKT PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# link-time optimisation (off for clang-cl on Windows, whose objects the Microsoft linker cannot read)
option(IMPAKT_LTO "Link-time optimisation" ON)
target_link_libraries(IMPAKT
    PRIVATE
        IMPAKT_UI
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
if(IMPAKT_LTO)
    target_link_libraries(IMPAKT PUBLIC juce::juce_recommended_lto_flags)
endif()
