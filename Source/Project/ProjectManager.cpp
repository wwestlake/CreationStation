#include "ProjectManager.h"

namespace
{
juce::String sanitiseName(const juce::String& name)
{
    auto trimmed = name.trim();
    return trimmed.isNotEmpty() ? trimmed : "Untitled Project";
}
}

juce::File ProjectManager::appDataRoot()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("CreationStation");
}

juce::File ProjectManager::defaultWorkspaceRoot()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("CreationStation")
        .getChildFile("Projects");
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
    return defaultWorkspaceRoot();
}

juce::File ProjectManager::getProjectsRoot() const
{
    return getWorkspaceRoot();
}

juce::File ProjectManager::getProjectManifestFile() const
{
    return currentProject.rootDirectory.getChildFile("project.xml");
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
    auto marker = appDataRoot().getChildFile("last-project.txt");
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
    if (! hasProject())
        return;

    auto stateFile = currentProject.rootDirectory.getChildFile("state.xml");
    if (auto xml = state.createXml())
        xml->writeTo(stateFile);

    auto marker = appDataRoot();
    marker.createDirectory();
    marker.getChildFile("last-project.txt").replaceWithText(currentProject.rootDirectory.getFullPathName());
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
