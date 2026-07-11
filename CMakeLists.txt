cmake_minimum_required(VERSION 3.22)

project(CreativeWorkstation VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to your JUCE root directory before configuring.")
endif()

add_subdirectory("${JUCE_DIR}" juce)

juce_add_gui_app(CreativeWorkstation
    PRODUCT_NAME "Creative Workstation"
    COMPANY_NAME "Creation Station"
    BUNDLE_ID "com.creationstation.creativeworkstation"
)

juce_generate_juce_header(CreativeWorkstation)

target_sources(CreativeWorkstation PRIVATE
    Source/Audio/WorkstationAudioEngine.cpp
    Source/Audio/WorkstationAudioEngine.h
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
    Source/Language/DslCompiler.cpp
    Source/Language/DslCompiler.h
    Source/Views/AiPanel.cpp
    Source/Views/AiPanel.h
    Source/Views/DslPanel.cpp
    Source/Views/DslPanel.h
    Source/Views/GraphPanel.cpp
    Source/Views/GraphPanel.h
    Source/Views/MixerPanel.cpp
    Source/Views/MixerPanel.h
)

target_compile_definitions(CreativeWorkstation PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CreativeWorkstation,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CreativeWorkstation,JUCE_VERSION>"
)

target_link_libraries(CreativeWorkstation PRIVATE
    juce::juce_audio_devices
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
