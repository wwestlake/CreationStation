#include "ProjectManager.h"

namespace
{
juce::String sanitiseName(const juce::String& name)
{
    auto trimmed = name.trim();
    return trimmed.isNotEmpty() ? trimmed : "Untitled Project";
}
}

juce::File ProjectManager::getStoragePointerFile()
{
    auto executableDirectory = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    return executableDirectory.getChildFile("CreationStation.storage");
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

juce::File ProjectManager::getContentManifestFile() const
{
    return getConfigDirectory().getChildFile("content-library.json");
}

juce::File ProjectManager::getProjectsRoot() const
{
    return getWorkspaceRoot();
}

juce::File ProjectManager::getProjectManifestFile() const
{
    return currentProject.rootDirectory.getChildFile("project.xml");
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
    auto builtInContentDirectory = getBuiltInContentDirectory();
    auto downloadedContentDirectory = getDownloadedContentDirectory();
    auto userContentDirectory = getUserContentDirectory();

    if (! projectsDirectory.createDirectory()
        || ! configDirectory.createDirectory()
        || ! contentDirectory.createDirectory()
        || ! builtInContentDirectory.createDirectory()
        || ! downloadedContentDirectory.createDirectory()
        || ! userContentDirectory.createDirectory())
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
    manifest.setProperty("root", currentProject.rootDirectory.getFullPathName(), nullptr);
    manifest.setProperty("createdAt", juce::Time::getCurrentTime().toISO8601(true), nullptr);
    manifest.setProperty("updatedAt", juce::Time::getCurrentTime().toISO8601(true), nullptr);
    return manifest;
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
