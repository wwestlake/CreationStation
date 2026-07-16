cmake_minimum_required(VERSION 3.22)

project(CreativeWorkstation VERSION 0.1.2 LANGUAGES CXX)

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
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/CreationStationIconBig.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/CreationStationIconSmall.png"
)

juce_add_plugin(CreationStationInstrument
    COMPANY_NAME "Creation Station"
    PRODUCT_NAME "Creation Station Instrument"
    BUNDLE_ID "com.creationstation.instrument"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE CrSt
    PLUGIN_CODE CSIn
    FORMATS VST3 Standalone
)

juce_generate_juce_header(CreativeWorkstation)
juce_generate_juce_header(CreationStationInstrument)

target_sources(CreativeWorkstation PRIVATE
    Source/Audio/WorkstationAudioEngine.cpp
    Source/Audio/WorkstationAudioEngine.h
    Source/Audio/PatchRuntimePlayer.cpp
    Source/Audio/PatchRuntimePlayer.h
    Source/Audio/SignalGraphRuntime.cpp
    Source/Audio/SignalGraphRuntime.h
    Source/Auth/DesktopAuthSession.cpp
    Source/Auth/DesktopAuthSession.h
    Source/Branding.cpp
    Source/Branding.h
    Source/ControlSurface/XTouchControlSurface.cpp
    Source/ControlSurface/XTouchControlSurface.h
    Source/Content/ContentLibrary.cpp
    Source/Content/ContentLibrary.h
    Source/Content/ContentApiClient.cpp
    Source/Content/ContentApiClient.h
    Source/Project/ProjectManager.cpp
    Source/Project/ProjectManager.h
    Source/Patch/PatchModel.cpp
    Source/Patch/PatchModel.h
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
    Source/Language/DslCompiler.cpp
    Source/Language/DslCompiler.h
    Source/Language/PatinaLexer.cpp
    Source/Language/PatinaLexer.h
    Source/Language/PatinaParser.cpp
    Source/Language/PatinaParser.h
    Source/Language/PatinaSurfaceAst.h
    Source/Language/PatinaBuiltins.cpp
    Source/Language/PatinaBuiltins.h
    Source/Language/PatinaBinder.cpp
    Source/Language/PatinaBinder.h
    Source/Language/PatinaArtifactWriter.cpp
    Source/Language/PatinaArtifactWriter.h
    Source/Language/PatinaIr.cpp
    Source/Language/PatinaIr.h
    Source/Language/PatinaLowering.cpp
    Source/Language/PatinaLowering.h
    Source/Language/SignalGraph.cpp
    Source/Language/SignalGraph.h
    Source/Views/AuthGateView.cpp
    Source/Views/AuthGateView.h
    Source/Views/ArrangeView.cpp
    Source/Views/ArrangeView.h
    Source/Views/AiPanel.cpp
    Source/Views/AiPanel.h
    Source/Views/ContentPanel.cpp
    Source/Views/ContentPanel.h
    Source/Views/DslPanel.cpp
    Source/Views/DslPanel.h
    Source/Views/GraphPanel.cpp
    Source/Views/GraphPanel.h
    Source/Views/TourGuideOverlay.cpp
    Source/Views/TourGuideOverlay.h
    Source/Views/MixerPanel.cpp
    Source/Views/MixerPanel.h
    Source/Views/RecordView.cpp
    Source/Views/RecordView.h
    Source/Views/SignalLabPanel.cpp
    Source/Views/SignalLabPanel.h
)

target_sources(CreationStationInstrument PRIVATE
    Source/Plugins/CreationStationInstrumentProcessor.cpp
    Source/Plugins/CreationStationInstrumentProcessor.h
    Source/Patch/PatchModel.cpp
    Source/Patch/PatchModel.h
)

target_compile_definitions(CreativeWorkstation PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CreativeWorkstation,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CreativeWorkstation,JUCE_VERSION>"
)

target_compile_definitions(CreationStationInstrument PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CreationStationInstrument,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CreationStationInstrument,JUCE_VERSION>"
)

target_link_libraries(CreativeWorkstation PRIVATE
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_link_libraries(CreationStationInstrument PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

install(TARGETS CreativeWorkstation
    RUNTIME DESTINATION . COMPONENT CreativeWorkstation
    RESOURCE DESTINATION . COMPONENT CreativeWorkstation
)
install(FILES README.md LICENSE DESTINATION . COMPONENT CreativeWorkstation)

set(CPACK_PACKAGE_NAME "CreativeWorkstation")
set(CPACK_PACKAGE_VENDOR "Creation Station")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Creative Workstation")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-win64")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Creative Workstation")
set(CPACK_GENERATOR "ZIP;WIX")
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_WIX_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL CreativeWorkstation)
set(CPACK_WIX_UPGRADE_GUID "9F39E8E4-3B66-4EE8-8B7E-8E9D9B7F35D2")
set(CPACK_STRIP_FILES TRUE)

include(CPack)
