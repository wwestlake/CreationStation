#include "creation/assets/AssetTypes.h"

namespace creation::assets
{
juce::String toStorageToken(AssetKind kind)
{
    switch (kind)
    {
        case AssetKind::audio: return "audio";
        case AssetKind::render: return "render";
        case AssetKind::patch: return "patch";
        case AssetKind::script: return "script";
        case AssetKind::metadata: return "metadata";
        case AssetKind::preset: return "preset";
        case AssetKind::samplePack: return "samplePack";
        case AssetKind::midi: return "midi";
        case AssetKind::binary: return "binary";
        case AssetKind::unknown: break;
    }

    return "unknown";
}

AssetKind assetKindFromStorageToken(const juce::String& token)
{
    auto normalized = token.trim().toLowerCase();
    if (normalized == "audio") return AssetKind::audio;
    if (normalized == "render") return AssetKind::render;
    if (normalized == "patch") return AssetKind::patch;
    if (normalized == "script") return AssetKind::script;
    if (normalized == "metadata") return AssetKind::metadata;
    if (normalized == "preset") return AssetKind::preset;
    if (normalized == "samplepack") return AssetKind::samplePack;
    if (normalized == "midi") return AssetKind::midi;
    if (normalized == "binary") return AssetKind::binary;
    return AssetKind::unknown;
}

juce::String toDisplayName(AssetKind kind)
{
    switch (kind)
    {
        case AssetKind::audio: return "Audio";
        case AssetKind::render: return "Render";
        case AssetKind::patch: return "Patch";
        case AssetKind::script: return "Script";
        case AssetKind::metadata: return "Metadata";
        case AssetKind::preset: return "Preset";
        case AssetKind::samplePack: return "Sample Pack";
        case AssetKind::midi: return "MIDI";
        case AssetKind::binary: return "Binary";
        case AssetKind::unknown: break;
    }

    return "Unknown";
}

juce::String toStorageToken(AssetReferenceMode mode)
{
    switch (mode)
    {
        case AssetReferenceMode::compatibleLatest: return "compatibleLatest";
        case AssetReferenceMode::latest: return "latest";
        case AssetReferenceMode::exact: break;
    }

    return "exact";
}

AssetReferenceMode assetReferenceModeFromStorageToken(const juce::String& token)
{
    auto normalized = token.trim().toLowerCase();
    if (normalized == "compatiblelatest") return AssetReferenceMode::compatibleLatest;
    if (normalized == "latest") return AssetReferenceMode::latest;
    return AssetReferenceMode::exact;
}
}
