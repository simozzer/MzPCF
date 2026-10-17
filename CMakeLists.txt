cmake_minimum_required(VERSION 3.22)

project(MzPCF VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# JUCE lives beside the repository rather than inside it, shared with every other JUCE
# project on the machine. Override with -DJUCE_PATH=... if it is somewhere else.
#
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Where JUCE is checked out")

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR
        "No JUCE at ${JUCE_PATH}.\n"
        "  cd ${CMAKE_CURRENT_SOURCE_DIR}/..\n"
        "  git clone --depth 1 https://github.com/juce-framework/JUCE\n"
        "or pass -DJUCE_PATH=<wherever it is>.")
endif()

add_subdirectory("${JUCE_PATH}" JUCE)

#
# The per-user VST3 folder, which needs no elevation - JUCE's default of
# C:\Program Files\Common Files\VST3 cannot be written without running as administrator.
#
if(WIN32)
    set(VST3_DEFAULT "$ENV{LOCALAPPDATA}/Programs/Common/VST3")
elseif(APPLE)
    set(VST3_DEFAULT "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
else()
    set(VST3_DEFAULT "$ENV{HOME}/.vst3")
endif()

set(VST3_DESTINATION "${VST3_DEFAULT}" CACHE PATH "Where to install the built VST3")

set(MZPCF_FORMATS VST3)
if(APPLE)
    list(APPEND MZPCF_FORMATS AU)
endif()

#
# PRODUCT_NAME is what a player sees. "MzPCF" rather than anything with ReBirth in it:
# ReBirth is Reason Studios' trademark, and this is an independent emulation.
#
# The two codes are the plugin's IDENTITY and must never change once anyone has saved a
# project with it - a VST3's class ID is made from them, and a host finds the plugin in a
# saved set by that ID.
#
juce_add_plugin(MzPCF
    COMPANY_NAME                "simozzer"
    COMPANY_COPYRIGHT           "Created by Simon Moscrop"
    PRODUCT_NAME                "MzPCF"
    PLUGIN_MANUFACTURER_CODE    Smoz          # never change - see above
    PLUGIN_CODE                 Pcf1          # never change - see above

    # JUCE does not inherit this from project().
    VERSION                     "${PROJECT_VERSION}"

    VST3_COPY_DIR               "${VST3_DESTINATION}"
    VST3_CATEGORIES             Fx Filter

    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # Installed by install_vst3.cmake instead, so a host holding the old binary open is a
    # message rather than a failed build.
    COPY_PLUGIN_AFTER_BUILD     FALSE

    FORMATS                     ${MZPCF_FORMATS})

target_sources(MzPCF PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/Look.cpp
    Source/PCF/Pcf.cpp)

target_include_directories(MzPCF PRIVATE Source)

add_custom_command(TARGET MzPCF_VST3 POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
            "-DSRC=${CMAKE_CURRENT_BINARY_DIR}/MzPCF_artefacts/$<CONFIG>/VST3/MzPCF.vst3"
            "-DDST=${VST3_DESTINATION}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/install_vst3.cmake"
    VERBATIM)

target_compile_definitions(MzPCF PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MzPCF
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#
# The engine's own check. It links no JUCE: the filter and its clock are plain C++, so they
# can be held to the grid, the response and stability before a plugin is wrapped round them.
#
add_executable(PcfCheck
    Tests/PcfCheck.cpp
    Source/PCF/Pcf.cpp)

target_include_directories(PcfCheck PRIVATE Source/PCF)

enable_testing()
add_test(NAME pcf COMMAND PcfCheck)
