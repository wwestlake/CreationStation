#pragma once

#include <JuceHeader.h>
#include <creation/assets/AssetTypes.h>
#include <memory>
#include <optional>

class ProjectManager;

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

class IProjectStorage
{
public:
    virtual ~IProjectStorage() = default;

    virtual bool hasProject() const = 0;
    virtual juce::String getProjectDisplayName() const = 0;

    virtual juce::Array<AssetDescriptor> enumerateAssets(const AssetQuery& query = {}) const = 0;
    virtual std::optional<AssetDescriptor> getAssetDescriptor(const AssetId& assetId) const = 0;

    virtual std::unique_ptr<juce::InputStream> openReadStream(const AssetId& assetId,
                                                              juce::String& errorMessage) const = 0;
    virtual bool readBytes(const AssetId& assetId,
                           juce::MemoryBlock& destination,
                           juce::String& errorMessage) const = 0;
    virtual bool readText(const AssetId& assetId,
                          juce::String& destination,
                          juce::String& errorMessage) const = 0;

    virtual bool writeBytes(const AssetDescriptor& desiredAsset,
                            const juce::MemoryBlock& source,
                            AssetDescriptor& storedAsset,
                            juce::String& errorMessage) = 0;
    virtual bool writeText(const AssetDescriptor& desiredAsset,
                           const juce::String& source,
                           AssetDescriptor& storedAsset,
                           juce::String& errorMessage) = 0;
    virtual bool removeAsset(const AssetId& assetId, juce::String& errorMessage) = 0;
};

class IExternalFileBridge
{
public:
    virtual ~IExternalFileBridge() = default;

    virtual juce::File materializeAssetFile(const AssetId& assetId,
                                            juce::String& errorMessage) = 0;
};

std::unique_ptr<IProjectStorage> createFolderProjectStorage(ProjectManager& manager);
std::unique_ptr<IExternalFileBridge> createFolderExternalFileBridge(ProjectManager& manager);
}
