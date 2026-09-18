add_library(creation_asset_system STATIC
    src/AssetTypes.cpp
    src/VirtualFileSystem.cpp
)

target_include_directories(creation_asset_system PUBLIC include)
target_link_libraries(creation_asset_system PUBLIC juce::juce_core)
target_compile_features(creation_asset_system PUBLIC cxx_std_20)

# Auto-generated output name overrides
set_target_properties(creation_asset_system PROPERTIES OUTPUT_NAME "djehuti_asset_system")
