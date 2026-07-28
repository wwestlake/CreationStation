#include "ProjectStorage.h"
#include "ProjectManager.h"

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

namespace
{
juce::String normalizeLogicalPath(const juce::String& path)
{
    return path.replaceCharacter('\\', '/').trimCharactersAtStart("/");
}

juce::String slugForAssetName(const juce::String& name)
{
    auto slug = name.trim().toLowerCase();
    slug = slug.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-_ ");
    slug = slug.replace(" ", "-");
    while (slug.contains("--"))
        slug = slug.replace("--", "-");
    slug = slug.trimCharactersAtStart("-");
    slug = slug.trimCharactersAtEnd("-");
    return slug.isNotEmpty() ? slug : "asset";
}

juce::String defaultExtensionForAsset(const AssetDescriptor& asset)
{
    auto lowerMedia = asset.mediaType.trim().toLowerCase();
    auto lowerPath = asset.logicalPath.trim().toLowerCase();

    if (lowerPath.endsWith(".wav") || lowerMedia.contains("wav"))
        return ".wav";
    if (lowerPath.endsWith(".cspatch") || lowerMedia.contains("cspatch"))
        return ".cspatch";
    if (lowerPath.endsWith(".patina.json") || lowerMedia.contains("json"))
        return ".json";
    if (lowerPath.endsWith(".mid") || lowerMedia.contains("midi"))
        return ".mid";

    switch (asset.kind)
    {
        case AssetKind::audio:
        case AssetKind::render:
            return ".wav";
        case AssetKind::patch:
            return ".cspatch";
        case AssetKind::script:
        case AssetKind::metadata:
            return ".json";
        case AssetKind::midi:
            return ".mid";
        case AssetKind::preset:
            return ".preset";
        case AssetKind::samplePack:
            return ".zip";
        case AssetKind::binary:
        case AssetKind::unknown:
            return ".bin";
    }

    return ".bin";
}

AssetKind inferKindFromLegacyType(const juce::String& type)
{
    auto normalized = type.trim().toLowerCase();
    if (normalized == "audiofile") return AssetKind::audio;
    if (normalized == "render") return AssetKind::render;
    if (normalized == "signalpatch") return AssetKind::patch;
    if (normalized == "patinaprogram") return AssetKind::script;
    return AssetKind::unknown;
}

juce::String inferMediaType(const juce::File& file, AssetKind kind)
{
    auto ext = file.getFileExtension().toLowerCase();
    if (ext == ".wav") return "audio/wav";
    if (ext == ".json" || ext == ".patina.json") return "application/json";
    if (ext == ".cspatch") return "application/x-creation-station-patch";
    if (ext == ".mid") return "audio/midi";

    switch (kind)
    {
        case AssetKind::audio:
        case AssetKind::render: return "audio/wav";
        case AssetKind::patch: return "application/x-creation-station-patch";
        case AssetKind::script:
        case AssetKind::metadata: return "application/json";
        default: break;
    }

    return "application/octet-stream";
}

AssetDescriptor makeDescriptorFromProjectAsset(const ProjectManager::ProjectAsset& asset)
{
    AssetDescriptor descriptor;
    descriptor.id = asset.id;
    descriptor.versionId = asset.versionId;
    descriptor.displayName = asset.name;
    descriptor.kind = inferKindFromLegacyType(asset.type);
    descriptor.category = asset.category;
    descriptor.description = asset.description;
    descriptor.mediaType = inferMediaType(asset.file, descriptor.kind);
    descriptor.logicalPath = normalizeLogicalPath(asset.relativePath);
    descriptor.fileSizeBytes = asset.fileSizeBytes;
    descriptor.modifiedAt = asset.file.getLastModificationTime();
    descriptor.tags.add(asset.type.trim().toLowerCase());
    return descriptor;
}

bool assetMatchesQuery(const AssetDescriptor& asset, const AssetQuery& query)
{
    if (query.kind.has_value() && asset.kind != *query.kind)
        return false;

    auto prefix = normalizeLogicalPath(query.logicalPathPrefix);
    if (prefix.isNotEmpty() && ! asset.logicalPath.startsWithIgnoreCase(prefix))
        return false;

    auto text = query.searchText.trim();
    if (text.isNotEmpty())
    {
        auto haystack = (asset.displayName + "\n" + asset.description + "\n" + asset.logicalPath + "\n" + asset.category).toLowerCase();
        if (! haystack.contains(text.toLowerCase()))
            return false;
    }

    for (const auto& tag : query.tags)
        if (! asset.tags.contains(tag))
            return false;

    return true;
}

class FolderProjectStorage final : public IProjectStorage
{
public:
    explicit FolderProjectStorage(ProjectManager& managerIn) : manager(managerIn) {}

    bool hasProject() const override { return manager.hasProject(); }
    juce::String getProjectDisplayName() const override { return manager.getDisplayLabel(); }

    juce::Array<AssetDescriptor> enumerateAssets(const AssetQuery& query) const override
    {
        juce::Array<AssetDescriptor> results;
        for (const auto& legacyAsset : manager.listProjectAssets())
        {
            auto descriptor = makeDescriptorFromProjectAsset(legacyAsset);
            if (assetMatchesQuery(descriptor, query))
                results.add(std::move(descriptor));
        }

        return results;
    }

    std::optional<AssetDescriptor> getAssetDescriptor(const AssetId& assetId) const override
    {
        for (const auto& legacyAsset : manager.listProjectAssets())
            if (legacyAsset.id == assetId)
                return makeDescriptorFromProjectAsset(legacyAsset);

        return std::nullopt;
    }

    std::unique_ptr<juce::InputStream> openReadStream(const AssetId& assetId,
                                                      juce::String& errorMessage) const override
    {
        auto file = resolveAssetFile(assetId);
        if (! file.existsAsFile())
        {
            errorMessage = "That asset does not exist in the current project.";
            return {};
        }

        auto stream = file.createInputStream();
        if (stream == nullptr)
            errorMessage = "Could not open the asset for reading.";

        return stream;
    }

    bool readBytes(const AssetId& assetId,
                   juce::MemoryBlock& destination,
                   juce::String& errorMessage) const override
    {
        auto stream = openReadStream(assetId, errorMessage);
        if (stream == nullptr)
            return false;

        destination.reset();
        stream->readIntoMemoryBlock(destination);
        return true;
    }

    bool readText(const AssetId& assetId,
                  juce::String& destination,
                  juce::String& errorMessage) const override
    {
        auto file = resolveAssetFile(assetId);
        if (! file.existsAsFile())
        {
            errorMessage = "That asset does not exist in the current project.";
            return false;
        }

        destination = file.loadFileAsString();
        return true;
    }

    bool writeBytes(const AssetDescriptor& desiredAsset,
                    const juce::MemoryBlock& source,
                    AssetDescriptor& storedAsset,
                    juce::String& errorMessage) override
    {
        auto file = resolveWritableFile(desiredAsset, errorMessage);
        if (file == juce::File())
            return false;

        auto stream = std::unique_ptr<juce::FileOutputStream>(file.createOutputStream());
        if (stream == nullptr)
        {
            errorMessage = "Could not open the asset destination for writing.";
            return false;
        }

        if (! stream->write(source.getData(), source.getSize()))
        {
            errorMessage = "Could not write the asset data.";
            return false;
        }

        stream->flush();
        storedAsset = describeStoredAsset(desiredAsset, file);
        return true;
    }

    bool writeText(const AssetDescriptor& desiredAsset,
                   const juce::String& source,
                   AssetDescriptor& storedAsset,
                   juce::String& errorMessage) override
    {
        auto file = resolveWritableFile(desiredAsset, errorMessage);
        if (file == juce::File())
            return false;

        if (! file.replaceWithText(source))
        {
            errorMessage = "Could not write the asset text.";
            return false;
        }

        storedAsset = describeStoredAsset(desiredAsset, file);
        return true;
    }

    bool removeAsset(const AssetId& assetId, juce::String& errorMessage) override
    {
        auto file = resolveAssetFile(assetId);
        if (! file.existsAsFile())
        {
            errorMessage = "That asset does not exist in the current project.";
            return false;
        }

        if (! file.deleteFile())
        {
            errorMessage = "Could not remove the asset from the current project.";
            return false;
        }

        return true;
    }

private:
    juce::File resolveAssetFile(const AssetId& assetId) const
    {
        for (const auto& legacyAsset : manager.listProjectAssets())
            if (legacyAsset.id == assetId)
                return legacyAsset.file;

        return {};
    }

    juce::File defaultFolderForKind(AssetKind kind) const
    {
        const auto& project = manager.getCurrentProject();
        switch (kind)
        {
            case AssetKind::audio:
            case AssetKind::binary:
            case AssetKind::midi:
            case AssetKind::preset:
            case AssetKind::samplePack:
            case AssetKind::unknown:
                return project.assetsDirectory;
            case AssetKind::render:
                return project.rendersDirectory;
            case AssetKind::patch:
                return project.dslDirectory.getChildFile("Patches");
            case AssetKind::script:
                return project.dslDirectory.getChildFile("Patina");
            case AssetKind::metadata:
                return project.rootDirectory.getChildFile("Data");
        }

        return project.assetsDirectory;
    }

    juce::File resolveWritableFile(const AssetDescriptor& desiredAsset, juce::String& errorMessage) const
    {
        if (! manager.hasProject())
        {
            errorMessage = "Open or create a project before writing assets.";
            return {};
        }

        if (desiredAsset.id.isNotEmpty())
        {
            auto existing = resolveAssetFile(desiredAsset.id);
            if (existing != juce::File())
                return existing;
        }

        juce::File destination;
        auto logicalPath = normalizeLogicalPath(desiredAsset.logicalPath);
        if (logicalPath.isNotEmpty())
            destination = manager.getCurrentProject().rootDirectory.getChildFile(logicalPath);
        else
            destination = defaultFolderForKind(desiredAsset.kind)
                            .getChildFile(slugForAssetName(desiredAsset.displayName) + defaultExtensionForAsset(desiredAsset));

        auto parent = destination.getParentDirectory();
        if (! parent.exists() && ! parent.createDirectory())
        {
            errorMessage = "Could not create the destination folder for that asset.";
            return {};
        }

        if (desiredAsset.id.isEmpty() && destination.existsAsFile())
        {
            auto baseName = destination.getFileNameWithoutExtension();
            auto extension = destination.getFileExtension();
            auto suffix = 2;
            while (destination.existsAsFile())
            {
                destination = parent.getChildFile(baseName + "-" + juce::String(suffix) + extension);
                ++suffix;
            }
        }

        return destination;
    }

    AssetDescriptor describeStoredAsset(const AssetDescriptor& requested, const juce::File& file) const
    {
        AssetDescriptor stored = requested;
        stored.displayName = stored.displayName.isNotEmpty() ? stored.displayName : file.getFileNameWithoutExtension();
        stored.logicalPath = normalizeLogicalPath(file.getRelativePathFrom(manager.getCurrentProject().rootDirectory));
        stored.id = stored.id.isNotEmpty() ? stored.id
                                           : manager.getCurrentProject().slug + ":" + stored.logicalPath;
        stored.mediaType = stored.mediaType.isNotEmpty() ? stored.mediaType
                                                         : inferMediaType(file, stored.kind);
        stored.fileSizeBytes = file.getSize();
        stored.modifiedAt = file.getLastModificationTime();
        return stored;
    }

    ProjectManager& manager;
};

class FolderExternalFileBridge final : public IExternalFileBridge
{
public:
    explicit FolderExternalFileBridge(ProjectManager& managerIn) : manager(managerIn) {}

    juce::File materializeAssetFile(const AssetId& assetId, juce::String& errorMessage) override
    {
        for (const auto& asset : manager.listProjectAssets())
            if (asset.id == assetId && asset.file.existsAsFile())
                return asset.file;

        errorMessage = "That asset could not be materialized as a real file.";
        return {};
    }

private:
    ProjectManager& manager;
};
}

std::unique_ptr<IProjectStorage> createFolderProjectStorage(ProjectManager& manager)
{
    return std::make_unique<FolderProjectStorage>(manager);
}

std::unique_ptr<IExternalFileBridge> createFolderExternalFileBridge(ProjectManager& manager)
{
    return std::make_unique<FolderExternalFileBridge>(manager);
}
}
