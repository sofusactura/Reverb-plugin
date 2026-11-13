cmake_minimum_required(VERSION 3.22)

# These must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple Silicon + Intel)")
# Link the MSVC runtime statically so the plugin needs no VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(HybridGateReverb VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

option(HGR_COPY_AFTER_BUILD "Install the built plugins into the user's plugin folders" OFF)
option(HGR_BUILD_TESTS "Build the offline DSP test runner" ON)

# JUCE is fetched at configure time. To use a local checkout instead, pass
# -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

set(HGR_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND HGR_FORMATS AU)
elseif(UNIX)
    list(APPEND HGR_FORMATS LV2)
endif()

juce_add_plugin(HybridGateReverb
    PRODUCT_NAME                "HybridGate Reverb"
    COMPANY_NAME                "Actura"
    BUNDLE_ID                   "dk.actura.hybridgatereverb"
    PLUGIN_MANUFACTURER_CODE    Actu
    PLUGIN_CODE                 HyGR
    FORMATS                     ${HGR_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Reverb
    AU_MAIN_TYPE                kAudioUnitType_Effect
    LV2URI                      "https://actura.dk/plugins/hybridgatereverb"
    COPY_PLUGIN_AFTER_BUILD     ${HGR_COPY_AFTER_BUILD})

target_sources(HybridGateReverb PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ConvolutionEngine.cpp
    src/dsp/IRGenerator.cpp)

target_include_directories(HybridGateReverb PUBLIC src)

target_compile_definitions(HybridGateReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(HybridGateReverb
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

if(HGR_BUILD_TESTS)
    # The test runner reuses the plugin's already-compiled code (and JUCE) rather than building it twice.
    enable_testing()
    add_executable(HybridGateReverbTests tests/OfflineTests.cpp)
    target_link_libraries(HybridGateReverbTests PRIVATE HybridGateReverb)
    target_include_directories(HybridGateReverbTests PRIVATE
        $<TARGET_PROPERTY:HybridGateReverb,INCLUDE_DIRECTORIES>)
    target_compile_definitions(HybridGateReverbTests PRIVATE
        $<TARGET_PROPERTY:HybridGateReverb,COMPILE_DEFINITIONS>)
    add_test(NAME OfflineTests COMMAND HybridGateReverbTests)
endif()
