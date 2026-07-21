#pragma once

#include <JuceHeader.h>
#include "../AI/AiProviderSettings.h"
#include "../Project/ProjectManager.h"

class SettingsPanel final : public juce::Component
{
public:
    SettingsPanel();

    std::function<void()> onNewProjectRequested;
    std::function<void()> onOpenProjectRequested;
    std::function<void()> onSaveProjectRequested;
    std::function<void()> onRevealProjectFolderRequested;
    std::function<void()> onChangeStorageRequested;
    std::function<void()> onOpenAudioRequested;
    std::function<void()> onOpenDriverControlPanelRequested;
    std::function<void()> onRefreshStudioInputsRequested;
    std::function<void(const juce::String&)> onAudioSystemChanged;
    std::function<void(const juce::String&)> onAudioInputDeviceChanged;
    std::function<void(const juce::String&)> onAudioOutputDeviceChanged;
    std::function<void(int, const juce::String&)> onStudioInputNameChanged;
    std::function<void()> onManageVstPathsRequested;
    std::function<void()> onManageControlSurfaceMappingsRequested;
    std::function<void(bool)> onAutoloadChanged;
    std::function<void()> onRefreshAiModelsRequested;
    std::function<void(const AiProviderSettings&)> onAiProviderSettingsChanged;
    std::function<void(const ProjectManager::ProjectInfo&)> onProjectMetadataChanged;

    void setProjectMetadata(const ProjectManager::ProjectInfo& metadata);
    void setStoragePath(const juce::String& path);
    void setAutoloadEnabled(bool enabled);
    void setAiProviderSettings(const AiProviderSettings& settings);
    void setAvailableAiModels(const juce::StringArray& modelIds, const juce::String& statusText);
    void setStudioInputSummary(const juce::StringArray& inputSummaries);
    void setStudioInputRows(const juce::StringArray& names,
                            const juce::StringArray& hardwareNames,
                            const juce::Array<bool>& availability);
    void setAudioDeviceLists(const juce::StringArray& audioSystems,
                             const juce::StringArray& inputDevices,
                             const juce::StringArray& outputDevices,
                             const juce::String& selectedSystem,
                             const juce::String& selectedInput,
                             const juce::String& selectedOutput);
    void setAudioDiagnostics(const juce::String& diagnosticsText, bool canOpenDriverControlPanel);
    AiProviderSettings getAiProviderSettings() const;
    void setSearchText(const juce::String& text);
    juce::String getSearchText() const;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct ActionRow
    {
        juce::String title;
        juce::String keywords;
        juce::Label description;
        juce::TextButton actionButton;
        bool visible = true;

        ActionRow(const juce::String& rowTitle, const juce::String& rowKeywords, const juce::String& buttonText)
            : title(rowTitle), keywords(rowKeywords), actionButton(buttonText)
        {
        }
    };

    class ContentView final : public juce::Component
    {
    public:
        explicit ContentView(SettingsPanel& ownerRef);

        void setSearchText(const juce::String& text);
        void setComboItems(juce::ComboBox& comboBox, const juce::StringArray& items, const juce::String& selectedText);
        void setStudioInputRows(const juce::StringArray& names,
                                const juce::StringArray& hardwareNames,
                                const juce::Array<bool>& availability);
        void resized() override;

        juce::Label headerLabel;
        juce::Label subHeaderLabel;
        juce::Label searchLabel;
        juce::TextEditor searchEditor;
        juce::Label storageLabel;
        juce::Label storageValueLabel;
        juce::ToggleButton autoloadToggle { "Autoload last project" };
        juce::Label projectSectionLabel;
        juce::Label projectNameLabel;
        juce::Label projectDescriptionLabel;
        juce::Label projectAuthorLabel;
        juce::Label projectCopyrightLabel;
        juce::Label projectRightsLabel;
        juce::TextEditor projectNameEditor;
        juce::TextEditor projectDescriptionEditor;
        juce::TextEditor projectAuthorEditor;
        juce::TextEditor projectCopyrightEditor;
        juce::TextEditor projectRightsEditor;
        juce::TextButton projectMetadataSaveButton { "Apply Metadata" };
        juce::Label startupSectionLabel;
        juce::Label toolsSectionLabel;
        juce::Label aiSectionLabel;
        juce::Label studioInputsLabel;
        juce::Label studioInputsValueLabel;
        juce::OwnedArray<juce::Label> studioInputHardwareLabels;
        juce::OwnedArray<juce::TextEditor> studioInputNameEditors;
        juce::Label audioSystemLabel;
        juce::Label audioInputLabel;
        juce::Label audioOutputLabel;
        juce::ComboBox audioSystemComboBox;
        juce::ComboBox audioInputComboBox;
        juce::ComboBox audioOutputComboBox;
        juce::Label audioDiagnosticsLabel;
        juce::TextButton audioDriverControlPanelButton { "Open Driver Panel" };
        juce::Label aiProviderLabel;
        juce::Label aiHintLabel;
        juce::Label aiModelLabel;
        juce::Label aiEndpointLabel;
        juce::Label aiKeyLabel;
        juce::ComboBox aiProviderComboBox;
        juce::ComboBox aiModelComboBox;
        juce::TextEditor aiEndpointEditor;
        juce::TextEditor aiKeyEditor;
        juce::Label aiModelStatusLabel;
        juce::TextButton aiRefreshModelsButton { "Refresh Models" };

        ActionRow newProjectRow { "New Project", "project create new", "New Project" };
        ActionRow openProjectRow { "Open Project", "project open load", "Open Project" };
        ActionRow saveProjectRow { "Save Project", "project save", "Save Project" };
        ActionRow revealProjectRow { "Open Project Folder", "project folder reveal", "Open Folder" };
        ActionRow storageRow { "Storage Folder", "storage folder project content", "Change Storage" };
        ActionRow studioInputsRow { "Studio Inputs", "audio source input channel microphone instrument routing", "Refresh Inputs" };
        ActionRow audioRow { "Audio Devices", "audio device settings", "Open Audio" };
        ActionRow vstRow { "VST Paths", "plugin vst vst3 scan folders", "Manage VST Paths" };
        ActionRow controlSurfaceRow { "Control Surface Maps", "xtouch bcr2000 mapping profiles midi control", "Edit Maps" };

    private:
        SettingsPanel& owner;
        juce::Viewport viewport;
        juce::Component content;

        void applyAiSettings();
        void applyProjectMetadata();
        void applyFilter();
        void layoutContent();
    };

    ContentView contentView;
    juce::String searchText;
};
