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
    bool hasStorageRoot() const noexcept { return storageRoot.isDirectory(); }
    const ProjectInfo& getCurrentProject() const noexcept { return currentProject; }

    bool loadStorageConfiguration(juce::String& errorMessage);
    bool setStorageRoot(const juce::File& rootDirectory, juce::String& errorMessage);
    bool createProject(const juce::String& projectName, juce::String& errorMessage);
    bool openProject(const juce::File& projectDirectory, juce::String& errorMessage);
    bool loadLastProject();
    void clearProject();
    void saveProjectState(const juce::ValueTree& state) const;
    juce::ValueTree loadProjectState() const;
    juce::File importAssetFile(const juce::File& sourceFile, juce::String& errorMessage) const;
    juce::File saveGeneratedAssetFile(const juce::AudioBuffer<float>& buffer,
                                      double sampleRate,
                                      const juce::String& suggestedName,
                                      juce::String& errorMessage) const;
    juce::File savePatchFile(const juce::String& patchJson,
                             const juce::String& suggestedName,
                             juce::String& errorMessage) const;
    juce::File saveUserPatchFile(const juce::String& patchJson,
                                 const juce::String& suggestedName,
                                 juce::String& errorMessage) const;
    juce::File savePatinaArtifactFile(const juce::String& artifactJson,
                                      const juce::String& suggestedName,
                                      juce::String& errorMessage) const;
    juce::File saveUserPatinaArtifactFile(const juce::String& artifactJson,
                                          const juce::String& suggestedName,
                                          juce::String& errorMessage) const;
    juce::Array<juce::File> listAssetFiles() const;

    juce::String getDisplayLabel() const;
    juce::File getStorageRoot() const;
    juce::File getConfigDirectory() const;
    juce::File getContentDirectory() const;
    juce::File getBuiltInContentDirectory() const;
    juce::File getDownloadedContentDirectory() const;
    juce::File getUserContentDirectory() const;
    juce::File getContentManifestFile() const;
    juce::File getWorkspaceRoot() const;
    juce::File getProjectsRoot() const;
    juce::File getProjectManifestFile() const;

private:
    static juce::String makeSlug(const juce::String& name);
    static juce::File getStoragePointerFile();

    bool ensureDirectories(juce::String& errorMessage);
    bool ensureStorageDirectories(juce::String& errorMessage) const;
    bool writeManifest(const juce::String& errorMessageHint);
    bool readManifest(const juce::File& projectDirectory, juce::String& errorMessage);
    juce::ValueTree createManifestTree() const;

    juce::File storageRoot;
    ProjectInfo currentProject;
};
