#include <creation/assets/ProjectManifest.h>
#include <creation/assets/AssetTypes.h>
#pragma once

#include <JuceHeader.h>
#include "../AI/AiProviderSettings.h"

class SettingsPanel final : public juce::Component
{
public:
    struct MidiDeviceInfo
    {
        juce::String id;
        juce::String name;
        bool enabled = false;
        int routedTrackIndex = -1; // -1 = feeds every track (Omni-device), the original behaviour
    };

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
    std::function<void(const creation::assets::ProjectManifest&)> onProjectMetadataChanged;
    std::function<void()> onRefreshMidiDevicesRequested;
    std::function<void(const juce::String& deviceId, bool enabled)> onMidiInputDeviceEnabledChanged;
    std::function<void(const juce::String& deviceId, int trackIndexOrMinusOne)> onMidiInputDeviceRouteChanged;

    void setMidiInputDevices(const juce::Array<MidiDeviceInfo>& devices, const juce::StringArray& midiTrackNames);

    void setProjectMetadata(const creation::assets::ProjectManifest& metadata);
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

    // A clickable section title with an expand/collapse chevron - click anywhere on it to toggle.
    // Search still overrides this: a section with a matching result forces itself open so results
    // are never hidden behind a collapsed header.
    class CollapsibleHeader final : public juce::Component
    {
    public:
        std::function<void()> onToggled;

        void setText(const juce::String& text) { title = text; repaint(); }
        juce::String getText() const { return title; }
        void setExpanded(bool shouldExpand)
        {
            if (expanded == shouldExpand)
                return;
            expanded = shouldExpand;
            repaint();
        }
        bool isExpanded() const noexcept { return expanded; }

        void paint(juce::Graphics& g) override
        {
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(18.0f).boldened());
            auto arrow = expanded ? juce::String(juce::CharPointer_UTF8("\xe2\x96\xbe"))
                                  : juce::String(juce::CharPointer_UTF8("\xe2\x96\xb8"));
            g.drawText(arrow + "  " + title, getLocalBounds(), juce::Justification::centredLeft);
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            expanded = ! expanded;
            repaint();
            if (onToggled != nullptr)
                onToggled();
        }

    private:
        juce::String title;
        bool expanded = true;
    };

    struct MidiDeviceRow
    {
        juce::String deviceId;
        juce::Label nameLabel;
        juce::ToggleButton enabledToggle { "Enabled" };
        juce::ComboBox routeCombo;
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
        void setMidiInputDevices(const juce::Array<SettingsPanel::MidiDeviceInfo>& devices, const juce::StringArray& midiTrackNames);
        void resized() override;

        bool projectSectionVisible = true;
        bool startupSectionVisible = true;
        bool toolsSectionVisible = true;
        bool aiSectionVisible = true;
        bool midiSectionVisible = true;

        juce::Label headerLabel;
        juce::Label subHeaderLabel;
        juce::Label searchLabel;
        juce::TextEditor searchEditor;
        juce::Label storageLabel;
        juce::Label storageValueLabel;
        juce::ToggleButton autoloadToggle { "Autoload last project" };
        CollapsibleHeader projectSectionLabel;
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
        CollapsibleHeader startupSectionLabel;
        CollapsibleHeader toolsSectionLabel;
        CollapsibleHeader aiSectionLabel;
        CollapsibleHeader midiSectionLabel;
        juce::Label midiHintLabel;
        juce::TextButton refreshMidiDevicesButton { "Refresh Devices" };
        juce::OwnedArray<MidiDeviceRow> midiDeviceRows;
        juce::StringArray midiRouteTrackNames;
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
