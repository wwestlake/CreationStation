cmake_minimum_required(VERSION 3.22)

set(CREATION_STATION_VERSION "0.5.1" CACHE STRING "Creation Station product version")
project(CreativeWorkstation VERSION ${CREATION_STATION_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../shared/CMake/CreationSharedLLVM.cmake")
configure_creation_shared_llvm(CS "Creation Station")

# Keep full optimizations in Release but still emit PDBs, so a crash dump collected from a
# Release build can actually be resolved to real function names/line numbers instead of raw
# offsets into the executable.
if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Zi")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
endif()

if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to your JUCE root directory before configuring.")
endif()

option(CREATION_STATION_ENABLE_ASIO "Enable Steinberg ASIO support in Creation Station" OFF)
set(CREATION_STATION_ASIO_SDK_DIR "" CACHE PATH "Path to the Steinberg ASIO SDK folder containing iasiodrv.h")
set(CREATION_SUITE_SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../shared" CACHE PATH "Path to the Creation Suite shared code root")

add_subdirectory("${JUCE_DIR}" juce)

set(CS_ASSET_SYSTEM_TARGET creation_asset_system)
set(CS_SERVICES_TARGET "")
set(CS_UI_TARGET "")
if(EXISTS "${CREATION_SUITE_SHARED_DIR}/AssetSystem/CMakeLists.txt")
    add_subdirectory("${CREATION_SUITE_SHARED_DIR}/AssetSystem"
                     "${CMAKE_CURRENT_BINARY_DIR}/CreationSuiteSharedAssetSystem")
    set(CS_ASSET_SYSTEM_TARGET creation_suite_asset_system)
else()
    add_subdirectory(AssetSystem)
endif()

set(CS_VFS_TARGET "")
if(EXISTS "${CREATION_SUITE_SHARED_DIR}/VFS/CMakeLists.txt")
    add_subdirectory("${CREATION_SUITE_SHARED_DIR}/VFS"
                     "${CMAKE_CURRENT_BINARY_DIR}/CreationSuiteSharedVFS")
    set(CS_VFS_TARGET creation_suite_vfs)
endif()

set(CS_SERVICES_TARGET "")
if(EXISTS "${CREATION_SUITE_SHARED_DIR}/Services/CMakeLists.txt")
    add_subdirectory("${CREATION_SUITE_SHARED_DIR}/Services"
                     "${CMAKE_CURRENT_BINARY_DIR}/CreationSuiteSharedServices")
    set(CS_SERVICES_TARGET creation_suite_services)
endif()

set(CS_UI_TARGET "")
if(EXISTS "${CREATION_SUITE_SHARED_DIR}/UI/CMakeLists.txt")
    add_subdirectory("${CREATION_SUITE_SHARED_DIR}/UI"
                     "${CMAKE_CURRENT_BINARY_DIR}/CreationSuiteSharedUI")
    set(CS_UI_TARGET creation_suite_ui)
endif()

set(CS_INTEROP_TARGET "")
if(EXISTS "${CREATION_SUITE_SHARED_DIR}/Interop/CMakeLists.txt")
    add_subdirectory("${CREATION_SUITE_SHARED_DIR}/Interop"
                     "${CMAKE_CURRENT_BINARY_DIR}/CreationSuiteSharedInterop")
    set(CS_INTEROP_TARGET creation_suite_interop)
endif()

# Station's own scripting surface (the "Code" workspace tab) is CEL, the
# one suite-wide language -- not a bespoke per-app DSL. ce_lang_frontend
# (lexer/parser/sema/diagnostics) and node_system build with no LLVM
# dependency at all; only the JIT half of shared/CEL needs LLVM, and this
# panel only parses/validates source, it doesn't execute it.
if(NOT TARGET node_system)
    add_subdirectory("${CREATION_SUITE_SHARED_DIR}/NodeSystem" CreationSuiteSharedNodeSystem)
endif()
if(NOT TARGET ce_lang_frontend)
    add_subdirectory("${CREATION_SUITE_SHARED_DIR}/CEL" CreationSuiteSharedCel)
endif()

juce_add_gui_app(CreativeWorkstation
    PRODUCT_NAME "Creation Station"
    COMPANY_NAME "LagDaemon Software"
    BUNDLE_ID "com.lagdaemon.creationstation"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/CreationStationIconBig.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/CreationStationIconSmall.png"
)

# Mirrors the built executable into the workspace-level shared bin
# directory (D:\CreationSuite-Workspaces\claude-<config>-bin) so it's
# reachable at a stable path without needing OS symlink privileges.
# Every suite agent does this for its own repo -- see AGENTS.md.
add_custom_command(TARGET CreativeWorkstation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "D:/CreationSuite-Workspaces/claude-$<LOWER_CASE:$<CONFIG>>-bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:CreativeWorkstation>" "D:/CreationSuite-Workspaces/claude-$<LOWER_CASE:$<CONFIG>>-bin/"
    COMMENT "Copying CreativeWorkstation to the shared claude bin directory"
)

juce_add_plugin(CreationStationInstrument
    COMPANY_NAME "LagDaemon Software"
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

add_library(creation_station_language_host STATIC
    Source/Language/AppLanguagePolicy.cpp
    Source/Language/AppLanguagePolicy.h
)

target_include_directories(creation_station_language_host PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source/Language")

if(CS_ENABLE_SCRIPTING)
    target_include_directories(creation_station_language_host PRIVATE "${CS_LLVM_ROOT}/include")
    target_compile_definitions(creation_station_language_host PUBLIC CS_ENABLE_SCRIPTING=1)
else()
    target_compile_definitions(creation_station_language_host PUBLIC CS_ENABLE_SCRIPTING=0)
endif()

target_sources(CreativeWorkstation PRIVATE
    Source/AI/CreationStationContextEngine.cpp
    Source/AI/CreationStationContextEngine.h
    Source/AI/CreationStationContextStore.cpp
    Source/AI/CreationStationContextStore.h
    Source/AI/CreationStationAppManifest.cpp
    Source/AI/CreationStationAppManifest.h
    Source/AI/LiteSemRagApiClient.cpp
    Source/AI/LiteSemRagApiClient.h
    Source/AI/OpenAiModelCatalogClient.cpp
    Source/AI/OpenAiModelCatalogClient.h
    Source/AI/CreationStationTaskPlanner.cpp
    Source/AI/CreationStationTaskPlanner.h
    Source/Audio/WorkstationAudioEngine.cpp
    Source/Audio/WorkstationAudioEngine.h
    Source/Audio/StudioIOModel.cpp
    Source/Audio/StudioIOModel.h
    Source/Audio/PatchRuntimePlayer.cpp
    Source/Audio/PatchRuntimePlayer.h
    Source/Audio/SignalGraphRuntime.cpp
    Source/Audio/SignalGraphRuntime.h
    Source/Audio/VstPluginCatalog.cpp
    Source/Audio/VstPluginCatalog.h
    Source/Audio/SamplePackBuilder/PitchDetector.cpp
    Source/Audio/SamplePackBuilder/PitchDetector.h
    Source/Audio/SamplePackBuilder/SamplePackBuilderEngine.cpp
    Source/Audio/SamplePackBuilder/SamplePackBuilderEngine.h
    Source/Audio/SamplePackCatalog.cpp
    Source/Audio/SamplePackCatalog.h
    Source/Branding.cpp
    Source/Branding.h
    Source/ControlSurface/XTouchControlSurface.cpp
    Source/ControlSurface/XTouchControlSurface.h
    Source/ControlSurface/ControlSurfaceMappingStore.cpp
    Source/ControlSurface/ControlSurfaceMappingStore.h
    Source/Content/ContentLibrary.cpp
    Source/Content/ContentLibrary.h
    Source/Content/ContentApiClient.cpp
    Source/Content/ContentApiClient.h
    Source/Project/ProjectStorage.cpp
    Source/Project/ProjectStorage.h
    Source/Suite/SuiteSettings.h
    Source/Tutorial/GuidedTutorial.cpp
    Source/Tutorial/GuidedTutorial.h
    Source/Tutorial/TutorialScriptCompiler.cpp
    Source/Tutorial/TutorialScriptCompiler.h
    Source/Timeline/TimelineModel.cpp
    Source/Timeline/TimelineModel.h
    Source/Patch/PatchModel.cpp
    Source/Patch/PatchModel.h
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
    Source/Language/SignalGraph.cpp
    Source/Language/SignalGraph.h
    Source/Language/AudioNodeCatalog.cpp
    Source/Language/AudioNodeCatalog.h
    Source/Language/AudioGraphCodegen.cpp
    Source/Language/AudioGraphCodegen.h
    Source/Language/AudioGraphSelfTest.cpp
    Source/Language/AudioGraphSelfTest.h
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
    Source/Views/DockingWorkspace.cpp
    Source/Views/DockingWorkspace.h
    Source/Views/GraphPanel.cpp
    Source/Views/GraphPanel.h
    Source/Views/TourGuideOverlay.cpp
    Source/Views/TourGuideOverlay.h
    Source/Views/MidiEditorPanel.cpp
    Source/Views/MidiEditorPanel.h
    Source/Views/MixerPanel.cpp
    Source/Views/MixerPanel.h
    Source/Views/PluginBrowserList.cpp
    Source/Views/PluginBrowserList.h
    Source/Views/PluginsPanel.cpp
    Source/Views/PluginsPanel.h
    Source/Views/RecordView.cpp
    Source/Views/RecordView.h
    Source/Views/SettingsPanel.cpp
    Source/Views/SettingsPanel.h
    Source/Views/ScorePanel.cpp
    Source/Views/ScorePanel.h
    Source/Views/SignalLabPanel.cpp
    Source/Views/SignalLabPanel.h
    Source/Views/TrackerPanel.cpp
    Source/Views/TrackerPanel.h
    Source/Views/SamplePackBuilderPanel.cpp
    Source/Views/SamplePackBuilderPanel.h
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

if(CREATION_STATION_ENABLE_ASIO)
    if(NOT EXISTS "${CREATION_STATION_ASIO_SDK_DIR}/iasiodrv.h")
        message(FATAL_ERROR "CREATION_STATION_ENABLE_ASIO is ON, but iasiodrv.h was not found in CREATION_STATION_ASIO_SDK_DIR.")
    endif()

    target_compile_definitions(CreativeWorkstation PRIVATE JUCE_ASIO=1)
    target_include_directories(CreativeWorkstation PRIVATE "${CREATION_STATION_ASIO_SDK_DIR}")
endif()

target_compile_definitions(CreationStationInstrument PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CreationStationInstrument,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CreationStationInstrument,JUCE_VERSION>"
)

if(MSVC)
    target_compile_options(CreativeWorkstation PRIVATE /FS)
    target_compile_options(CreationStationInstrument PRIVATE /FS)
endif()

target_link_libraries(CreativeWorkstation PRIVATE
    ${CS_ASSET_SYSTEM_TARGET}
    ${CS_SERVICES_TARGET}
    ${CS_UI_TARGET}
    ${CS_INTEROP_TARGET}
    creation_station_language_host
    ce_lang_frontend
    ce_lang_jit
    node_system
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
    ${CS_ASSET_SYSTEM_TARGET}
    ${CS_SERVICES_TARGET}
    creation_station_language_host
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# --- Creation Station branded effect plugin suite ---------------------------

add_library(CreationStationPluginKit STATIC
    Source/PluginKit/CreationStationPluginLookAndFeel.cpp
    Source/PluginKit/CreationStationPluginLookAndFeel.h
    Source/PluginKit/CreationStationPluginHeader.cpp
    Source/PluginKit/CreationStationPluginHeader.h
    Source/PluginKit/LevelMeterComponent.cpp
    Source/PluginKit/LevelMeterComponent.h
    Source/PluginKit/ThresholdMeterComponent.cpp
    Source/PluginKit/ThresholdMeterComponent.h
    Source/PluginKit/EqCurveComponent.cpp
    Source/PluginKit/EqCurveComponent.h
    Source/PluginKit/CreationStationPluginPalette.h
    Source/Branding.cpp
    Source/Branding.h
)
target_link_libraries(CreationStationPluginKit PUBLIC
    juce::juce_gui_extra
    juce::juce_dsp
)

include(cmake/CreationStationPlugin.cmake)

add_creation_station_effect_plugin(CreationStationCompressor "Creation Station Compressor" CSCp "Fx")
target_sources(CreationStationCompressor PRIVATE
    Source/Plugins/Compressor/CreationStationCompressorProcessor.cpp
    Source/Plugins/Compressor/CreationStationCompressorProcessor.h
    Source/Plugins/Compressor/CreationStationCompressorEditor.cpp
    Source/Plugins/Compressor/CreationStationCompressorEditor.h
)

add_creation_station_effect_plugin(CreationStationLimiter "Creation Station Limiter" CSLm "Fx")
target_sources(CreationStationLimiter PRIVATE
    Source/Plugins/Limiter/CreationStationLimiterProcessor.cpp
    Source/Plugins/Limiter/CreationStationLimiterProcessor.h
    Source/Plugins/Limiter/CreationStationLimiterEditor.cpp
    Source/Plugins/Limiter/CreationStationLimiterEditor.h
)

add_creation_station_effect_plugin(CreationStationDeEsser "Creation Station De-Esser" CSDe "Fx")
target_sources(CreationStationDeEsser PRIVATE
    Source/Plugins/DeEsser/CreationStationDeEsserProcessor.cpp
    Source/Plugins/DeEsser/CreationStationDeEsserProcessor.h
    Source/Plugins/DeEsser/CreationStationDeEsserEditor.cpp
    Source/Plugins/DeEsser/CreationStationDeEsserEditor.h
)

add_creation_station_effect_plugin(CreationStationPhaser "Creation Station Phaser" CSPh "Fx")
target_sources(CreationStationPhaser PRIVATE
    Source/Plugins/Phaser/CreationStationPhaserProcessor.cpp
    Source/Plugins/Phaser/CreationStationPhaserProcessor.h
    Source/Plugins/Phaser/CreationStationPhaserEditor.cpp
    Source/Plugins/Phaser/CreationStationPhaserEditor.h
)

add_creation_station_effect_plugin(CreationStationChorus "Creation Station Chorus" CSCh "Fx")
target_sources(CreationStationChorus PRIVATE
    Source/Plugins/Chorus/CreationStationChorusProcessor.cpp
    Source/Plugins/Chorus/CreationStationChorusProcessor.h
    Source/Plugins/Chorus/CreationStationChorusEditor.cpp
    Source/Plugins/Chorus/CreationStationChorusEditor.h
)

add_creation_station_effect_plugin(CreationStationDelay "Creation Station Delay" CSDl "Fx")
target_sources(CreationStationDelay PRIVATE
    Source/Plugins/Delay/CreationStationDelayProcessor.cpp
    Source/Plugins/Delay/CreationStationDelayProcessor.h
    Source/Plugins/Delay/CreationStationDelayEditor.cpp
    Source/Plugins/Delay/CreationStationDelayEditor.h
)

add_creation_station_effect_plugin(CreationStationReverb "Creation Station Reverb" CSRv "Fx")
target_sources(CreationStationReverb PRIVATE
    Source/Plugins/Reverb/CreationStationReverbProcessor.cpp
    Source/Plugins/Reverb/CreationStationReverbProcessor.h
    Source/Plugins/Reverb/CreationStationReverbEditor.cpp
    Source/Plugins/Reverb/CreationStationReverbEditor.h
)

add_creation_station_effect_plugin(CreationStationOverdrive "Creation Station Overdrive" CSOd "Fx")
target_sources(CreationStationOverdrive PRIVATE
    Source/Plugins/Overdrive/CreationStationOverdriveProcessor.cpp
    Source/Plugins/Overdrive/CreationStationOverdriveProcessor.h
    Source/Plugins/Overdrive/CreationStationOverdriveEditor.cpp
    Source/Plugins/Overdrive/CreationStationOverdriveEditor.h
)

add_creation_station_effect_plugin(CreationStationClipper "Creation Station Clipper" CSCp "Fx|Distortion")
target_sources(CreationStationClipper PRIVATE
    Source/Plugins/Clipper/CreationStationClipperProcessor.cpp
    Source/Plugins/Clipper/CreationStationClipperProcessor.h
    Source/Plugins/Clipper/CreationStationClipperEditor.cpp
    Source/Plugins/Clipper/CreationStationClipperEditor.h
)

add_creation_station_effect_plugin(CreationStationGate "Creation Station Gate" CSGt "Fx")
target_sources(CreationStationGate PRIVATE
    Source/Plugins/Gate/CreationStationGateProcessor.cpp
    Source/Plugins/Gate/CreationStationGateProcessor.h
    Source/Plugins/Gate/CreationStationGateEditor.cpp
    Source/Plugins/Gate/CreationStationGateEditor.h
)

add_creation_station_effect_plugin(CreationStationParametricEQ "Creation Station Parametric EQ" CSEq "EQ")
target_sources(CreationStationParametricEQ PRIVATE
    Source/Plugins/ParametricEQ/CreationStationParametricEQProcessor.cpp
    Source/Plugins/ParametricEQ/CreationStationParametricEQProcessor.h
    Source/Plugins/ParametricEQ/CreationStationParametricEQEditor.cpp
    Source/Plugins/ParametricEQ/CreationStationParametricEQEditor.h
)

add_creation_station_effect_plugin(CreationStationVocalFocus "Creation Station Vocal Focus" CSVf "Fx")
target_sources(CreationStationVocalFocus PRIVATE
    Source/Plugins/VocalFocus/CreationStationVocalFocusProcessor.cpp
    Source/Plugins/VocalFocus/CreationStationVocalFocusProcessor.h
    Source/Plugins/VocalFocus/CreationStationVocalFocusEditor.cpp
    Source/Plugins/VocalFocus/CreationStationVocalFocusEditor.h
)

add_creation_station_effect_plugin(CreationStationTune "Creation Station Tune" CSTn "Fx|Pitch Shift")
target_sources(CreationStationTune PRIVATE
    Source/Plugins/Tune/CreationStationTuneProcessor.cpp
    Source/Plugins/Tune/CreationStationTuneProcessor.h
    Source/Plugins/Tune/CreationStationTuneEditor.cpp
    Source/Plugins/Tune/CreationStationTuneEditor.h
    Source/Audio/SamplePackBuilder/PitchDetector.cpp
    Source/Audio/SamplePackBuilder/PitchDetector.h
)

add_creation_station_effect_plugin(CreationStationDynamicEQ "Creation Station Dynamic EQ" CSDq "Fx|EQ")
target_sources(CreationStationDynamicEQ PRIVATE
    Source/Plugins/DynamicEQ/CreationStationDynamicEQProcessor.cpp
    Source/Plugins/DynamicEQ/CreationStationDynamicEQProcessor.h
    Source/Plugins/DynamicEQ/CreationStationDynamicEQEditor.cpp
    Source/Plugins/DynamicEQ/CreationStationDynamicEQEditor.h
)

add_creation_station_effect_plugin(CreationStationResonanceSuppressor "Creation Station Resonance Suppressor" CSRs "Fx|EQ")
target_sources(CreationStationResonanceSuppressor PRIVATE
    Source/Plugins/ResonanceSuppressor/CreationStationResonanceSuppressorProcessor.cpp
    Source/Plugins/ResonanceSuppressor/CreationStationResonanceSuppressorProcessor.h
    Source/Plugins/ResonanceSuppressor/CreationStationResonanceSuppressorEditor.cpp
    Source/Plugins/ResonanceSuppressor/CreationStationResonanceSuppressorEditor.h
)

add_creation_station_instrument_plugin(CreationStationSamplePlayer "Creation Station Sample Player" CSSp "Instrument|Sampler")
target_sources(CreationStationSamplePlayer PRIVATE
    Source/Plugins/SamplePlayer/CreationStationSamplePlayerProcessor.cpp
    Source/Plugins/SamplePlayer/CreationStationSamplePlayerProcessor.h
    Source/Plugins/SamplePlayer/CreationStationSamplePlayerEditor.cpp
    Source/Plugins/SamplePlayer/CreationStationSamplePlayerEditor.h
    Source/Plugins/SamplePlayer/SamplePlayerLibrarySettings.cpp
    Source/Plugins/SamplePlayer/SamplePlayerLibrarySettings.h
    Source/Audio/SamplePackCatalog.cpp
    Source/Audio/SamplePackCatalog.h
)

# CreationStationPitchPipe: disabled -- Source/Plugins/PitchPipe/ has never
# existed in this repo's history on any branch. This target was added in
# 185ec4c ("feat(station): complete VFS container cutover and remove legacy
# ProjectManager") referencing four source files that were never committed,
# which broke configure for every clean checkout since. Re-enable once the
# plugin is actually built.
# add_creation_station_instrument_plugin(CreationStationPitchPipe "Creation Station Pitch Pipe" CSPp "Instrument")
# target_sources(CreationStationPitchPipe PRIVATE
#     Source/Plugins/PitchPipe/CreationStationPitchPipeProcessor.cpp
#     Source/Plugins/PitchPipe/CreationStationPitchPipeProcessor.h
#     Source/Plugins/PitchPipe/CreationStationPitchPipeEditor.cpp
#     Source/Plugins/PitchPipe/CreationStationPitchPipeEditor.h
# )

install(TARGETS CreativeWorkstation
    RUNTIME DESTINATION . COMPONENT CreativeWorkstation
    RESOURCE DESTINATION . COMPONENT CreativeWorkstation
)
install(FILES README.md LICENSE docs/EULA.txt DESTINATION . COMPONENT CreativeWorkstation)

set(CPACK_PACKAGE_NAME "Creation Station")
set(CPACK_PACKAGE_VENDOR "LagDaemon Software")
set(CPACK_PACKAGE_CONTACT "LagDaemon Software")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Creation Station")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "creation-station-${CPACK_PACKAGE_VERSION}-win64")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Creation Station")
set(CPACK_GENERATOR "ZIP;WIX")
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_WIX_COMPONENT_INSTALL ON)
set(CPACK_WIX_PROGRAM_MENU_FOLDER "Creation Station")
set(CPACK_WIX_LICENSE_RTF "${CMAKE_CURRENT_SOURCE_DIR}/installer/EULA.rtf")
set(CPACK_PACKAGE_EXECUTABLES "Creative Workstation" "Creation Station")
set(CPACK_CREATE_DESKTOP_LINKS "Creative Workstation")
set(CPACK_COMPONENTS_ALL CreativeWorkstation)
set(CPACK_WIX_UPGRADE_GUID "9F39E8E4-3B66-4EE8-8B7E-8E9D9B7F35D2")
set(CPACK_STRIP_FILES TRUE)

include(CPack)
