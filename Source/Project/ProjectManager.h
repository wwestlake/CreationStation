#pragma once

#include <JuceHeader.h>
#include "../AI/AiProviderSettings.h"
#include "../ControlSurface/ControlSurfaceMappingStore.h"

class ProjectManager final
{
public:
    struct ProjectInfo
    {
        juce::String name;
        juce::String slug;
        juce::String description;
        juce::String author;
        juce::String copyright;
        juce::String distributionRights;
        juce::File rootDirectory;
        juce::File audioDirectory;
        juce::File dslDirectory;
        juce::File rendersDirectory;
        juce::File assetsDirectory;
    };

    struct ProjectAsset
    {
        juce::String id;
        juce::String name;
        juce::String type;
        juce::String category;
        juce::String description;
        juce::String relativePath;
        juce::File file;
        int64 fileSizeBytes = 0;
    };

    bool hasProject() const noexcept { return currentProject.rootDirectory.exists(); }
    bool hasStorageRoot() const noexcept { return storageRoot.isDirectory(); }
    const ProjectInfo& getCurrentProject() const noexcept { return currentProject; }

    bool loadStorageConfiguration(juce::String& errorMessage);
    bool setStorageRoot(const juce::File& rootDirectory, juce::String& errorMessage);
    bool shouldAutoloadLastProject() const;
    bool setAutoloadLastProject(bool shouldAutoload, juce::String& errorMessage) const;
    bool createProject(const juce::String& projectName, juce::String& errorMessage);
    bool saveCurrentProjectAs(const juce::String& projectName, juce::String& errorMessage);
    bool openProject(const juce::File& projectDirectory, juce::String& errorMessage);
    bool openProjectPackage(const juce::File& packageFile, juce::ValueTree& restoredState, juce::String& errorMessage);
    bool updateProjectMetadata(const ProjectInfo& metadata, juce::String& errorMessage);
    bool loadLastProject();
    void clearProject();
    void saveProjectState(const juce::ValueTree& state) const;
    juce::ValueTree loadProjectState() const;
    bool saveProjectPackage(const juce::ValueTree& state, juce::String& errorMessage) const;
    bool saveTemplatePackage(const juce::ValueTree& state,
                             const juce::String& templateName,
                             juce::File& templateFile,
                             juce::String& errorMessage) const;
    bool createProjectFromTemplate(const juce::File& templateFile,
                                   const juce::String& projectName,
                                   juce::ValueTree& restoredState,
                                   juce::String& errorMessage);
    juce::File importAssetFile(const juce::File& sourceFile, juce::String& errorMessage) const;
    juce::File saveGeneratedAssetFile(const juce::AudioBuffer<float>& buffer,
                                      double sampleRate,
                                      const juce::String& suggestedName,
                                      juce::String& errorMessage) const;
    juce::File saveRenderFile(const juce::AudioBuffer<float>& buffer,
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
    juce::Array<ProjectAsset> listProjectAssets() const;

    juce::String getDisplayLabel() const;
    juce::File getStorageRoot() const;
    juce::File getConfigDirectory() const;
    juce::File getContentDirectory() const;
    juce::File getBuiltInContentDirectory() const;
    juce::File getDownloadedContentDirectory() const;
    juce::File getUserContentDirectory() const;
    juce::File getBuiltInTutorialDirectory() const;
    juce::File getUserTutorialDirectory() const;
    juce::File getContentManifestFile() const;
    juce::File getAiContextStoreFile() const;
    juce::File getAiProviderSettingsFile() const;
    bool loadAiProviderSettings(AiProviderSettings& settings) const;
    bool saveAiProviderSettings(const AiProviderSettings& settings, juce::String& errorMessage) const;
    juce::File getVstSearchPathFile() const;
    juce::StringArray loadVstSearchPaths() const;
    bool saveVstSearchPaths(const juce::StringArray& paths, juce::String& errorMessage) const;
    juce::File getControlSurfaceMappingsFile() const;
    bool loadControlSurfaceMappings(ControlSurfaceMappingStore& store, juce::String& errorMessage) const;
    bool saveControlSurfaceMappings(const ControlSurfaceMappingStore& store, juce::String& errorMessage) const;
    juce::File getWorkspaceRoot() const;
    juce::File getLayoutsRoot() const;
    juce::File getProjectsRoot() const;
    juce::File getTemplatesRoot() const;
    juce::File getProjectManifestFile() const;
    juce::File getProjectPackageFile() const;
    juce::File getTemplatePackageFile(const juce::String& templateName) const;
    juce::File getLayoutPackageFile(const juce::String& layoutName) const;

private:
    static juce::String makeSlug(const juce::String& name);
    static juce::File getStoragePointerFile();
    juce::File getAutoloadLastProjectFile() const;

    bool ensureDirectories(juce::String& errorMessage);
    bool ensureStorageDirectories(juce::String& errorMessage) const;
    bool writeManifest(const juce::String& errorMessageHint);
    bool readManifest(const juce::File& projectDirectory, juce::String& errorMessage);
    juce::ValueTree createManifestTree() const;
    juce::String createProjectJson() const;
    juce::String createTemplateJson(const juce::String& templateName) const;
    juce::String createPackageManifestJson(const juce::Array<juce::File>& packagedFiles) const;

    juce::File storageRoot;
    ProjectInfo currentProject;
};
