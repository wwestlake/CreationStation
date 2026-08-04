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
    if (lowerMedia.contains("json"))
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

juce::String inferMediaType(const juce::File& file, AssetKind kind)
{
    auto ext = file.getFileExtension().toLowerCase();
    if (ext == ".wav") return "audio/wav";
    if (ext == ".json") return "application/json";
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

juce::String defaultLogicalRootForKind(AssetKind kind)
{
    switch (kind)
    {
        case AssetKind::render:
            return creation::assets::ProjectContainerPaths::derivedAssetRoot;
        case AssetKind::patch:
        case AssetKind::script:
        case AssetKind::metadata:
        case AssetKind::audio:
        case AssetKind::preset:
        case AssetKind::samplePack:
        case AssetKind::midi:
        case AssetKind::binary:
        case AssetKind::unknown:
        default:
            return creation::assets::ProjectContainerPaths::sourceAssetRoot;
    }
}

juce::String defaultLogicalPathForAsset(const AssetDescriptor& asset)
{
    return defaultLogicalRootForKind(asset.kind)
         + slugForAssetName(asset.displayName.isNotEmpty() ? asset.displayName : asset.id)
         + defaultExtensionForAsset(asset);
}

AssetDescriptor buildSuiteDescriptor(const AssetDescriptor& requested,
                                     const juce::String& logicalPath,
                                     int64 fileSizeBytes,
                                     juce::Time modifiedAt)
{
    AssetDescriptor descriptor = requested;
    if (descriptor.id.isEmpty())
        descriptor.id = "asset:" + juce::Uuid().toString();
    if (descriptor.version.isEmpty())
        descriptor.version = "1";
    if (descriptor.versionId.isEmpty())
        descriptor.versionId = descriptor.id + "@" + descriptor.version;
    descriptor.displayName = descriptor.displayName.isNotEmpty()
        ? descriptor.displayName
        : logicalPath.fromLastOccurrenceOf("/", false, false).upToLastOccurrenceOf(".", false, false);
    descriptor.logicalPath = logicalPath;
    descriptor.sourceApp = "Creation Station";
    descriptor.fileSizeBytes = fileSizeBytes;
    descriptor.modifiedAt = modifiedAt;
    if (descriptor.createdAt == juce::Time())
        descriptor.createdAt = modifiedAt;
    descriptor.revision = juce::jmax(1, descriptor.revision + 1);
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

class SuiteProjectStorage final : public IProjectStorage
{
public:
    SuiteProjectStorage(creation::assets::ProjectSession& sessionIn,
                        const creation::suite::SuiteSettings& settingsIn)
        : session(sessionIn), suiteSettings(settingsIn) {}

    bool hasProject() const override { return session.isValid(); }
    juce::String getProjectDisplayName() const override
    {
        return session.isValid() ? session.getManifest().projectName : juce::String();
    }

    juce::Array<AssetDescriptor> enumerateAssets(const AssetQuery& query) const override
    {
        if (! session.isValid())
            return {};
        return session.getManifest().assetCatalog.query(query);
    }

    std::optional<AssetDescriptor> getAssetDescriptor(const AssetId& assetId) const override
    {
        if (! session.isValid())
            return std::nullopt;
        if (const auto* descriptor = session.getManifest().assetCatalog.findById(assetId))
            return *descriptor;
        return std::nullopt;
    }

    std::unique_ptr<juce::InputStream> openReadStream(const AssetId& assetId,
                                                      juce::String& errorMessage) const override
    {
        if (! session.isValid())
        {
            errorMessage = "No active project session.";
            return {};
        }
        const auto* descriptor = session.getManifest().assetCatalog.findById(assetId);
        if (descriptor == nullptr)
        {
            errorMessage = "That asset does not exist in the container.";
            return {};
        }
        juce::MemoryBlock data;
        if (! session.readEntry(descriptor->logicalPath, data))
        {
            errorMessage = "That asset entry was not found in the suite container.";
            return {};
        }
        return std::make_unique<juce::MemoryInputStream>(data, false);
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
        juce::MemoryBlock data;
        if (! readBytes(assetId, data, errorMessage))
            return false;
        destination = juce::String::fromUTF8(static_cast<const char*>(data.getData()),
                                             static_cast<int>(data.getSize()));
        return true;
    }

    bool writeBytes(const AssetDescriptor& desiredAsset,
                    const juce::MemoryBlock& source,
                    AssetDescriptor& storedAsset,
                    juce::String& errorMessage) override
    {
        if (! session.isValid())
        {
            errorMessage = "No active project session.";
            return false;
        }

        auto logicalPath = normalizeLogicalPath(desiredAsset.logicalPath);
        if (logicalPath.isEmpty())
            logicalPath = defaultLogicalPathForAsset(desiredAsset);

        if (! session.writeEntry(logicalPath, source, juce::Time::getCurrentTime()))
        {
            errorMessage = "Could not stage the asset into the suite container.";
            return false;
        }

        auto descriptor = buildSuiteDescriptor(desiredAsset,
                                               logicalPath,
                                               static_cast<int64>(source.getSize()),
                                               juce::Time::getCurrentTime());
        session.upsertAssetDescriptor(descriptor);
        if (! session.commit(errorMessage))
            return false;

        storedAsset = descriptor;
        return true;
    }

    bool writeText(const AssetDescriptor& desiredAsset,
                   const juce::String& source,
                   AssetDescriptor& storedAsset,
                   juce::String& errorMessage) override
    {
        juce::MemoryBlock data(source.toRawUTF8(), static_cast<size_t>(source.getNumBytesAsUTF8()));
        return writeBytes(desiredAsset, data, storedAsset, errorMessage);
    }

    bool removeAsset(const AssetId& assetId, juce::String& errorMessage) override
    {
        if (! session.isValid())
        {
            errorMessage = "No active project session.";
            return false;
        }

        const auto* descriptor = session.getManifest().assetCatalog.findById(assetId);
        if (descriptor == nullptr)
        {
            errorMessage = "That asset does not exist in the suite container.";
            return false;
        }

        if (! session.removeEntry(descriptor->logicalPath))
        {
            errorMessage = "Could not remove the asset entry from the suite container.";
            return false;
        }

        session.removeAssetDescriptorByVersionId(descriptor->versionId);
        return session.commit(errorMessage);
    }

private:
    creation::assets::ProjectSession& session;
    const creation::suite::SuiteSettings& suiteSettings;
};

class SuiteExternalFileBridge final : public IExternalFileBridge
{
public:
    SuiteExternalFileBridge(creation::assets::ProjectSession& sessionIn,
                            const creation::suite::SuiteSettings& settingsIn)
        : session(sessionIn), suiteSettings(settingsIn) {}

    ~SuiteExternalFileBridge() override
    {
        for (auto& lease : leases)
        {
            juce::String errorMessage;
            creation::assets::AssetMaterializer::releaseLease(lease, errorMessage);
        }
    }

    juce::File materializeAssetFile(const AssetId& assetId, juce::String& errorMessage) override
    {
        if (! session.isValid())
        {
            errorMessage = "No active project session.";
            return {};
        }

        const auto* descriptor = session.getManifest().assetCatalog.findById(assetId);
        if (descriptor == nullptr)
        {
            errorMessage = "That asset could not be found in the suite container.";
            return {};
        }

        creation::assets::AssetRef reference;
        reference.id = assetId;
        reference.mode = creation::assets::AssetReferenceMode::exact;

        creation::assets::MaterializedAssetLease lease;
        if (! creation::assets::ProjectWorkspaceService::materializeAsset(session,
                                                                          suiteSettings,
                                                                          reference,
                                                                          creation::assets::MaterializationAccess::readOnly,
                                                                          lease,
                                                                          errorMessage))
            return {};

        for (int index = leases.size(); --index >= 0;)
        {
            if (leases.getReference(index).logicalPath == descriptor->logicalPath)
            {
                juce::String releaseError;
                creation::assets::AssetMaterializer::releaseLease(leases.getReference(index), releaseError);
                leases.remove(index);
            }
        }

        auto materializedFile = lease.materializedFile;
        leases.add(std::move(lease));
        return materializedFile;
    }

private:
    creation::assets::ProjectSession& session;
    const creation::suite::SuiteSettings& suiteSettings;
    juce::Array<creation::assets::MaterializedAssetLease> leases;
};
}

std::unique_ptr<cs::IProjectStorage> cs::createSuiteProjectStorage(creation::assets::ProjectSession& session,
                                                                    const creation::suite::SuiteSettings& settings)
{
    return std::make_unique<SuiteProjectStorage>(session, settings);
}

std::unique_ptr<cs::IExternalFileBridge> cs::createSuiteExternalFileBridge(creation::assets::ProjectSession& session,
                                                                            const creation::suite::SuiteSettings& settings)
{
    return std::make_unique<SuiteExternalFileBridge>(session, settings);
}
}
