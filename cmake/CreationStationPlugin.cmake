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
        COPY_PLUGIN_AFTER_BUILD TRUE
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
endfunction()
