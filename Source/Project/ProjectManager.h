#pragma once

#include <JuceHeader.h>

class ProjectManager final
{
public:
    struct ProjectInfo
    {
        juce::String name;
        juce::String slug;
        juce::File rootDirectory;
        juce::File audioDirectory;
        juce::File dslDirectory;
        juce::File rendersDirectory;
        juce::File assetsDirectory;
    };

    bool hasProject() const noexcept { return currentProject.rootDirectory.exists(); }
    const ProjectInfo& getCurrentProject() const noexcept { return currentProject; }

    bool createProject(const juce::String& projectName, juce::String& errorMessage);
    bool openProject(const juce::File& projectDirectory, juce::String& errorMessage);
    bool loadLastProject();
    void clearProject();
    void saveProjectState(const juce::ValueTree& state) const;
    juce::ValueTree loadProjectState() const;

    juce::String getDisplayLabel() const;
    juce::File getWorkspaceRoot() const;
    juce::File getProjectsRoot() const;
    juce::File getProjectManifestFile() const;

private:
    static juce::String makeSlug(const juce::String& name);
    static juce::File appDataRoot();
    static juce::File defaultWorkspaceRoot();

    bool ensureDirectories(juce::String& errorMessage);
    bool writeManifest(const juce::String& errorMessageHint);
    bool readManifest(const juce::File& projectDirectory, juce::String& errorMessage);
    juce::ValueTree createManifestTree() const;

    ProjectInfo currentProject;
};
