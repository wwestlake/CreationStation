#pragma once

#include <JuceHeader.h>
#include <creation/assets/AssetTypes.h>
#include <creation/assets/ProjectSession.h>
#include <creation/suite/SuiteSettings.h>
#include <memory>
#include <optional>

namespace cs
{
using AssetId = creation::assets::AssetId;
using AssetVersionId = creation::assets::AssetVersionId;
using AssetKind = creation::assets::AssetKind;
using AssetReferenceMode = creation::assets::AssetReferenceMode;
using AssetRef = creation::assets::AssetRef;
using AssetDescriptor = creation::assets::AssetDescriptor;
using AssetQuery = creation::assets::AssetQuery;

juce::String toStorageToken(AssetKind kind);
AssetKind assetKindFromStorageToken(const juce::String& token);
juce::String toDisplayName(AssetKind kind);
juce::String toStorageToken(AssetReferenceMode mode);
AssetReferenceMode assetReferenceModeFromStorageToken(const juce::String& token);
}
