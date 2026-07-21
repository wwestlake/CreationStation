#include "ProjectManager.h"

namespace
{
juce::String sanitiseName(const juce::String& name)
{
    auto trimmed = name.trim();
    return trimmed.isNotEmpty() ? trimmed : "Untitled Project";
}

juce::String normaliseArchivePath(juce::String path)
{
    return path.replaceCharacter('\\', '/').trimCharactersAtStart("/");
}

void addTextEntry(juce::ZipFile::Builder& builder, const juce::String& path, const juce::String& text)
{
    builder.addEntry(new juce::MemoryInputStream(text.toRawUTF8(), text.getNumBytesAsUTF8(), true),
                     9,
                     normaliseArchivePath(path),
                     juce::Time::getCurrentTime());
}

void collectFiles(const juce::File& directory, juce::Array<juce::File>& files)
{
    if (! directory.isDirectory())
        return;

    directory.findChildFiles(files, juce::File::findFiles, true, "*");
}

void addDirectoryToPackage(juce::ZipFile::Builder& builder,
                           const juce::File& sourceRoot,
                           const juce::File& directory,
                           const juce::String& archiveRoot,
                           juce::Array<juce::File>& packagedFiles)
{
    juce::Array<juce::File> files;
    collectFiles(directory, files);

    for (const auto& file : files)
    {
        auto relativePath = file.getRelativePathFrom(sourceRoot);
        auto archivePath = normaliseArchivePath(archiveRoot + "/" + relativePath);
        builder.addFile(file, 6, archivePath);
        packagedFiles.add(file);
    }
}
}

juce::File ProjectManager::getStoragePointerFile()
{
    auto executableDirectory = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    return executableDirectory.getChildFile("CreationStation.storage");
}

juce::File ProjectManager::getAutoloadLastProjectFile() const
{
    return getConfigDirectory().getChildFile("autoload-last-project.txt");
}

juce::String ProjectManager::makeSlug(const juce::String& name)
{
    auto slug = name.trim().toLowerCase();
    slug = slug.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-_ ");
    slug = slug.replace(" ", "-");
    while (slug.contains("--"))
        slug = slug.replace("--", "-");
    slug = slug.trimCharactersAtStart("-");
    slug = slug.trimCharactersAtEnd("-");
    return slug.isNotEmpty() ? slug : "untitled-project";
}

juce::File ProjectManager::getWorkspaceRoot() const
{
    return storageRoot.getChildFile("Projects");
}

juce::File ProjectManager::getLayoutsRoot() const
{
    return storageRoot.getChildFile("Layouts");
}

juce::File ProjectManager::getStorageRoot() const
{
    return storageRoot;
}

juce::File ProjectManager::getConfigDirectory() const
{
    return storageRoot.getChildFile("Config");
}

juce::File ProjectManager::getContentDirectory() const
{
    return storageRoot.getChildFile("Content");
}

juce::File ProjectManager::getBuiltInContentDirectory() const
{
    return getContentDirectory().getChildFile("BuiltIn");
}

juce::File ProjectManager::getDownloadedContentDirectory() const
{
    return getContentDirectory().getChildFile("Downloaded");
}

juce::File ProjectManager::getUserContentDirectory() const
{
    return getContentDirectory().getChildFile("User");
}

juce::File ProjectManager::getBuiltInTutorialDirectory() const
{
    return getBuiltInContentDirectory().getChildFile("Tutorials");
}

juce::File ProjectManager::getUserTutorialDirectory() const
{
    return getUserContentDirectory().getChildFile("Tutorials");
}

juce::File ProjectManager::getContentManifestFile() const
{
    return getConfigDirectory().getChildFile("content-library.json");
}

juce::File ProjectManager::getAiContextStoreFile() const
{
    return getConfigDirectory().getChildFile("ai-context-store.json");
}

juce::File ProjectManager::getAiProviderSettingsFile() const
{
    return getConfigDirectory().getChildFile("ai-provider-settings.json");
}

juce::File ProjectManager::getVstSearchPathFile() const
{
    return getConfigDirectory().getChildFile("vst-search-paths.txt");
}

juce::File ProjectManager::getControlSurfaceMappingsFile() const
{
    return getConfigDirectory().getChildFile("control-surface-mappings.json");
}

bool ProjectManager::loadAiProviderSettings(AiProviderSettings& settings) const
{
    auto file = getAiProviderSettingsFile();
    if (! file.existsAsFile())
        return false;

    auto json = juce::JSON::parse(file.loadFileAsString());
    if (! json.isObject())
        return false;

    auto* object = json.getDynamicObject();
    if (object == nullptr)
        return false;

    settings.providerName = object->getProperty("providerName").toString();
    settings.baseUrl = object->getProperty("baseUrl").toString();
    settings.modelName = object->getProperty("modelName").toString();
    settings.apiKey = object->getProperty("apiKey").toString();
    return true;
}

bool ProjectManager::saveAiProviderSettings(const AiProviderSettings& settings, juce::String& errorMessage) const
{
    if (! hasStorageRoot())
    {
        errorMessage = "Choose a storage folder before saving AI settings.";
        return false;
    }

    auto configDirectory = getConfigDirectory();
    if (! configDirectory.exists() && ! configDirectory.createDirectory())
    {
        errorMessage = "Could not create the config folder for AI settings.";
        return false;
    }

    auto* root = new juce::DynamicObject();
    root->setProperty("providerName", settings.providerName);
    root->setProperty("baseUrl", settings.baseUrl);
    root->setProperty("modelName", settings.modelName);
    root->setProperty("apiKey", settings.apiKey);

    auto json = juce::JSON::toString(juce::var(root), true);
    if (! getAiProviderSettingsFile().replaceWithText(json))
    {
        errorMessage = "Could not save the AI provider settings.";
        return false;
    }

    return true;
}

juce::StringArray ProjectManager::loadVstSearchPaths() const
{
    juce::StringArray paths;
    auto file = getVstSearchPathFile();
    if (! file.existsAsFile())
        return paths;

    paths.addLines(file.loadFileAsString());
    paths.trim();
    paths.removeEmptyStrings();
    paths.removeDuplicates(false);
    return paths;
}

bool ProjectManager::saveVstSearchPaths(const juce::StringArray& paths, juce::String& errorMessage) const
{
    if (! hasStorageRoot())
    {
        errorMessage = "Choose a storage folder before saving VST paths.";
        return false;
    }

    auto configDirectory = getConfigDirectory();
    if (! configDirectory.exists() && ! configDirectory.createDirectory())
    {
        errorMessage = "Could not create the config folder for VST paths.";
        return false;
    }

    juce::StringArray normalized = paths;
    normalized.trim();
    normalized.removeEmptyStrings();
    normalized.removeDuplicates(false);

    if (! getVstSearchPathFile().replaceWithText(normalized.joinIntoString("\n")))
    {
        errorMessage = "Could not save the VST search paths.";
        return false;
    }

    return true;
}

bool ProjectManager::loadControlSurfaceMappings(ControlSurfaceMappingStore& store, juce::String& errorMessage) const
{
    return store.loadFromFile(getControlSurfaceMappingsFile(), errorMessage);
}

bool ProjectManager::saveControlSurfaceMappings(const ControlSurfaceMappingStore& store, juce::String& errorMessage) const
{
    if (! hasStorageRoot())
    {
        errorMessage = "Choose a storage folder before saving control surface mappings.";
        return false;
    }

    return store.saveToFile(getControlSurfaceMappingsFile(), errorMessage);
}

juce::File ProjectManager::getProjectsRoot() const
{
    return getWorkspaceRoot();
}

juce::File ProjectManager::getProjectManifestFile() const
{
    return currentProject.rootDirectory.getChildFile("project.xml");
}

juce::File ProjectManager::getProjectPackageFile() const
{
    if (! hasStorageRoot() || currentProject.slug.isEmpty())
        return {};

    return getProjectsRoot().getChildFile(currentProject.slug + ".csp");
}

juce::File ProjectManager::getLayoutPackageFile(const juce::String& layoutName) const
{
    if (! hasStorageRoot())
        return {};

    auto normalizedName = makeSlug(layoutName.isNotEmpty() ? layoutName : "last-used");
    return getLayoutsRoot().getChildFile(normalizedName + ".cslayout");
}

bool ProjectManager::loadStorageConfiguration(juce::String& errorMessage)
{
    auto pointerFile = getStoragePointerFile();
    if (! pointerFile.existsAsFile())
        return false;

    auto storedPath = pointerFile.loadFileAsString().trim();
    if (storedPath.isEmpty())
        return false;

    auto configuredRoot = juce::File(storedPath);
    if (! configuredRoot.isDirectory())
    {
        errorMessage = "The saved storage location is no longer available.";
        return false;
    }

    return setStorageRoot(configuredRoot, errorMessage);
}

bool ProjectManager::shouldAutoloadLastProject() const
{
    auto file = getAutoloadLastProjectFile();
    if (! file.existsAsFile())
        return false;

    auto value = file.loadFileAsString().trim().toLowerCase();
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

bool ProjectManager::setAutoloadLastProject(bool shouldAutoload, juce::String& errorMessage) const
{
    if (! hasStorageRoot())
    {
        errorMessage = "Choose a storage folder before saving project settings.";
        return false;
    }

    auto configDirectory = getConfigDirectory();
    if (! configDirectory.exists() && ! configDirectory.createDirectory())
    {
        errorMessage = "Could not create the config folder for project settings.";
        return false;
    }

    if (! getAutoloadLastProjectFile().replaceWithText(shouldAutoload ? "true" : "false"))
    {
        errorMessage = "Could not save the autoload preference.";
        return false;
    }

    return true;
}

bool ProjectManager::setStorageRoot(const juce::File& rootDirectory, juce::String& errorMessage)
{
    if (! rootDirectory.isDirectory() && ! rootDirectory.createDirectory())
    {
        errorMessage = "Could not create the selected storage folder.";
        return false;
    }

    storageRoot = rootDirectory;

    if (! ensureStorageDirectories(errorMessage))
    {
        storageRoot = {};
        return false;
    }

    auto pointerFile = getStoragePointerFile();
    if (! pointerFile.replaceWithText(storageRoot.getFullPathName()))
    {
        errorMessage = "Could not save the storage location pointer.";
        return false;
    }

    return true;
}

juce::File ProjectManager::importAssetFile(const juce::File& sourceFile, juce::String& errorMessage) const
{
    if (! hasProject())
    {
        errorMessage = "Open or create a project before importing sounds.";
        return {};
    }

    if (! sourceFile.existsAsFile())
    {
        errorMessage = "That sound file does not exist.";
        return {};
    }

    if (! currentProject.assetsDirectory.exists() && ! currentProject.assetsDirectory.createDirectory())
    {
        errorMessage = "Could not create the Assets folder.";
        return {};
    }

    auto baseName = sourceFile.getFileNameWithoutExtension();
    auto extension = sourceFile.getFileExtension();
    auto destination = currentProject.assetsDirectory.getChildFile(sourceFile.getFileName());
    auto suffix = 2;

    while (destination.existsAsFile())
    {
        destination = currentProject.assetsDirectory.getChildFile(baseName + "-" + juce::String(suffix) + extension);
        ++suffix;
    }

    if (! sourceFile.copyFileTo(destination))
    {
        errorMessage = "Could not copy the sound into this project.";
        return {};
    }

    return destination;
}

juce::File ProjectManager::saveGeneratedAssetFile(const juce::AudioBuffer<float>& buffer,
                                                  double sampleRate,
                                                  const juce::String& suggestedName,
                                                  juce::String& errorMessage) const
{
    if (! hasProject())
    {
        errorMessage = "Open or create a project before rendering sounds.";
        return {};
    }

    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
    {
        errorMessage = "There is no generated sound to render yet.";
        return {};
    }

    if (! currentProject.assetsDirectory.exists() && ! currentProject.assetsDirectory.createDirectory())
    {
        errorMessage = "Could not create the Assets folder.";
        return {};
    }

    auto baseName = makeSlug(suggestedName.isNotEmpty() ? suggestedName : "signal-lab-render");
    auto destination = currentProject.assetsDirectory.getChildFile(baseName + ".wav");
    auto suffix = 2;

    while (destination.existsAsFile())
    {
        destination = currentProject.assetsDirectory.getChildFile(baseName + "-" + juce::String(suffix) + ".wav");
        ++suffix;
    }

    juce::WavAudioFormat wavFormat;
    auto outputStream = std::unique_ptr<juce::FileOutputStream>(destination.createOutputStream());
    if (outputStream == nullptr)
    {
        errorMessage = "Could not open the render file for writing.";
        return {};
    }

    auto writer = std::unique_ptr<juce::AudioFormatWriter>(wavFormat.createWriterFor(outputStream.get(),
                                                                                      sampleRate,
                                                                                      (unsigned int) buffer.getNumChannels(),
                                                                                      24,
                                                                                      {},
                                                                                      0));
    if (writer == nullptr)
    {
        errorMessage = "Could not create a WAV writer for this render.";
        return {};
    }

    outputStream.release();

    if (! writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
    {
        errorMessage = "The generated sound could not be written to disk.";
        return {};
    }

    return destination;
}

juce::Array<juce::File> ProjectManager::listAssetFiles() const
{
    juce::Array<juce::File> files;

    if (! hasProject())
        return files;

    currentProject.assetsDirectory.findChildFiles(files, juce::File::findFiles, false, "*");
    return files;
}

juce::Array<ProjectManager::ProjectAsset> ProjectManager::listProjectAssets() const
{
    juce::Array<ProjectAsset> assets;

    if (! hasProject())
        return assets;

    auto appendFiles = [&assets, this](const juce::File& directory,
                                       const juce::String& type,
                                       const juce::String& category,
                                       const juce::String& description,
                                       const juce::String& pattern)
    {
        if (! directory.isDirectory())
            return;

        juce::Array<juce::File> files;
        directory.findChildFiles(files, juce::File::findFiles, true, pattern);

        for (const auto& file : files)
        {
            ProjectAsset asset;
            asset.file = file;
            asset.name = file.getFileNameWithoutExtension().replace("-", " ");
            asset.type = type;
            asset.category = category;
            asset.description = description;
            asset.relativePath = file.getRelativePathFrom(currentProject.rootDirectory).replaceCharacter('\\', '/');
            asset.id = currentProject.slug + ":" + asset.relativePath;
            asset.fileSizeBytes = file.getSize();
            assets.add(asset);
        }
    };

    appendFiles(currentProject.assetsDirectory, "audioFile", "Audio / Takes", "Recorded, imported, or generated audio usable on the Tracker.", "*");
    appendFiles(currentProject.dslDirectory.getChildFile("Patches"), "signalPatch", "Signals / Patches", "Editable sound design patch from Signal Lab or Patch tools.", "*.cspatch");
    appendFiles(currentProject.dslDirectory.getChildFile("Patina"), "patinaProgram", "Scripts / Patina", "Compiled or exported Patina artifact.", "*.patina.json");
    appendFiles(currentProject.rendersDirectory, "render", "Renders", "Rendered mix, stem, loop, or sound-design artifact.", "*");

    return assets;
}

juce::File ProjectManager::savePatchFile(const juce::String& patchJson,
                                         const juce::String& suggestedName,
                                         juce::String& errorMessage) const
{
    if (! hasProject())
    {
        errorMessage = "Open or create a project before exporting patches.";
        return {};
    }

    if (patchJson.trim().isEmpty())
    {
        errorMessage = "There is no patch data to export.";
        return {};
    }

    auto patchesDirectory = currentProject.dslDirectory.getChildFile("Patches");
    if (! patchesDirectory.exists() && ! patchesDirectory.createDirectory())
    {
        errorMessage = "Could not create the Patches folder.";
        return {};
    }

    auto baseName = makeSlug(suggestedName.isNotEmpty() ? suggestedName : "creation-station-patch");
    auto destination = patchesDirectory.getChildFile(baseName + ".cspatch");
    auto suffix = 2;

    while (destination.existsAsFile())
    {
        destination = patchesDirectory.getChildFile(baseName + "-" + juce::String(suffix) + ".cspatch");
        ++suffix;
    }

    if (! destination.replaceWithText(patchJson))
    {
        errorMessage = "Could not write the patch file.";
        return {};
    }

    return destination;
}

juce::File ProjectManager::saveUserPatchFile(const juce::String& patchJson,
                                             const juce::String& suggestedName,
                                             juce::String& errorMessage) const
{
    if (! hasStorageRoot())
    {
        errorMessage = "Choose a local storage folder before saving library patches.";
        return {};
    }

    if (patchJson.trim().isEmpty())
    {
        errorMessage = "There is no patch data to save yet.";
        return {};
    }

    auto userDirectory = getUserContentDirectory();
    if (! userDirectory.exists() && ! userDirectory.createDirectory())
    {
        errorMessage = "Could not create the user content folder.";
        return {};
    }

    auto baseName = makeSlug(suggestedName.isNotEmpty() ? suggestedName : "creation-station-patch");
    auto destination = userDirectory.getChildFile(baseName + ".cspatch");
    auto suffix = 2;

    while (destination.existsAsFile())
    {
        destination = userDirectory.getChildFile(baseName + "-" + juce::String(suffix) + ".cspatch");
        ++suffix;
    }

    if (! destination.replaceWithText(patchJson))
    {
        errorMessage = "Could not write the user patch file.";
        return {};
    }

    return destination;
}

juce::File ProjectManager::savePatinaArtifactFile(const juce::String& artifactJson,
                                                  const juce::String& suggestedName,
                                                  juce::String& errorMessage) const
{
    if (! hasProject())
    {
        errorMessage = "Open or create a project before exporting Patina artifacts.";
        return {};
    }

    if (artifactJson.trim().isEmpty())
    {
        errorMessage = "There is no Patina artifact data to export.";
        return {};
    }

    auto artifactsDirectory = currentProject.dslDirectory.getChildFile("Patina");
    if (! artifactsDirectory.exists() && ! artifactsDirectory.createDirectory())
    {
        errorMessage = "Could not create the Patina artifacts folder.";
        return {};
    }

    auto baseName = makeSlug(suggestedName.isNotEmpty() ? suggestedName : "patina-artifact");
    auto destination = artifactsDirectory.getChildFile(baseName + ".patina.json");
    auto suffix = 2;

    while (destination.existsAsFile())
    {
        destination = artifactsDirectory.getChildFile(baseName + "-" + juce::String(suffix) + ".patina.json");
        ++suffix;
    }

    if (! destination.replaceWithText(artifactJson))
    {
        errorMessage = "Could not write the Patina artifact file.";
        return {};
    }

    return destination;
}

juce::File ProjectManager::saveUserPatinaArtifactFile(const juce::String& artifactJson,
                                                      const juce::String& suggestedName,
                                                      juce::String& errorMessage) const
{
    if (! hasStorageRoot())
    {
        errorMessage = "Choose a local storage folder before saving Patina artifacts to your library.";
        return {};
    }

    if (artifactJson.trim().isEmpty())
    {
        errorMessage = "There is no Patina artifact data to save yet.";
        return {};
    }

    auto userDirectory = getUserContentDirectory().getChildFile("Patina");
    if (! userDirectory.exists() && ! userDirectory.createDirectory())
    {
        errorMessage = "Could not create the Patina library folder.";
        return {};
    }

    auto baseName = makeSlug(suggestedName.isNotEmpty() ? suggestedName : "patina-artifact");
    auto destination = userDirectory.getChildFile(baseName + ".patina.json");
    auto suffix = 2;

    while (destination.existsAsFile())
    {
        destination = userDirectory.getChildFile(baseName + "-" + juce::String(suffix) + ".patina.json");
        ++suffix;
    }

    if (! destination.replaceWithText(artifactJson))
    {
        errorMessage = "Could not write the Patina library artifact file.";
        return {};
    }

    return destination;
}

bool ProjectManager::ensureDirectories(juce::String& errorMessage)
{
    if (! currentProject.rootDirectory.createDirectory())
    {
        errorMessage = "Could not create the project folder.";
        return false;
    }

    if (! currentProject.audioDirectory.createDirectory()
        || ! currentProject.dslDirectory.createDirectory()
        || ! currentProject.rendersDirectory.createDirectory()
        || ! currentProject.assetsDirectory.createDirectory())
    {
        errorMessage = "Could not create one or more project subfolders.";
        return false;
    }

    return true;
}

bool ProjectManager::ensureStorageDirectories(juce::String& errorMessage) const
{
    if (! storageRoot.isDirectory())
    {
        errorMessage = "No storage root has been configured yet.";
        return false;
    }

    auto projectsDirectory = getProjectsRoot();
    auto configDirectory = getConfigDirectory();
    auto contentDirectory = getContentDirectory();
    auto layoutsDirectory = getLayoutsRoot();
    auto builtInContentDirectory = getBuiltInContentDirectory();
    auto downloadedContentDirectory = getDownloadedContentDirectory();
    auto userContentDirectory = getUserContentDirectory();
    auto builtInTutorialDirectory = getBuiltInTutorialDirectory();
    auto userTutorialDirectory = getUserTutorialDirectory();

    if (! projectsDirectory.createDirectory()
        || ! configDirectory.createDirectory()
        || ! contentDirectory.createDirectory()
        || ! layoutsDirectory.createDirectory()
        || ! builtInContentDirectory.createDirectory()
        || ! downloadedContentDirectory.createDirectory()
        || ! userContentDirectory.createDirectory()
        || ! builtInTutorialDirectory.createDirectory()
        || ! userTutorialDirectory.createDirectory())
    {
        errorMessage = "Could not create one or more storage folders.";
        return false;
    }

    return true;
}

juce::ValueTree ProjectManager::createManifestTree() const
{
    juce::ValueTree manifest("Project");
    manifest.setProperty("name", currentProject.name, nullptr);
    manifest.setProperty("slug", currentProject.slug, nullptr);
    manifest.setProperty("description", currentProject.description, nullptr);
    manifest.setProperty("author", currentProject.author, nullptr);
    manifest.setProperty("copyright", currentProject.copyright, nullptr);
    manifest.setProperty("distributionRights", currentProject.distributionRights, nullptr);
    manifest.setProperty("root", currentProject.rootDirectory.getFullPathName(), nullptr);
    manifest.setProperty("createdAt", juce::Time::getCurrentTime().toISO8601(true), nullptr);
    manifest.setProperty("updatedAt", juce::Time::getCurrentTime().toISO8601(true), nullptr);
    return manifest;
}

juce::String ProjectManager::createProjectJson() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty("format", "creation-station-project");
    root->setProperty("formatVersion", 1);
    root->setProperty("application", "Creation Station");
    root->setProperty("name", currentProject.name);
    root->setProperty("slug", currentProject.slug);
    root->setProperty("description", currentProject.description);
    root->setProperty("author", currentProject.author);
    root->setProperty("copyright", currentProject.copyright);
    root->setProperty("distributionRights", currentProject.distributionRights);
    root->setProperty("savedAt", juce::Time::getCurrentTime().toISO8601(true));
    return juce::JSON::toString(juce::var(root), true);
}

juce::String ProjectManager::createPackageManifestJson(const juce::Array<juce::File>& packagedFiles) const
{
    juce::Array<juce::var> entries;

    for (const auto& file : packagedFiles)
    {
        auto* entry = new juce::DynamicObject();
        entry->setProperty("path", normaliseArchivePath("project/" + file.getRelativePathFrom(currentProject.rootDirectory)));
        entry->setProperty("sizeBytes", static_cast<double>(file.getSize()));
        entry->setProperty("modifiedAt", file.getLastModificationTime().toISO8601(true));
        entries.add(juce::var(entry));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty("format", "creation-station-package-manifest");
    root->setProperty("formatVersion", 1);
    root->setProperty("projectSlug", currentProject.slug);
    root->setProperty("entryCount", entries.size());
    root->setProperty("entries", entries);
    return juce::JSON::toString(juce::var(root), true);
}

bool ProjectManager::writeManifest(const juce::String& errorMessageHint)
{
    auto manifest = createManifestTree();
    auto file = getProjectManifestFile();
    if (auto xml = manifest.createXml())
    {
        if (xml->writeTo(file))
            return true;
    }

    juce::ignoreUnused(errorMessageHint);
    return false;
}

bool ProjectManager::readManifest(const juce::File& projectDirectory, juce::String& errorMessage)
{
    auto file = projectDirectory.getChildFile("project.xml");
    if (! file.existsAsFile())
    {
        errorMessage = "That folder does not contain a project.xml manifest.";
        return false;
    }

    auto xml = juce::parseXML(file);
    if (xml == nullptr)
    {
        errorMessage = "The project manifest could not be read.";
        return false;
    }

    auto state = juce::ValueTree::fromXml(*xml);
    currentProject.name = sanitiseName(state.getProperty("name").toString());
    currentProject.slug = makeSlug(currentProject.name);
    currentProject.description = state.getProperty("description").toString();
    currentProject.author = state.getProperty("author").toString();
    currentProject.copyright = state.getProperty("copyright").toString();
    currentProject.distributionRights = state.getProperty("distributionRights").toString();
    currentProject.rootDirectory = projectDirectory;
    currentProject.audioDirectory = projectDirectory.getChildFile("Audio");
    currentProject.dslDirectory = projectDirectory.getChildFile("DSL");
    currentProject.rendersDirectory = projectDirectory.getChildFile("Renders");
    currentProject.assetsDirectory = projectDirectory.getChildFile("Assets");
    return true;
}

bool ProjectManager::createProject(const juce::String& projectName, juce::String& errorMessage)
{
    if (! hasStorageRoot())
    {
        errorMessage = "Choose a local storage folder before creating projects.";
        return false;
    }

    currentProject.name = sanitiseName(projectName);
    currentProject.slug = makeSlug(currentProject.name);
    currentProject.rootDirectory = getProjectsRoot().getChildFile(currentProject.slug);
    currentProject.audioDirectory = currentProject.rootDirectory.getChildFile("Audio");
    currentProject.dslDirectory = currentProject.rootDirectory.getChildFile("DSL");
    currentProject.rendersDirectory = currentProject.rootDirectory.getChildFile("Renders");
    currentProject.assetsDirectory = currentProject.rootDirectory.getChildFile("Assets");

    if (! ensureDirectories(errorMessage))
        return false;

    return writeManifest(errorMessage);
}

bool ProjectManager::openProject(const juce::File& projectDirectory, juce::String& errorMessage)
{
    if (! projectDirectory.exists() || ! projectDirectory.isDirectory())
    {
        errorMessage = "That project folder does not exist.";
        return false;
    }

    if (! readManifest(projectDirectory, errorMessage))
        return false;

    return true;
}

bool ProjectManager::loadLastProject()
{
    if (! hasStorageRoot())
        return false;

    auto marker = getConfigDirectory().getChildFile("last-project.txt");
    if (! marker.existsAsFile())
        return false;

    auto path = marker.loadFileAsString().trim();
    if (path.isEmpty())
        return false;

    juce::String errorMessage;
    return openProject(juce::File(path), errorMessage);
}

void ProjectManager::clearProject()
{
    currentProject = {};
}

void ProjectManager::saveProjectState(const juce::ValueTree& state) const
{
    if (! hasProject() || ! hasStorageRoot())
        return;

    auto stateFile = currentProject.rootDirectory.getChildFile("state.xml");
    if (auto xml = state.createXml())
        xml->writeTo(stateFile);

    getConfigDirectory().createDirectory();
    getConfigDirectory().getChildFile("last-project.txt").replaceWithText(currentProject.rootDirectory.getFullPathName());
}

bool ProjectManager::saveProjectPackage(const juce::ValueTree& state, juce::String& errorMessage) const
{
    if (! hasProject() || ! hasStorageRoot())
    {
        errorMessage = "Open or create a project before saving.";
        return false;
    }

    auto packageFile = getProjectPackageFile();
    if (packageFile.getFullPathName().isEmpty())
    {
        errorMessage = "Could not resolve the project package file.";
        return false;
    }

    packageFile.getParentDirectory().createDirectory();
    auto tempFile = packageFile.getSiblingFile(packageFile.getFileName() + ".tmp");
    if (tempFile.existsAsFile())
        tempFile.deleteFile();

    juce::ZipFile::Builder builder;
    juce::Array<juce::File> packagedFiles;

    addTextEntry(builder, "project.json", createProjectJson());
    if (auto xml = state.createXml())
        addTextEntry(builder, "state/session.xml", xml->toString());

    addDirectoryToPackage(builder, currentProject.rootDirectory, currentProject.audioDirectory, "project", packagedFiles);
    addDirectoryToPackage(builder, currentProject.rootDirectory, currentProject.assetsDirectory, "project", packagedFiles);
    addDirectoryToPackage(builder, currentProject.rootDirectory, currentProject.dslDirectory, "project", packagedFiles);
    addDirectoryToPackage(builder, currentProject.rootDirectory, currentProject.rendersDirectory, "project", packagedFiles);

    auto projectXml = getProjectManifestFile();
    if (projectXml.existsAsFile())
    {
        builder.addFile(projectXml, 6, "project/project.xml");
        packagedFiles.add(projectXml);
    }

    auto stateXml = currentProject.rootDirectory.getChildFile("state.xml");
    if (stateXml.existsAsFile())
    {
        builder.addFile(stateXml, 6, "project/state.xml");
        packagedFiles.add(stateXml);
    }

    addTextEntry(builder, "manifest.json", createPackageManifestJson(packagedFiles));

    std::unique_ptr<juce::FileOutputStream> output(tempFile.createOutputStream());
    if (output == nullptr)
    {
        errorMessage = "Could not create the project package file.";
        return false;
    }

    double progress = 0.0;
    if (! builder.writeToStream(*output, &progress))
    {
        errorMessage = "Could not write the project package.";
        return false;
    }

    output.reset();

    if (packageFile.existsAsFile() && ! packageFile.deleteFile())
    {
        errorMessage = "Could not replace the previous project package.";
        return false;
    }

    if (! tempFile.moveFileTo(packageFile))
    {
        errorMessage = "Could not finalize the project package.";
        return false;
    }

    return true;
}

bool ProjectManager::updateProjectMetadata(const ProjectInfo& metadata, juce::String& errorMessage)
{
    if (! hasProject())
    {
        errorMessage = "Open or create a project before editing metadata.";
        return false;
    }

    currentProject.name = sanitiseName(metadata.name);
    currentProject.description = metadata.description;
    currentProject.author = metadata.author;
    currentProject.copyright = metadata.copyright;
    currentProject.distributionRights = metadata.distributionRights;

    if (! writeManifest(errorMessage))
    {
        errorMessage = "Could not save the project metadata.";
        return false;
    }

    return true;
}

juce::ValueTree ProjectManager::loadProjectState() const
{
    if (! hasProject())
        return {};

    auto stateFile = currentProject.rootDirectory.getChildFile("state.xml");
    if (! stateFile.existsAsFile())
        return {};

    auto xml = juce::parseXML(stateFile);
    if (xml == nullptr)
        return {};

    return juce::ValueTree::fromXml(*xml);
}

juce::String ProjectManager::getDisplayLabel() const
{
    if (! hasProject())
        return "No project open";

    return currentProject.name + " · " + currentProject.rootDirectory.getFullPathName();
}
