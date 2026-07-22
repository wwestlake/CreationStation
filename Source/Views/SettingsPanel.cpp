#include "SettingsPanel.h"

namespace
{
juce::Colour sectionAccent()
{
    return juce::Colour(0xff5f93ff);
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

    projectSectionLabel.setText("Project", juce::dontSendNotification);
    projectSectionLabel.setFont(juce::Font(18.0f).boldened());
    projectSectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
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
    content.addAndMakeVisible(projectMetadataSaveButton);

    startupSectionLabel.setText("Startup", juce::dontSendNotification);
    startupSectionLabel.setFont(juce::Font(18.0f).boldened());
    startupSectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    content.addAndMakeVisible(startupSectionLabel);

    toolsSectionLabel.setText("Tools", juce::dontSendNotification);
    toolsSectionLabel.setFont(juce::Font(18.0f).boldened());
    toolsSectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    content.addAndMakeVisible(toolsSectionLabel);

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
    content.addAndMakeVisible(audioDriverControlPanelButton);

    aiSectionLabel.setText("AI Provider", juce::dontSendNotification);
    aiSectionLabel.setFont(juce::Font(18.0f).boldened());
    aiSectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    content.addAndMakeVisible(aiSectionLabel);

    aiProviderLabel.setText("Provider", juce::dontSendNotification);
    aiProviderLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
    content.addAndMakeVisible(aiProviderLabel);

    aiProviderComboBox.addItem("OpenAI", 1);
    aiProviderComboBox.addItem("Ollama", 2);
    aiProviderComboBox.setSelectedId(1, juce::dontSendNotification);
    aiProviderComboBox.onChange = [this]
    {
        auto isOllama = aiProviderComboBox.getText().toLowerCase().contains("ollama");
        aiKeyLabel.setText("API key / token", juce::dontSendNotification);
        aiModelStatusLabel.setText(isOllama
                                   ? "Ollama usually does not need a key. Refresh models after setting the local server."
                                   : "Refresh the model list after entering your key.",
                                   juce::dontSendNotification);

        auto endpointText = aiEndpointEditor.getText().trim();
        if (isOllama)
        {
            if (endpointText.isEmpty() || endpointText.containsIgnoreCase("api.openai.com"))
                aiEndpointEditor.setText("http://localhost:11434", juce::dontSendNotification);
        }
        else if (endpointText.isEmpty() || endpointText.containsIgnoreCase("11434"))
        {
            aiEndpointEditor.setText("https://api.openai.com/v1", juce::dontSendNotification);
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

    auto projectVisible = newProjectRow.visible || openProjectRow.visible || saveProjectRow.visible || revealProjectRow.visible;
    auto startupVisible = storageRow.visible || autoloadToggle.isVisible();
    auto toolsVisible = studioInputsRow.visible || audioRow.visible || vstRow.visible || controlSurfaceRow.visible;
    auto aiVisible = query.isEmpty()
                     || aiSectionLabel.getText().toLowerCase().contains(query)
                     || aiProviderLabel.getText().toLowerCase().contains(query)
                     || aiHintLabel.getText().toLowerCase().contains(query)
                     || aiModelLabel.getText().toLowerCase().contains(query)
                     || aiEndpointLabel.getText().toLowerCase().contains(query)
                     || aiKeyLabel.getText().toLowerCase().contains(query)
                     || aiModelComboBox.getText().toLowerCase().contains(query)
                     || aiEndpointEditor.getText().toLowerCase().contains(query);

    projectSectionLabel.setVisible(projectVisible);
    projectNameLabel.setVisible(projectVisible);
    projectNameEditor.setVisible(projectVisible);
    projectDescriptionLabel.setVisible(projectVisible);
    projectDescriptionEditor.setVisible(projectVisible);
    projectAuthorLabel.setVisible(projectVisible);
    projectAuthorEditor.setVisible(projectVisible);
    projectCopyrightLabel.setVisible(projectVisible);
    projectCopyrightEditor.setVisible(projectVisible);
    projectRightsLabel.setVisible(projectVisible);
    projectRightsEditor.setVisible(projectVisible);
    projectMetadataSaveButton.setVisible(projectVisible);
    startupSectionLabel.setVisible(startupVisible);
    toolsSectionLabel.setVisible(toolsVisible);
    aiSectionLabel.setVisible(aiVisible);

    storageLabel.setVisible(storageRow.visible);
    storageValueLabel.setVisible(storageRow.visible);
    autoloadToggle.setVisible(startupVisible);
    studioInputsLabel.setVisible(studioInputsRow.visible);
    studioInputsValueLabel.setVisible(studioInputsRow.visible);
    for (auto* label : studioInputHardwareLabels)
        label->setVisible(studioInputsRow.visible);
    for (auto* editor : studioInputNameEditors)
        editor->setVisible(studioInputsRow.visible);
    audioSystemLabel.setVisible(studioInputsRow.visible);
    audioSystemComboBox.setVisible(studioInputsRow.visible);
    audioInputLabel.setVisible(studioInputsRow.visible);
    audioInputComboBox.setVisible(studioInputsRow.visible);
    audioOutputLabel.setVisible(studioInputsRow.visible);
    audioOutputComboBox.setVisible(studioInputsRow.visible);
    audioDiagnosticsLabel.setVisible(studioInputsRow.visible);
    audioDriverControlPanelButton.setVisible(studioInputsRow.visible && audioDriverControlPanelButton.isEnabled());
    aiProviderLabel.setVisible(aiVisible);
    aiProviderComboBox.setVisible(aiVisible);
    aiHintLabel.setVisible(aiVisible);
    aiModelLabel.setVisible(aiVisible);
    aiModelComboBox.setVisible(aiVisible);
    aiEndpointLabel.setVisible(aiVisible);
    aiEndpointEditor.setVisible(aiVisible);
    aiKeyLabel.setVisible(aiVisible);
    aiKeyEditor.setVisible(aiVisible);
    aiModelStatusLabel.setVisible(aiVisible);
    aiRefreshModelsButton.setVisible(aiVisible);

    for (auto* row : { &newProjectRow, &openProjectRow, &saveProjectRow, &revealProjectRow, &storageRow, &studioInputsRow, &audioRow, &vstRow, &controlSurfaceRow })
    {
        row->description.setVisible(row->visible);
        row->actionButton.setVisible(row->visible);
    }

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

    auto laySection = [&](juce::Label& label, const std::initializer_list<ActionRow*> rows)
    {
        bool anyVisible = false;
        for (auto* row : rows)
            anyVisible |= row->visible;

        label.setVisible(anyVisible);
        if (! anyVisible)
            return;

        label.setBounds(x, y, width, sectionHeight);
        y += sectionHeight + 6;

        for (auto* row : rows)
        {
            row->description.setVisible(row->visible);
            row->actionButton.setVisible(row->visible);
            if (! row->visible)
                continue;

            row->description.setBounds(x, y, juce::jmax(220, width - 180), rowHeight);
            row->actionButton.setBounds(x + width - 160, y + 4, 160, 32);
            y += rowHeight + 6;
        }

        y += 8;
    };

    laySection(projectSectionLabel, { &newProjectRow, &openProjectRow, &saveProjectRow, &revealProjectRow });

    if (projectSectionLabel.isVisible())
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

    laySection(startupSectionLabel, { &storageRow });

    if (autoloadToggle.isVisible())
    {
        autoloadToggle.setBounds(x, y, 240, 28);
        y += 38;
        y += 10;
    }

    laySection(toolsSectionLabel, { &studioInputsRow, &audioRow, &vstRow, &controlSurfaceRow });

    if (studioInputsRow.visible)
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

    if (aiSectionLabel.isVisible())
    {
        aiSectionLabel.setBounds(x, y, width, sectionHeight);
        y += sectionHeight + 6;

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
    auto isOllama = settings.providerName.toLowerCase().contains("ollama");
    contentView.aiProviderComboBox.setSelectedId(isOllama ? 2 : 1, juce::dontSendNotification);
    contentView.aiKeyLabel.setText("API key / token", juce::dontSendNotification);
    contentView.aiModelStatusLabel.setText(isOllama
                                           ? "Ollama usually does not need a key. Refresh models after setting the local server."
                                           : "Refresh the model list after entering your key.",
                                           juce::dontSendNotification);
    contentView.aiModelComboBox.setText(settings.modelName.isNotEmpty() ? settings.modelName : "gpt-4.1-mini",
                                        juce::dontSendNotification);
    auto endpointDefault = isOllama ? "http://localhost:11434" : "https://api.openai.com/v1";
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

AiProviderSettings SettingsPanel::getAiProviderSettings() const
{
    AiProviderSettings settings;
    settings.providerName = contentView.aiProviderComboBox.getText().trim();
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
