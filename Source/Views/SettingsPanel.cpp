#include "SettingsPanel.h"
#include <creation/services/SuiteAiProviderRuntime.h>

namespace
{
juce::Colour sectionAccent()
{
    return juce::Colour(0xff5f93ff);
}

const juce::Array<creation::services::SuiteAiProviderRuntimeProfile>& availableAiProviders()
{
    static const auto providers = creation::services::SuiteAiProviderRuntime::createChatCapableProfiles();
    return providers;
}

juce::String normalizedProviderId(const juce::String& providerName)
{
    return creation::services::SuiteAiProviderRuntime::normalizeProviderId(providerName);
}

const creation::services::SuiteAiProviderRuntimeProfile* findProviderPreset(const juce::String& providerName)
{
    const auto providerId = normalizedProviderId(providerName);
    for (const auto& provider : availableAiProviders())
        if (provider.providerId == providerId)
            return std::addressof(provider);
    return nullptr;
}
}

SettingsPanel::ContentView::ContentView(SettingsPanel& ownerRef)
    : owner(ownerRef)
{
    headerLabel.setText("Settings", juce::dontSendNotification);
    headerLabel.setFont(juce::Font(24.0f).boldened());
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    content.addAndMakeVisible(headerLabel);

    subHeaderLabel.setText("Search, scroll, and adjust startup and workspace preferences here. Only file pickers open modally when needed.",
                           juce::dontSendNotification);
    subHeaderLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(subHeaderLabel);

    searchLabel.setText("Finder", juce::dontSendNotification);
    searchLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(searchLabel);

    searchEditor.setTextToShowWhenEmpty("Search settings...", juce::Colour(0xff6e7e94));
    searchEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff141a24));
    searchEditor.setColour(juce::TextEditor::outlineColourId, sectionAccent().withAlpha(0.28f));
    searchEditor.setColour(juce::TextEditor::focusedOutlineColourId, sectionAccent());
    searchEditor.onTextChange = [this]
    {
        owner.searchText = searchEditor.getText();
        applyFilter();
    };
    content.addAndMakeVisible(searchEditor);

    projectSectionLabel.setText("Project");
    projectSectionLabel.onToggled = [this] { layoutContent(); };
    content.addAndMakeVisible(projectSectionLabel);

    auto setupProjectField = [this](juce::Label& label, juce::TextEditor& editor, const juce::String& labelText, const juce::String& emptyText)
    {
        label.setText(labelText, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
        content.addAndMakeVisible(label);

        editor.setTextToShowWhenEmpty(emptyText, juce::Colour(0xff6e7e94));
        editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff141a24));
        editor.setColour(juce::TextEditor::outlineColourId, sectionAccent().withAlpha(0.28f));
        editor.setColour(juce::TextEditor::focusedOutlineColourId, sectionAccent());
        content.addAndMakeVisible(editor);
    };

    setupProjectField(projectNameLabel, projectNameEditor, "Name", "Untitled Project");
    setupProjectField(projectDescriptionLabel, projectDescriptionEditor, "Description", "What is this project?");
    setupProjectField(projectAuthorLabel, projectAuthorEditor, "Author", "Creator / studio name");
    setupProjectField(projectCopyrightLabel, projectCopyrightEditor, "Copyright", "Copyright notice");
    setupProjectField(projectRightsLabel, projectRightsEditor, "Distribution", "Rights / license / usage notes");
    projectDescriptionEditor.setMultiLine(true);
    projectRightsEditor.setMultiLine(true);
    projectMetadataSaveButton.onClick = [this] { applyProjectMetadata(); };
    projectMetadataSaveButton.setTooltip("Save the project name, description, and other metadata");
    content.addAndMakeVisible(projectMetadataSaveButton);

    startupSectionLabel.setText("Startup");
    startupSectionLabel.onToggled = [this] { layoutContent(); };
    content.addAndMakeVisible(startupSectionLabel);

    toolsSectionLabel.setText("Tools");
    toolsSectionLabel.onToggled = [this] { layoutContent(); };
    content.addAndMakeVisible(toolsSectionLabel);

    midiSectionLabel.setText("MIDI");
    midiSectionLabel.onToggled = [this] { layoutContent(); };
    content.addAndMakeVisible(midiSectionLabel);

    midiHintLabel.setText("Devices below feed every enabled track by default (Omni). Route a device to one track "
                          "to keep it from bleeding into others - channel filtering (Omni / Ch 1-16) is still set "
                          "per track in the Tracker's Input dropdown.",
                          juce::dontSendNotification);
    midiHintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    midiHintLabel.setJustificationType(juce::Justification::topLeft);
    content.addAndMakeVisible(midiHintLabel);

    refreshMidiDevicesButton.onClick = [this]
    {
        if (owner.onRefreshMidiDevicesRequested != nullptr)
            owner.onRefreshMidiDevicesRequested();
    };
    refreshMidiDevicesButton.setTooltip("Rescan for newly connected MIDI devices");
    content.addAndMakeVisible(refreshMidiDevicesButton);

    studioInputsLabel.setText("Studio Inputs", juce::dontSendNotification);
    studioInputsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(studioInputsLabel);

    studioInputsValueLabel.setText("No audio input sources discovered yet.", juce::dontSendNotification);
    studioInputsValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    studioInputsValueLabel.setJustificationType(juce::Justification::topLeft);
    content.addAndMakeVisible(studioInputsValueLabel);

    audioSystemLabel.setText("Audio system", juce::dontSendNotification);
    audioSystemLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(audioSystemLabel);

    audioInputLabel.setText("Input device", juce::dontSendNotification);
    audioInputLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(audioInputLabel);

    audioOutputLabel.setText("Output device", juce::dontSendNotification);
    audioOutputLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(audioOutputLabel);

    audioSystemComboBox.onChange = [this]
    {
        if (owner.onAudioSystemChanged != nullptr)
            owner.onAudioSystemChanged(audioSystemComboBox.getText());
    };
    audioInputComboBox.onChange = [this]
    {
        if (owner.onAudioInputDeviceChanged != nullptr)
            owner.onAudioInputDeviceChanged(audioInputComboBox.getText());
    };
    audioOutputComboBox.onChange = [this]
    {
        if (owner.onAudioOutputDeviceChanged != nullptr)
            owner.onAudioOutputDeviceChanged(audioOutputComboBox.getText());
    };
    content.addAndMakeVisible(audioSystemComboBox);
    content.addAndMakeVisible(audioInputComboBox);
    content.addAndMakeVisible(audioOutputComboBox);

    audioDiagnosticsLabel.setText("Audio diagnostics will appear after a device is active.", juce::dontSendNotification);
    audioDiagnosticsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    audioDiagnosticsLabel.setJustificationType(juce::Justification::topLeft);
    content.addAndMakeVisible(audioDiagnosticsLabel);

    audioDriverControlPanelButton.onClick = [this]
    {
        if (owner.onOpenDriverControlPanelRequested != nullptr)
            owner.onOpenDriverControlPanelRequested();
    };
    audioDriverControlPanelButton.setTooltip("Open your audio driver's own control panel");
    content.addAndMakeVisible(audioDriverControlPanelButton);

    aiSectionLabel.setText("AI Provider");
    aiSectionLabel.onToggled = [this] { layoutContent(); };
    content.addAndMakeVisible(aiSectionLabel);

    aiProviderLabel.setText("Provider", juce::dontSendNotification);
    aiProviderLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(aiProviderLabel);

    for (int index = 0; index < availableAiProviders().size(); ++index)
        aiProviderComboBox.addItem(availableAiProviders()[index].displayName, index + 1);
    aiProviderComboBox.setSelectedId(1, juce::dontSendNotification);
    aiProviderComboBox.onChange = [this]
    {
        const auto* provider = findProviderPreset(aiProviderComboBox.getText());
        auto isOllama = provider != nullptr && provider->providerId == "ollama";
        aiKeyLabel.setText("API key / token", juce::dontSendNotification);
        aiModelStatusLabel.setText(isOllama
                                   ? "Ollama usually does not need a key. Refresh models after setting the local server."
                                   : "Refresh the model list after entering your key.",
                                   juce::dontSendNotification);

        auto endpointText = aiEndpointEditor.getText().trim();
        if (provider != nullptr
            && creation::services::SuiteAiProviderRuntime::shouldReplaceBaseUrlOnProviderSwitch(endpointText, *provider))
        {
            aiEndpointEditor.setText(provider->defaultBaseUrl, juce::dontSendNotification);
        }

        applyAiSettings();
    };
    content.addAndMakeVisible(aiProviderComboBox);

    aiHintLabel.setText("This is separate from your LagDaemon login. Enter your own model API key or local provider token here.",
                        juce::dontSendNotification);
    aiHintLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(aiHintLabel);

    aiModelLabel.setText("Model", juce::dontSendNotification);
    aiModelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(aiModelLabel);

    aiModelComboBox.setEditableText(true);
    aiModelComboBox.setTextWhenNothingSelected("gpt-4.1-mini");
    aiModelComboBox.onChange = [this]
    {
        applyAiSettings();
    };
    content.addAndMakeVisible(aiModelComboBox);

    aiEndpointLabel.setText("OpenAI endpoint", juce::dontSendNotification);
    aiEndpointLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(aiEndpointLabel);

    aiEndpointEditor.setTextToShowWhenEmpty("https://api.openai.com/v1", juce::Colour(0xff6e7e94));
    aiEndpointEditor.setText("https://api.openai.com/v1", juce::dontSendNotification);
    aiEndpointEditor.onTextChange = [this] { applyAiSettings(); };
    content.addAndMakeVisible(aiEndpointEditor);

    aiKeyLabel.setText("API key / token", juce::dontSendNotification);
    aiKeyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(aiKeyLabel);

    aiKeyEditor.setTextToShowWhenEmpty("sk-...", juce::Colour(0xff6e7e94));
    aiKeyEditor.setPasswordCharacter('*');
    aiKeyEditor.onTextChange = [this] { applyAiSettings(); };
    content.addAndMakeVisible(aiKeyEditor);

    aiModelStatusLabel.setText("Refresh the model list after entering your key.", juce::dontSendNotification);
    aiModelStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(aiModelStatusLabel);

    aiRefreshModelsButton.onClick = [this]
    {
        if (owner.onRefreshAiModelsRequested != nullptr)
            owner.onRefreshAiModelsRequested();
    };
    aiRefreshModelsButton.setTooltip("Fetch the list of available models from the AI provider");
    content.addAndMakeVisible(aiRefreshModelsButton);

    storageLabel.setText("Storage folder", juce::dontSendNotification);
    storageLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(storageLabel);

    storageValueLabel.setText("Not configured yet", juce::dontSendNotification);
    storageValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    storageValueLabel.setJustificationType(juce::Justification::centredLeft);
    content.addAndMakeVisible(storageValueLabel);

    autoloadToggle.onClick = [this]
    {
        if (owner.onAutoloadChanged != nullptr)
            owner.onAutoloadChanged(autoloadToggle.getToggleState());
    };
    autoloadToggle.setTooltip("Automatically reopen your last project on startup");
    content.addAndMakeVisible(autoloadToggle);

    auto setupRow = [this](ActionRow& row, const juce::String& description)
    {
        row.description.setText(description, juce::dontSendNotification);
        row.description.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
        row.description.setJustificationType(juce::Justification::centredLeft);
        content.addAndMakeVisible(row.description);

        row.actionButton.onClick = [this, &row]
        {
            if (&row == &newProjectRow && owner.onNewProjectRequested != nullptr)
                owner.onNewProjectRequested();
            else if (&row == &openProjectRow && owner.onOpenProjectRequested != nullptr)
                owner.onOpenProjectRequested();
            else if (&row == &saveProjectRow && owner.onSaveProjectRequested != nullptr)
                owner.onSaveProjectRequested();
            else if (&row == &revealProjectRow && owner.onRevealProjectFolderRequested != nullptr)
                owner.onRevealProjectFolderRequested();
            else if (&row == &storageRow && owner.onChangeStorageRequested != nullptr)
                owner.onChangeStorageRequested();
            else if (&row == &studioInputsRow && owner.onRefreshStudioInputsRequested != nullptr)
                owner.onRefreshStudioInputsRequested();
            else if (&row == &audioRow && owner.onOpenAudioRequested != nullptr)
                owner.onOpenAudioRequested();
            else if (&row == &vstRow && owner.onManageVstPathsRequested != nullptr)
                owner.onManageVstPathsRequested();
            else if (&row == &controlSurfaceRow && owner.onManageControlSurfaceMappingsRequested != nullptr)
                owner.onManageControlSurfaceMappingsRequested();
        };
        row.actionButton.setTooltip(description);
        content.addAndMakeVisible(row.actionButton);
    };

    setupRow(newProjectRow, "Create a blank project and start fresh.");
    setupRow(openProjectRow, "Open an existing project folder.");
    setupRow(saveProjectRow, "Save the current project state now.");
    setupRow(revealProjectRow, "Reveal the active project on disk.");
    setupRow(storageRow, "Choose where projects, content, and config live.");
    setupRow(studioInputsRow, "Show the named studio sources tracks can record from.");
    setupRow(audioRow, "Open the audio device setup window.");
    setupRow(vstRow, "Manage plugin folders and rescan the plugin list.");
    setupRow(controlSurfaceRow, "Open the mapping file for devices like X-Touch and BCR2000.");

    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, true);
    addAndMakeVisible(viewport);

    applyFilter();
}

void SettingsPanel::ContentView::setSearchText(const juce::String& text)
{
    searchEditor.setText(text, juce::dontSendNotification);
    applyFilter();
}

void SettingsPanel::ContentView::applyAiSettings()
{
    if (owner.onAiProviderSettingsChanged != nullptr)
        owner.onAiProviderSettingsChanged(owner.getAiProviderSettings());
}

void SettingsPanel::ContentView::applyProjectMetadata()
{
    if (owner.onProjectMetadataChanged == nullptr)
        return;

    ProjectManager::ProjectInfo metadata;
    metadata.name = projectNameEditor.getText().trim();
    metadata.description = projectDescriptionEditor.getText().trim();
    metadata.author = projectAuthorEditor.getText().trim();
    metadata.copyright = projectCopyrightEditor.getText().trim();
    metadata.distributionRights = projectRightsEditor.getText().trim();
    owner.onProjectMetadataChanged(metadata);
}

void SettingsPanel::ContentView::applyFilter()
{
    auto query = searchEditor.getText().trim().toLowerCase();

    auto matches = [&query](const ActionRow& row)
    {
        if (query.isEmpty())
            return true;

        auto haystack = (row.title + " " + row.keywords + " " + row.description.getText()).toLowerCase();
        return haystack.contains(query);
    };

    newProjectRow.visible = matches(newProjectRow);
    openProjectRow.visible = matches(openProjectRow);
    saveProjectRow.visible = matches(saveProjectRow);
    revealProjectRow.visible = matches(revealProjectRow);
    storageRow.visible = matches(storageRow);
    studioInputsRow.visible = matches(studioInputsRow);
    audioRow.visible = matches(audioRow);
    vstRow.visible = matches(vstRow);
    controlSurfaceRow.visible = matches(controlSurfaceRow);

    projectSectionVisible = newProjectRow.visible || openProjectRow.visible || saveProjectRow.visible || revealProjectRow.visible;
    startupSectionVisible = storageRow.visible;
    toolsSectionVisible = studioInputsRow.visible || audioRow.visible || vstRow.visible || controlSurfaceRow.visible;
    aiSectionVisible = query.isEmpty()
                       || aiSectionLabel.getText().toLowerCase().contains(query)
                       || aiProviderLabel.getText().toLowerCase().contains(query)
                       || aiHintLabel.getText().toLowerCase().contains(query)
                       || aiModelLabel.getText().toLowerCase().contains(query)
                       || aiEndpointLabel.getText().toLowerCase().contains(query)
                       || aiKeyLabel.getText().toLowerCase().contains(query)
                       || aiModelComboBox.getText().toLowerCase().contains(query)
                       || aiEndpointEditor.getText().toLowerCase().contains(query);

    midiSectionVisible = query.isEmpty()
                         || midiSectionLabel.getText().toLowerCase().contains(query)
                         || midiHintLabel.getText().toLowerCase().contains(query);
    if (! midiSectionVisible)
        for (auto* row : midiDeviceRows)
            if (row->nameLabel.getText().toLowerCase().contains(query))
                midiSectionVisible = true;

    // A search match forces its section open, so results are never hidden behind a collapsed
    // header - manual collapse choices only matter again once the search box is cleared.
    if (query.isNotEmpty())
    {
        if (projectSectionVisible) projectSectionLabel.setExpanded(true);
        if (startupSectionVisible) startupSectionLabel.setExpanded(true);
        if (toolsSectionVisible) toolsSectionLabel.setExpanded(true);
        if (aiSectionVisible) aiSectionLabel.setExpanded(true);
        if (midiSectionVisible) midiSectionLabel.setExpanded(true);
    }

    projectSectionLabel.setVisible(projectSectionVisible);
    startupSectionLabel.setVisible(startupSectionVisible);
    toolsSectionLabel.setVisible(toolsSectionVisible);
    aiSectionLabel.setVisible(aiSectionVisible);
    midiSectionLabel.setVisible(midiSectionVisible);

    for (auto* row : { &newProjectRow, &openProjectRow, &saveProjectRow, &revealProjectRow, &storageRow, &studioInputsRow, &audioRow, &vstRow, &controlSurfaceRow })
        row->description.setVisible(row->visible);

    layoutContent();
}

void SettingsPanel::ContentView::layoutContent()
{
    auto contentWidth = juce::jmax(1, viewport.getWidth() - viewport.getScrollBarThickness());
    auto x = 20;
    auto width = juce::jmax(320, contentWidth - 40);
    auto y = 20;

    headerLabel.setBounds(x, y, width, 40); y += 40;
    subHeaderLabel.setBounds(x, y, width, 32); y += 38;
    searchLabel.setBounds(x, y, 80, 24);
    searchEditor.setBounds(x + 90, y - 2, juce::jmax(180, width - 90), 28);
    y += 44;

    auto sectionHeight = 28;
    auto rowHeight = 44;

    // Lays out one ActionRow-based section; returns whether its body should be laid out (visible
    // via search AND expanded) so callers can gate any extra hand-written body beneath it.
    auto laySection = [&](CollapsibleHeader& header, bool sectionVisible, const std::initializer_list<ActionRow*> rows)
    {
        header.setVisible(sectionVisible);
        if (! sectionVisible)
        {
            for (auto* row : rows)
            {
                row->description.setVisible(false);
                row->actionButton.setVisible(false);
            }
            return false;
        }

        header.setBounds(x, y, width, sectionHeight);
        y += sectionHeight + 6;

        auto expanded = header.isExpanded();
        for (auto* row : rows)
        {
            auto rowVisible = row->visible && expanded;
            row->description.setVisible(rowVisible);
            row->actionButton.setVisible(rowVisible);
            if (! rowVisible)
                continue;

            row->description.setBounds(x, y, juce::jmax(220, width - 180), rowHeight);
            row->actionButton.setBounds(x + width - 160, y + 4, 160, 32);
            y += rowHeight + 6;
        }

        if (expanded)
            y += 8;

        return expanded;
    };

    auto projectExpanded = laySection(projectSectionLabel, projectSectionVisible,
                                      { &newProjectRow, &openProjectRow, &saveProjectRow, &revealProjectRow });

    projectNameLabel.setVisible(projectExpanded);
    projectNameEditor.setVisible(projectExpanded);
    projectDescriptionLabel.setVisible(projectExpanded);
    projectDescriptionEditor.setVisible(projectExpanded);
    projectAuthorLabel.setVisible(projectExpanded);
    projectAuthorEditor.setVisible(projectExpanded);
    projectCopyrightLabel.setVisible(projectExpanded);
    projectCopyrightEditor.setVisible(projectExpanded);
    projectRightsLabel.setVisible(projectExpanded);
    projectRightsEditor.setVisible(projectExpanded);
    projectMetadataSaveButton.setVisible(projectExpanded);

    if (projectExpanded)
    {
        auto fieldLabelWidth = 120;
        auto fieldHeight = 28;
        auto largeFieldHeight = 58;

        auto layField = [&](juce::Label& label, juce::TextEditor& editor, int height)
        {
            label.setBounds(x, y, fieldLabelWidth, 24);
            editor.setBounds(x + fieldLabelWidth + 10, y - 2, juce::jmax(220, width - fieldLabelWidth - 10), height);
            y += height + 10;
        };

        layField(projectNameLabel, projectNameEditor, fieldHeight);
        layField(projectDescriptionLabel, projectDescriptionEditor, largeFieldHeight);
        layField(projectAuthorLabel, projectAuthorEditor, fieldHeight);
        layField(projectCopyrightLabel, projectCopyrightEditor, fieldHeight);
        layField(projectRightsLabel, projectRightsEditor, largeFieldHeight);
        projectMetadataSaveButton.setBounds(x + width - 180, y, 180, 32);
        y += 44;
    }

    auto startupExpanded = laySection(startupSectionLabel, startupSectionVisible, { &storageRow });
    storageLabel.setVisible(startupExpanded && storageRow.visible);
    storageValueLabel.setVisible(startupExpanded && storageRow.visible);
    autoloadToggle.setVisible(startupExpanded);

    if (startupExpanded)
    {
        autoloadToggle.setBounds(x, y, 240, 28);
        y += 38 + 10;
    }

    auto toolsExpanded = laySection(toolsSectionLabel, toolsSectionVisible,
                                    { &studioInputsRow, &audioRow, &vstRow, &controlSurfaceRow });

    auto studioInputsVisible = toolsExpanded && studioInputsRow.visible;
    audioSystemLabel.setVisible(studioInputsVisible);
    audioSystemComboBox.setVisible(studioInputsVisible);
    audioInputLabel.setVisible(studioInputsVisible);
    audioInputComboBox.setVisible(studioInputsVisible);
    audioOutputLabel.setVisible(studioInputsVisible);
    audioOutputComboBox.setVisible(studioInputsVisible);
    audioDiagnosticsLabel.setVisible(studioInputsVisible);
    studioInputsLabel.setVisible(studioInputsVisible);
    studioInputsValueLabel.setVisible(studioInputsVisible);
    audioDriverControlPanelButton.setVisible(studioInputsVisible && audioDriverControlPanelButton.isEnabled());
    for (auto* label : studioInputHardwareLabels)
        label->setVisible(studioInputsVisible);
    for (auto* editor : studioInputNameEditors)
        editor->setVisible(studioInputsVisible);

    if (studioInputsVisible)
    {
        audioSystemLabel.setBounds(x, y, 140, 24);
        audioSystemComboBox.setBounds(x + 150, y - 2, juce::jmax(240, width - 150), 28);
        y += 36;

        audioInputLabel.setBounds(x, y, 140, 24);
        audioInputComboBox.setBounds(x + 150, y - 2, juce::jmax(240, width - 150), 28);
        y += 36;

        audioOutputLabel.setBounds(x, y, 140, 24);
        audioOutputComboBox.setBounds(x + 150, y - 2, juce::jmax(240, width - 150), 28);
        y += 40;

        audioDiagnosticsLabel.setBounds(x + 150, y, juce::jmax(240, width - 150), 92);
        if (audioDriverControlPanelButton.isVisible())
        {
            audioDriverControlPanelButton.setBounds(x, y, 140, 30);
            y += 100;
        }
        else
        {
            y += 96;
        }

        studioInputsLabel.setBounds(x, y, 140, 24);
        if (studioInputNameEditors.isEmpty())
        {
            studioInputsValueLabel.setBounds(x + 150, y, juce::jmax(240, width - 150), 44);
            y += 54;
        }
        else
        {
            studioInputsValueLabel.setBounds(x + 150, y, juce::jmax(240, width - 150), 22);
            y += 30;

            for (int index = 0; index < studioInputNameEditors.size(); ++index)
            {
                auto* editor = studioInputNameEditors[index];
                auto* label = studioInputHardwareLabels[index];
                if (editor == nullptr || label == nullptr)
                    continue;

                auto rowWidth = juce::jmax(240, width - 150);
                editor->setBounds(x + 150, y, rowWidth, 28);
                y += 30;
                label->setBounds(x + 150, y, juce::jmax(180, width - 150), 20);
                y += 26;
            }
        }
    }

    midiSectionLabel.setVisible(midiSectionVisible);
    auto midiExpanded = midiSectionVisible && midiSectionLabel.isExpanded();

    if (midiSectionVisible)
    {
        midiSectionLabel.setBounds(x, y, width, sectionHeight);
        y += sectionHeight + 6;
    }

    midiHintLabel.setVisible(midiExpanded);
    refreshMidiDevicesButton.setVisible(midiExpanded);
    for (auto* row : midiDeviceRows)
    {
        row->nameLabel.setVisible(midiExpanded);
        row->enabledToggle.setVisible(midiExpanded);
        row->routeCombo.setVisible(midiExpanded);
    }

    if (midiExpanded)
    {
        midiHintLabel.setBounds(x, y, width, 40);
        y += 46;

        refreshMidiDevicesButton.setBounds(x, y, 160, 30);
        y += 40;

        if (midiDeviceRows.isEmpty())
        {
            y += 4;
        }
        else
        {
            for (auto* row : midiDeviceRows)
            {
                row->nameLabel.setBounds(x, y, juce::jmax(160, width - 340), 28);
                row->enabledToggle.setBounds(x + width - 320, y, 100, 28);
                row->routeCombo.setBounds(x + width - 210, y, 210, 28);
                y += 34;
            }
        }

        y += 8;
    }

    aiSectionLabel.setVisible(aiSectionVisible);
    auto aiExpanded = aiSectionVisible && aiSectionLabel.isExpanded();

    aiProviderLabel.setVisible(aiExpanded);
    aiProviderComboBox.setVisible(aiExpanded);
    aiHintLabel.setVisible(aiExpanded);
    aiModelLabel.setVisible(aiExpanded);
    aiModelComboBox.setVisible(aiExpanded);
    aiEndpointLabel.setVisible(aiExpanded);
    aiEndpointEditor.setVisible(aiExpanded);
    aiKeyLabel.setVisible(aiExpanded);
    aiKeyEditor.setVisible(aiExpanded);
    aiModelStatusLabel.setVisible(aiExpanded);
    aiRefreshModelsButton.setVisible(aiExpanded);

    if (aiSectionVisible)
    {
        aiSectionLabel.setBounds(x, y, width, sectionHeight);
        y += sectionHeight + 6;
    }

    if (aiExpanded)
    {
        aiProviderLabel.setBounds(x, y, 100, 24);
        aiProviderComboBox.setBounds(x + 110, y, juce::jmax(180, width - 110), 24);
        y += 28;

        aiHintLabel.setBounds(x, y, width, 28);
        y += 32;

        aiModelLabel.setBounds(x, y, 100, 24);
        aiModelComboBox.setBounds(x + 110, y - 2, juce::jmax(240, width - 110), 28);
        y += 36;

        aiEndpointLabel.setBounds(x, y, 140, 24);
        aiEndpointEditor.setBounds(x + 150, y - 2, juce::jmax(240, width - 150), 28);
        y += 36;

        aiKeyLabel.setBounds(x, y, 140, 24);
        aiKeyEditor.setBounds(x + 150, y - 2, juce::jmax(240, width - 150), 28);
        y += 40;

        aiModelStatusLabel.setBounds(x, y, width, 22);
        y += 26;

        aiRefreshModelsButton.setBounds(x, y, 160, 32);
        y += 44;
    }

    y += 24;
    content.setSize(contentWidth, juce::jmax(y, viewport.getHeight()));
}

void SettingsPanel::ContentView::resized()
{
    viewport.setBounds(getLocalBounds());
    layoutContent();
}

void SettingsPanel::ContentView::setComboItems(juce::ComboBox& comboBox,
                                               const juce::StringArray& items,
                                               const juce::String& selectedText)
{
    comboBox.clear(juce::dontSendNotification);

    for (int index = 0; index < items.size(); ++index)
        comboBox.addItem(items[index], index + 1);

    if (selectedText.isNotEmpty())
        comboBox.setText(selectedText, juce::dontSendNotification);
    else if (! items.isEmpty())
        comboBox.setSelectedId(1, juce::dontSendNotification);
}

void SettingsPanel::ContentView::setStudioInputRows(const juce::StringArray& names,
                                                    const juce::StringArray& hardwareNames,
                                                    const juce::Array<bool>& availability)
{
    if (names.isEmpty())
    {
        studioInputHardwareLabels.clear();
        studioInputNameEditors.clear();
        studioInputsValueLabel.setText("No input device is active. Open Audio and choose an input device.",
                                       juce::dontSendNotification);
        resized();
        return;
    }

    studioInputsValueLabel.setText("Detected " + juce::String(names.size())
                                   + " mono source" + (names.size() == 1 ? "" : "s")
                                   + ". Name these once; tracks use these names from then on.",
                                   juce::dontSendNotification);

    while (studioInputNameEditors.size() > names.size())
    {
        studioInputNameEditors.removeLast();
        studioInputHardwareLabels.removeLast();
    }

    for (int index = 0; index < names.size(); ++index)
    {
        if (studioInputNameEditors.size() <= index)
        {
            auto* editor = new juce::TextEditor();
            editor->setTextToShowWhenEmpty("Studio input name", juce::Colour(0xff6e7e94));
            editor->setSelectAllWhenFocused(true);
            editor->setInputRestrictions(48);
            editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff141a24));
            editor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
            editor->setColour(juce::TextEditor::outlineColourId, sectionAccent().withAlpha(0.28f));
            editor->setColour(juce::TextEditor::focusedOutlineColourId, sectionAccent());
            editor->setColour(juce::TextEditor::highlightColourId, sectionAccent().withAlpha(0.35f));
            content.addAndMakeVisible(editor);
            studioInputNameEditors.add(editor);

            auto* label = new juce::Label();
            content.addAndMakeVisible(label);
            studioInputHardwareLabels.add(label);
        }

        auto* editor = studioInputNameEditors[index];
        auto* label = studioInputHardwareLabels[index];
        if (editor == nullptr || label == nullptr)
            continue;

        const auto editorIndex = index;
        editor->onTextChange = [this, editorIndex, editor]
        {
            if (owner.onStudioInputNameChanged != nullptr)
                owner.onStudioInputNameChanged(editorIndex, editor->getText());
        };
        editor->onReturnKey = [this, editorIndex, editor]
        {
            if (owner.onStudioInputNameChanged != nullptr)
                owner.onStudioInputNameChanged(editorIndex, editor->getText());
        };
        editor->onFocusLost = [this, editorIndex, editor]
        {
            if (owner.onStudioInputNameChanged != nullptr)
                owner.onStudioInputNameChanged(editorIndex, editor->getText());
        };

        if (! editor->hasKeyboardFocus(false))
            editor->setText(names[index], juce::dontSendNotification);

        auto hardwareName = index < hardwareNames.size() ? hardwareNames[index] : juce::String();
        auto available = index < availability.size() ? availability[index] : false;
        label->setText((available ? "Hardware: " : "Missing: ")
                       + (hardwareName.isNotEmpty() ? hardwareName : "No hardware name reported"),
                       juce::dontSendNotification);
        label->setColour(juce::Label::textColourId, available ? juce::Colour(0xffaebbd0)
                                                              : juce::Colour(0xffffb0a8));
    }

    resized();
}

void SettingsPanel::ContentView::setMidiInputDevices(const juce::Array<SettingsPanel::MidiDeviceInfo>& devices,
                                                      const juce::StringArray& midiTrackNames)
{
    midiRouteTrackNames = midiTrackNames;

    while (midiDeviceRows.size() > devices.size())
        midiDeviceRows.removeLast();

    while (midiDeviceRows.size() < devices.size())
    {
        auto* row = new MidiDeviceRow();
        row->nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        content.addAndMakeVisible(row->nameLabel);

        row->enabledToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffaebbd0));
        row->enabledToggle.setTooltip("Enable or disable this MIDI input device");
        row->routeCombo.setTooltip("Which track this device's MIDI feeds - All Tracks, or one specific track");
        content.addAndMakeVisible(row->enabledToggle);

        row->routeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff141a24));
        row->routeCombo.setColour(juce::ComboBox::outlineColourId, sectionAccent().withAlpha(0.28f));
        content.addAndMakeVisible(row->routeCombo);

        midiDeviceRows.add(row);
    }

    for (int index = 0; index < devices.size(); ++index)
    {
        auto* row = midiDeviceRows[index];
        const auto& info = devices.getReference(index);
        row->deviceId = info.id;
        row->nameLabel.setText(info.name, juce::dontSendNotification);

        row->enabledToggle.onClick = nullptr;
        row->enabledToggle.setToggleState(info.enabled, juce::dontSendNotification);
        auto deviceId = info.id;
        row->enabledToggle.onClick = [this, &toggle = row->enabledToggle, deviceId]
        {
            if (owner.onMidiInputDeviceEnabledChanged != nullptr)
                owner.onMidiInputDeviceEnabledChanged(deviceId, toggle.getToggleState());
        };

        row->routeCombo.onChange = nullptr;
        row->routeCombo.clear(juce::dontSendNotification);
        row->routeCombo.addItem("All Tracks", 1);
        for (int trackIndex = 0; trackIndex < midiTrackNames.size(); ++trackIndex)
            row->routeCombo.addItem(midiTrackNames[trackIndex], trackIndex + 2);
        row->routeCombo.setSelectedId(info.routedTrackIndex < 0 ? 1 : (info.routedTrackIndex + 2), juce::dontSendNotification);
        row->routeCombo.onChange = [this, &combo = row->routeCombo, deviceId]
        {
            if (owner.onMidiInputDeviceRouteChanged != nullptr)
                owner.onMidiInputDeviceRouteChanged(deviceId, combo.getSelectedId() - 2);
        };
    }

    resized();
}

SettingsPanel::SettingsPanel()
    : contentView(*this)
{
    addAndMakeVisible(contentView);
}

void SettingsPanel::setProjectMetadata(const ProjectManager::ProjectInfo& metadata)
{
    contentView.projectNameEditor.setText(metadata.name, juce::dontSendNotification);
    contentView.projectDescriptionEditor.setText(metadata.description, juce::dontSendNotification);
    contentView.projectAuthorEditor.setText(metadata.author, juce::dontSendNotification);
    contentView.projectCopyrightEditor.setText(metadata.copyright, juce::dontSendNotification);
    contentView.projectRightsEditor.setText(metadata.distributionRights, juce::dontSendNotification);
}

void SettingsPanel::setStoragePath(const juce::String& path)
{
    contentView.storageValueLabel.setText(path.isNotEmpty() ? path : "Not configured yet", juce::dontSendNotification);
}

void SettingsPanel::setAutoloadEnabled(bool enabled)
{
    contentView.autoloadToggle.setToggleState(enabled, juce::dontSendNotification);
}

void SettingsPanel::setAiProviderSettings(const AiProviderSettings& settings)
{
    const auto providerId = normalizedProviderId(settings.providerId.isNotEmpty()
                                                     ? settings.providerId
                                                     : settings.providerDisplayName);
    auto selectedIndex = 0;
    for (int index = 0; index < availableAiProviders().size(); ++index)
        if (availableAiProviders()[index].providerId == providerId)
            selectedIndex = index;
    const auto* provider = findProviderPreset(providerId);
    auto isOllama = provider != nullptr && provider->providerId == "ollama";
    contentView.aiProviderComboBox.setSelectedId(selectedIndex + 1, juce::dontSendNotification);
    contentView.aiKeyLabel.setText("API key / token", juce::dontSendNotification);
    contentView.aiModelStatusLabel.setText(isOllama
                                           ? "Ollama usually does not need a key. Refresh models after setting the local server."
                                           : "Refresh the model list after entering your key.",
                                           juce::dontSendNotification);
    contentView.aiModelComboBox.setText(settings.modelName.isNotEmpty()
                                            ? settings.modelName
                                            : creation::services::SuiteAiProviderRuntime::defaultModelName(*provider),
                                        juce::dontSendNotification);
    auto endpointDefault = provider != nullptr ? provider->defaultBaseUrl
                                               : (isOllama ? "http://localhost:11434" : "https://api.openai.com/v1");
    contentView.aiEndpointEditor.setText(settings.baseUrl.isNotEmpty() ? settings.baseUrl : endpointDefault,
                                         juce::dontSendNotification);
    contentView.aiKeyEditor.setText(settings.apiKey, juce::dontSendNotification);
}

void SettingsPanel::setAvailableAiModels(const juce::StringArray& modelIds, const juce::String& statusText)
{
    contentView.aiModelComboBox.clear(juce::dontSendNotification);
    for (int index = 0; index < modelIds.size(); ++index)
        contentView.aiModelComboBox.addItem(modelIds[index], index + 1);

    auto current = contentView.aiModelComboBox.getText().trim();
    if (current.isNotEmpty())
        contentView.aiModelComboBox.setText(current, juce::dontSendNotification);
    else if (modelIds.size() > 0)
        contentView.aiModelComboBox.setText(modelIds[0], juce::dontSendNotification);

    contentView.aiModelStatusLabel.setText(statusText, juce::dontSendNotification);
}

void SettingsPanel::setAudioDeviceLists(const juce::StringArray& audioSystems,
                                        const juce::StringArray& inputDevices,
                                        const juce::StringArray& outputDevices,
                                        const juce::String& selectedSystem,
                                        const juce::String& selectedInput,
                                        const juce::String& selectedOutput)
{
    contentView.setComboItems(contentView.audioSystemComboBox, audioSystems, selectedSystem);
    contentView.setComboItems(contentView.audioInputComboBox, inputDevices, selectedInput);
    contentView.setComboItems(contentView.audioOutputComboBox, outputDevices, selectedOutput);
}

void SettingsPanel::setAudioDiagnostics(const juce::String& diagnosticsText, bool canOpenDriverControlPanel)
{
    contentView.audioDiagnosticsLabel.setText(diagnosticsText.isNotEmpty()
                                                  ? diagnosticsText
                                                  : "No active audio device.",
                                              juce::dontSendNotification);
    contentView.audioDriverControlPanelButton.setEnabled(canOpenDriverControlPanel);
    contentView.audioDriverControlPanelButton.setVisible(canOpenDriverControlPanel);
    contentView.resized();
}

void SettingsPanel::setStudioInputSummary(const juce::StringArray& inputSummaries)
{
    if (inputSummaries.isEmpty())
    {
        contentView.studioInputsValueLabel.setText("No input device is active. Open Audio and choose an input device.",
                                                   juce::dontSendNotification);
        return;
    }

    juce::String text;
    for (int index = 0; index < inputSummaries.size(); ++index)
    {
        if (index > 0)
            text << "\n";

        text << inputSummaries[index];
    }

    contentView.studioInputsValueLabel.setText(text, juce::dontSendNotification);
}

void SettingsPanel::setStudioInputRows(const juce::StringArray& names,
                                       const juce::StringArray& hardwareNames,
                                       const juce::Array<bool>& availability)
{
    contentView.setStudioInputRows(names, hardwareNames, availability);
}

void SettingsPanel::setMidiInputDevices(const juce::Array<MidiDeviceInfo>& devices, const juce::StringArray& midiTrackNames)
{
    contentView.setMidiInputDevices(devices, midiTrackNames);
}

AiProviderSettings SettingsPanel::getAiProviderSettings() const
{
    AiProviderSettings settings;
    settings.providerDisplayName = contentView.aiProviderComboBox.getText().trim();
    settings.providerId = normalizedProviderId(settings.providerDisplayName);
    settings.modelName = contentView.aiModelComboBox.getText().trim();
    settings.baseUrl = contentView.aiEndpointEditor.getText().trim();
    settings.apiKey = contentView.aiKeyEditor.getText();
    return settings;
}

void SettingsPanel::setSearchText(const juce::String& text)
{
    contentView.setSearchText(text);
}

juce::String SettingsPanel::getSearchText() const
{
    return contentView.searchEditor.getText();
}

void SettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff141820));
}

void SettingsPanel::resized()
{
    contentView.setBounds(getLocalBounds());
}
