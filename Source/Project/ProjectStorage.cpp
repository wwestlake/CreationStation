#include <creation/assets/ProjectManifest.h>
#include <creation/assets/AssetTypes.h>
#include "ProjectStorage.h"

#include <creation/assets/ProjectSession.h>
#include <creation/assets/ProjectWorkspaceService.h>
#include <creation/assets/AssetMaterializer.h>

namespace cs
{
juce::String toStorageToken(AssetKind kind)
{
    return creation::assets::toStorageToken(kind);
}

AssetKind assetKindFromStorageToken(const juce::String& token)
{
    return creation::assets::assetKindFromStorageToken(token);
}

juce::String toDisplayName(AssetKind kind)
{
    return creation::assets::toDisplayName(kind);
}

juce::String toStorageToken(AssetReferenceMode mode)
{
    return creation::assets::toStorageToken(mode);
}

AssetReferenceMode assetReferenceModeFromStorageToken(const juce::String& token)
{
    return creation::assets::assetReferenceModeFromStorageToken(token);
}
}
