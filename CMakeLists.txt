cmake_minimum_required(VERSION 3.24)
project(guitar_mpe_transducer
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive- /wd4324)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(TRANSDUCER_BUILD_JUCE "Build JUCE VST3/AU/Standalone targets" OFF)
option(TRANSDUCER_BUILD_JUCE_CLAP "Also export CLAP via clap-juce-extensions" OFF)

# Fetch dependencies
include(cmake/Dependencies.cmake)

# Project include directory
include_directories(include)

# Transducer CLAP plugin library
add_library(guitar_mpe_transducer MODULE
    src/plugin.cpp
    src/plugin_entry.cpp
)

target_link_libraries(guitar_mpe_transducer
    PRIVATE
        clap
)

target_include_directories(guitar_mpe_transducer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(guitar_mpe_transducer PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
)

# Tests
add_subdirectory(tests)

if(TRANSDUCER_BUILD_JUCE)
    set(TRANSDUCER_JUCE_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND TRANSDUCER_JUCE_FORMATS AU)
    endif()

    juce_add_plugin(guitar_mpe_transducer_juce
        COMPANY_NAME "SGM Audio"
        BUNDLE_ID "com.sgm-audio.guitar-mpe-transducer"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE SgmA
        PLUGIN_CODE GtrM
        FORMATS ${TRANSDUCER_JUCE_FORMATS}
        PRODUCT_NAME "Guitar MPE Transducer"
    )

    target_sources(guitar_mpe_transducer_juce
        PRIVATE
            src/juce/PluginProcessor.cpp
            src/juce/PluginEditor.cpp
    )

    target_include_directories(guitar_mpe_transducer_juce
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_definitions(guitar_mpe_transducer_juce
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
    )

    target_link_libraries(guitar_mpe_transducer_juce
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            jive::jive_layouts
            jive::jive_style_sheets
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

    if(TRANSDUCER_BUILD_JUCE_CLAP)
        clap_juce_extensions_plugin(
            TARGET guitar_mpe_transducer_juce
            CLAP_ID "com.sgm-audio.guitar-mpe-transducer"
            CLAP_FEATURES audio-effect note-effect utility
        )
    endif()
endif()
