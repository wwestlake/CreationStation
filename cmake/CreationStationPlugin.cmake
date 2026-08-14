# Shared definition for every "Creation Station" branded effect plugin target.
# Keeps the per-plugin CMake boilerplate (juce_add_plugin + header generation +
# compile definitions + link libraries) to one call site per plugin instead of
# repeating the ~50-line block from the CreationStationInstrument target.

function(add_creation_station_effect_plugin TARGET_NAME PRODUCT_NAME PLUGIN_CODE VST3_CATEGORY)
    juce_add_plugin(${TARGET_NAME}
        COMPANY_NAME "LagDaemon Software"
        PRODUCT_NAME "${PRODUCT_NAME}"
        BUNDLE_ID "com.creationstation.${TARGET_NAME}"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        # Auto-copy to the system VST3 folder (C:\Program Files\Common Files\VST3) needs admin
        # rights and fails in a normal dev build, breaking the whole build. Keep it off and copy
        # the built .vst3 to a user-writable, app-scanned folder manually instead.
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE CrSt
        PLUGIN_CODE ${PLUGIN_CODE}
        FORMATS VST3
        VST3_CATEGORIES ${VST3_CATEGORY}
    )

    juce_generate_juce_header(${TARGET_NAME})

    target_compile_definitions(${TARGET_NAME} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${TARGET_NAME},JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${TARGET_NAME},JUCE_VERSION>"
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
        CreationStationPluginKit
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )

    creation_station_copy_plugin_to_scanned_folder(${TARGET_NAME})
endfunction()

# Copies a plugin target's built VST3 output into a "Plugins" folder that sits directly next to
# the CreativeWorkstation executable's own artefact folder -- the "user-writable, app-scanned
# folder" referenced in the comment above, which was never actually wired up until now.
# VstPluginCatalog::rescan() (Source/Audio/VstPluginCatalog.cpp) always scans this exact relative
# location in addition to whatever the user has configured, so every in-house plugin is found with
# zero configuration. Whole-directory copy (not a single file) because JUCE's VST3 output shape
# (single binary vs. a Contents/<arch> bundle folder) isn't this function's business to know.
function(creation_station_copy_plugin_to_scanned_folder TARGET_NAME)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/CreativeWorkstation_artefacts/$<CONFIG>/Plugins"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_artefacts/$<CONFIG>/VST3"
            "${CMAKE_CURRENT_BINARY_DIR}/CreativeWorkstation_artefacts/$<CONFIG>/Plugins"
        COMMENT "Copying ${TARGET_NAME} into the app-scanned Plugins folder"
    )
endfunction()

# Same shape as add_creation_station_effect_plugin, but for an instrument (MIDI in, audio out,
# no audio input bus) instead of an effect. IS_SYNTH/NEEDS_MIDI_INPUT are the only functional
# differences; everything else (company/manufacturer identity, PluginKit link, VST3-only format,
# manual-copy policy) stays identical so every in-house plugin behaves consistently.
function(add_creation_station_instrument_plugin TARGET_NAME PRODUCT_NAME PLUGIN_CODE VST3_CATEGORY)
    juce_add_plugin(${TARGET_NAME}
        COMPANY_NAME "LagDaemon Software"
        PRODUCT_NAME "${PRODUCT_NAME}"
        BUNDLE_ID "com.creationstation.${TARGET_NAME}"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE CrSt
        PLUGIN_CODE ${PLUGIN_CODE}
        FORMATS VST3
        VST3_CATEGORIES ${VST3_CATEGORY}
    )

    juce_generate_juce_header(${TARGET_NAME})

    target_compile_definitions(${TARGET_NAME} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${TARGET_NAME},JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${TARGET_NAME},JUCE_VERSION>"
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
        CreationStationPluginKit
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )

    creation_station_copy_plugin_to_scanned_folder(${TARGET_NAME})
endfunction()
