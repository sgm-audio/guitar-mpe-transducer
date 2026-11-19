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
option(BUILD_GUI_PREVIEW "Build headless 2x GuiPreview snapshot renderer" ON)

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

# Offline CSV test harness
add_executable(transducer_offline_harness
    tools/offline_csv_harness.cpp
)
target_include_directories(transducer_offline_harness PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(transducer_offline_harness PRIVATE cxx_std_20)

# Tests
add_subdirectory(tests)

if(TRANSDUCER_BUILD_JUCE)
    juce_add_binary_data(GuitarMpeUiAssets
        SOURCES
            assets/ui/chassis_frame.png
            assets/ui/lava_rock_tile.png
            assets/ui/knob_cap.png
            assets/ui/combobox_bar.png
            assets/ui/active_tab_lava.png
            assets/ui/bronze_header_strip.png
    )

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
            JIVE_IS_PLUGIN_PROJECT=1
    )

    target_link_libraries(guitar_mpe_transducer_juce
        PRIVATE
            GuitarMpeUiAssets
            jive::jive_layouts
            jive::jive_style_sheets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

    if(TRANSDUCER_BUILD_JUCE_CLAP)
        clap_juce_extensions_plugin(
            TARGET guitar_mpe_transducer_juce
            CLAP_ID "com.sgm-audio.guitar-mpe-transducer.juce"
            CLAP_FEATURES audio-effect note-effect utility
        )
    endif()

    if(BUILD_GUI_PREVIEW)
        juce_add_console_app(GuiPreview
            PRODUCT_NAME "GuiPreview"
        )

        target_sources(GuiPreview
            PRIVATE
                tools/gui_preview_main.cpp
                src/juce/PluginProcessor.cpp
                src/juce/PluginEditor.cpp
        )

        target_include_directories(GuiPreview
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

        target_compile_definitions(GuiPreview
            PRIVATE
                JUCE_WEB_BROWSER=0
                JUCE_USE_CURL=0
                JUCE_MODAL_LOOPS_PERMITTED=1
                JIVE_IS_PLUGIN_PROJECT=1
                JucePlugin_Name="Guitar MPE Transducer"
        )

        target_link_libraries(GuiPreview
            PRIVATE
                GuitarMpeUiAssets
                jive::jive_layouts
                jive::jive_style_sheets
                juce::juce_audio_processors
                juce::juce_dsp
                juce::juce_gui_basics
                juce::juce_recommended_config_flags
        )
    endif()
endif()
