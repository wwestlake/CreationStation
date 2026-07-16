#include "CreationStationContextStore.h"
#include "CreationStationContextEngine.h"
#include "../Patch/PatchModel.h"

namespace
{
juce::var documentToVar(const CreationStationContextEngine::SourceDocument& document)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", document.id);
    object->setProperty("title", document.title);
    object->setProperty("category", document.category);
    object->setProperty("body", document.body);
    object->setProperty("sourcePath", document.sourcePath);
    object->setProperty("updatedAt", document.updatedAt.toISO8601(true));

    juce::Array<juce::var> tags;
    for (const auto& tag : document.tags)
        tags.add(tag);
    object->setProperty("tags", juce::var(tags));

    return juce::var(object);
}

CreationStationContextEngine::SourceDocument documentFromVar(const juce::var& value)
{
    CreationStationContextEngine::SourceDocument document;
    if (auto* object = value.getDynamicObject())
    {
        document.id = object->getProperty("id").toString();
        document.title = object->getProperty("title").toString();
        document.category = object->getProperty("category").toString();
        document.body = object->getProperty("body").toString();
        document.sourcePath = object->getProperty("sourcePath").toString();
        document.updatedAt = juce::Time::fromISO8601(object->getProperty("updatedAt").toString());
        if (auto* tags = object->getProperty("tags").getArray())
            for (const auto& tag : *tags)
                document.tags.add(tag.toString());
    }

    return document;
}
}

bool CreationStationContextStore::rebuild(const ProjectManager& projectManager,
                                          const ContentLibrary& contentLibrary,
                                          const juce::String& workspaceMode,
                                          const juce::String& patinaSource,
                                          juce::String& errorMessage)
{
    documents.clearQuick();

    if (! projectManager.hasStorageRoot())
    {
        errorMessage = "Storage root is not configured yet.";
        return false;
    }

    snapshotFile = projectManager.getConfigDirectory().getChildFile("ai-context-store.json");

    CreationStationContextEngine::SourceDocument modeDocument;
    modeDocument.id = "workspace-mode";
    modeDocument.title = "Current workspace";
    modeDocument.category = "session";
    modeDocument.body = "Workspace mode: " + workspaceMode;
    modeDocument.tags.addArray({ "session", "workspace", workspaceMode.toLowerCase() });
    modeDocument.updatedAt = juce::Time::getCurrentTime();
    documents.add(modeDocument);

    CreationStationContextEngine::SourceDocument patinaDocument;
    patinaDocument.id = "patina-source-live";
    patinaDocument.title = "Patina source buffer";
    patinaDocument.category = "language";
    patinaDocument.body = patinaSource;
    patinaDocument.tags.addArray({ "patina", "dsl", "script" });
    patinaDocument.updatedAt = juce::Time::getCurrentTime();
    documents.add(patinaDocument);

    if (projectManager.hasProject())
    {
        const auto& project = projectManager.getCurrentProject();

        CreationStationContextEngine::SourceDocument projectDocument;
        projectDocument.id = "project-state";
        projectDocument.title = project.name;
        projectDocument.category = "project";
        projectDocument.body = "Project: " + project.name
                             + "\nSlug: " + project.slug
                             + "\nRoot: " + project.rootDirectory.getFullPathName()
                             + "\nAudio: " + project.audioDirectory.getFullPathName()
                             + "\nDSL: " + project.dslDirectory.getFullPathName()
                             + "\nAssets: " + project.assetsDirectory.getFullPathName();
        projectDocument.sourcePath = project.rootDirectory.getFullPathName();
        projectDocument.tags.addArray({ "project", "session" });
        projectDocument.updatedAt = juce::Time::getCurrentTime();
        documents.add(projectDocument);

        juce::Array<juce::File> patinaFiles;
        project.dslDirectory.getChildFile("Patina").findChildFiles(patinaFiles, juce::File::findFiles, false, "*.patina.json");
        for (const auto& file : patinaFiles)
        {
            CreationStationContextEngine::SourceDocument document;
            document.id = documentIdForFile("patina", file);
            document.title = file.getFileNameWithoutExtension();
            document.category = "patina-artifact";
            document.body = readTextPreview(file, 3000);
            document.sourcePath = file.getFullPathName();
            document.tags.addArray({ "patina", "artifact", "language" });
            document.updatedAt = file.getLastModificationTime();
            documents.add(document);
        }

        juce::Array<juce::File> patchFiles;
        project.dslDirectory.getChildFile("Patches").findChildFiles(patchFiles, juce::File::findFiles, false, "*.cspatch");
        for (const auto& file : patchFiles)
        {
            CreationStationContextEngine::SourceDocument document;
            document.id = documentIdForFile("patch", file);
            document.title = file.getFileNameWithoutExtension();
            document.category = "patch";

            juce::String patchError;
            cw::PatchDocument patch;
            if (cw::parsePatchDocumentJson(file.loadFileAsString(), patch, patchError))
            {
                document.body = "Patch: " + patch.name
                              + "\nType: " + patch.type
                              + "\nAuthor: " + patch.author
                              + "\nDescription: " + patch.description
                              + "\nSources: " + juce::String(patch.sources.size())
                              + "\nNodes: " + juce::String(patch.nodes.size())
                              + "\nConnections: " + juce::String(patch.connections.size())
                              + "\nParameters: " + juce::String(patch.parameters.size());
            }
            else
            {
                document.body = readTextPreview(file, 2500);
            }

            document.sourcePath = file.getFullPathName();
            document.tags.addArray({ "patch", "signal", "runtime" });
            document.updatedAt = file.getLastModificationTime();
            documents.add(document);
        }

        auto assetFiles = projectManager.listAssetFiles();
        for (const auto& file : assetFiles)
        {
            CreationStationContextEngine::SourceDocument document;
            document.id = documentIdForFile("asset", file);
            document.title = file.getFileName();
            document.category = "asset";
            document.body = makeAssetSummary(file);
            document.sourcePath = file.getFullPathName();
            document.tags.addArray({ "asset", "audio", "foley" });
            document.updatedAt = file.getLastModificationTime();
            documents.add(document);
        }
    }

    for (const auto& item : contentLibrary.getItems())
    {
        CreationStationContextEngine::SourceDocument document;
        document.id = "library-" + item.id;
        document.title = item.name;
        document.category = "content-library";
        document.body = "Name: " + item.name
                      + "\nType: " + item.type
                      + "\nCategory: " + item.category
                      + "\nDescription: " + item.description
                      + "\nTier: " + item.requiredTier
                      + "\nVersion: " + item.version
                      + "\nOrigin: " + ContentLibrary::originName(item.origin)
                      + "\nAccess: " + ContentLibrary::accessName(item.accessState);
        document.sourcePath = item.file.getFullPathName();
        document.tags.addArray({ "content", "library", item.type.toLowerCase() });
        document.updatedAt = item.file.exists() ? item.file.getLastModificationTime() : juce::Time::getCurrentTime();
        documents.add(document);
    }

    return writeSnapshot(snapshotFile, errorMessage);
}

bool CreationStationContextStore::load(const juce::File& file, juce::String& errorMessage)
{
    documents.clearQuick();
    snapshotFile = file;

    if (! file.existsAsFile())
    {
        errorMessage = "The AI context snapshot does not exist yet.";
        return false;
    }

    auto parsed = juce::JSON::parse(file.loadFileAsString());
    if (parsed.isVoid())
    {
        errorMessage = "The AI context snapshot is not valid JSON.";
        return false;
    }

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
    {
        errorMessage = "The AI context snapshot is missing its root object.";
        return false;
    }

    if (auto* docs = root->getProperty("documents").getArray())
        for (const auto& value : *docs)
            documents.add(documentFromVar(value));

    return true;
}

juce::String CreationStationContextStore::readTextPreview(const juce::File& file, int maxCharacters)
{
    if (! file.existsAsFile())
        return {};

    auto text = file.loadFileAsString();
    return text.length() > maxCharacters ? text.substring(0, maxCharacters) : text;
}

juce::String CreationStationContextStore::makeAssetSummary(const juce::File& file)
{
    return "Asset: " + file.getFileName()
         + "\nExtension: " + file.getFileExtension()
         + "\nSize: " + juce::File::descriptionOfSizeInBytes(file.getSize())
         + "\nPath: " + file.getFullPathName();
}

juce::String CreationStationContextStore::documentIdForFile(const juce::String& prefix, const juce::File& file)
{
    return prefix + "-" + juce::String::toHexString(file.getFullPathName().hashCode64());
}

juce::String CreationStationContextStore::joinTags(const juce::StringArray& tags)
{
    return tags.joinIntoString(", ");
}

bool CreationStationContextStore::writeSnapshot(const juce::File& file, juce::String& errorMessage) const
{
    auto* root = new juce::DynamicObject();
    root->setProperty("generatedAt", juce::Time::getCurrentTime().toISO8601(true));

    juce::Array<juce::var> serializedDocuments;
    for (const auto& document : documents)
        serializedDocuments.add(documentToVar(document));
    root->setProperty("documents", juce::var(serializedDocuments));

    file.getParentDirectory().createDirectory();
    if (! file.replaceWithText(juce::JSON::toString(juce::var(root), true)))
    {
        errorMessage = "Could not write the AI context snapshot.";
        return false;
    }

    return true;
}
