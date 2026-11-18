cmake_minimum_required(VERSION 3.22)

# macOS settings MUST come before project(): set afterwards, CMake ignores them and the plugin
# ends up requiring the macOS version of the build machine.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")

# Windows: link the MSVC runtime statically (no VC++ Redistributable needed on the user's PC)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime")

project(AuroraTrigger VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE: downloaded automatically, or a local copy with -DAURORA_JUCE_DIR=/path/to/JUCE
# ---------------------------------------------------------------------------
set(AURORA_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")

if(AURORA_JUCE_DIR)
    add_subdirectory(${AURORA_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# AU only exists on macOS
set(AURORA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND AURORA_FORMATS AU)
endif()

juce_add_plugin(AuroraTrigger
    COMPANY_NAME "Aurora Audio"
    BUNDLE_ID "com.auroraaudio.auroratrigger"
    PLUGIN_MANUFACTURER_CODE Auro
    PLUGIN_CODE AuTr
    PRODUCT_NAME "Aurora Trigger"
    FORMATS ${AURORA_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_MusicEffect)

target_sources(AuroraTrigger
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(AuroraTrigger
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(AuroraTrigger PRIVATE /utf-8)
endif()

target_link_libraries(AuroraTrigger
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests:  cmake --build build --target AuroraTriggerPluginLoadTest
#   Loads the INSTALLED plugin like a DAW does (VST3, and AU on macOS)
# ---------------------------------------------------------------------------
option(AURORA_BUILD_TESTS "Build the plugin load test" ON)

if(AURORA_BUILD_TESTS)
    juce_add_console_app(AuroraTriggerPluginLoadTest PRODUCT_NAME "AuroraTriggerPluginLoadTest")
    target_sources(AuroraTriggerPluginLoadTest PRIVATE Tests/PluginLoadTest.cpp)
    target_compile_definitions(AuroraTriggerPluginLoadTest PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(AuroraTriggerPluginLoadTest PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
endif()
