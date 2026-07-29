#include "SuiteSettingsPanel.h"

namespace
{
juce::Colour panelFill() { return juce::Colour(0xff121822); }
juce::Colour panelOutline() { return juce::Colour(0xff2a3a50); }
juce::Colour labelColour() { return juce::Colour(0xffdce6f5); }
juce::Colour hintColour() { return juce::Colour(0xff97a9c1); }
}

SuiteSettingsPanel::SuiteSettingsPanel()
{
    titleLabel.setText("Creation Suite Control", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subTitleLabel.setText("Manage the suite-wide VFS location, shared resources, and where each app finds its project space.",
                          juce::dontSendNotification);
    subTitleLabel.setColour(juce::Label::textColourId, hintColour());
    addAndMakeVisible(subTitleLabel);

    configureRow(suiteVfsRow, "suite_vfs_root", "Suite VFS Root");
    configureRow(sharedResourcesRow, "shared_resources_root", "Shared Resources");
    configureRow(stationProjectsRow, "creation_station_projects_root", "Creation Station Projects");
    configureRow(engineProjectsRow, "creation_engine_projects_root", "Creation Engine Projects");
    configureRow(movieProjectsRow, "creation_movie_projects_root", "Creation Movie Projects");
    configureRow(liveProjectsRow, "creation_live_projects_root", "Creation Live Projects");

    applyButton.onClick = [this]
    {
        if (onApplyRequested)
            onApplyRequested(getSettings());
    };
    addAndMakeVisible(applyButton);

    eulaButton.onClick = [this]
    {
        if (onReadEulaRequested)
            onReadEulaRequested();
    };
    addAndMakeVisible(eulaButton);

    statusLabel.setColour(juce::Label::textColourId, hintColour());
    statusLabel.setText("Suite settings are stored at the shared suite level for all Creation apps. The EULA lives here too.",
                        juce::dontSendNotification);
    addAndMakeVisible(statusLabel);
}

void SuiteSettingsPanel::setSettings(const SuiteSettings& settings)
{
    suiteVfsRow.editor.setText(settings.suiteVfsRoot, juce::dontSendNotification);
    sharedResourcesRow.editor.setText(settings.sharedResourcesRoot, juce::dontSendNotification);
    stationProjectsRow.editor.setText(settings.creationStationProjectsRoot, juce::dontSendNotification);
    engineProjectsRow.editor.setText(settings.creationEngineProjectsRoot, juce::dontSendNotification);
    movieProjectsRow.editor.setText(settings.creationMovieProjectsRoot, juce::dontSendNotification);
    liveProjectsRow.editor.setText(settings.creationLiveProjectsRoot, juce::dontSendNotification);
}

SuiteSettings SuiteSettingsPanel::getSettings() const
{
    SuiteSettings settings;
    settings.suiteVfsRoot = suiteVfsRow.editor.getText().trim();
    settings.sharedResourcesRoot = sharedResourcesRow.editor.getText().trim();
    settings.creationStationProjectsRoot = stationProjectsRow.editor.getText().trim();
    settings.creationEngineProjectsRoot = engineProjectsRow.editor.getText().trim();
    settings.creationMovieProjectsRoot = movieProjectsRow.editor.getText().trim();
    settings.creationLiveProjectsRoot = liveProjectsRow.editor.getText().trim();
    return settings;
}

void SuiteSettingsPanel::setStatusText(const juce::String& text)
{
    statusLabel.setText(text, juce::dontSendNotification);
}

void SuiteSettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(panelFill());
    g.setColour(panelOutline());
    g.drawRect(getLocalBounds(), 1);
}

void SuiteSettingsPanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(30));
    subTitleLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(12);

    layoutRow(suiteVfsRow, area);
    layoutRow(sharedResourcesRow, area);
    layoutRow(stationProjectsRow, area);
    layoutRow(engineProjectsRow, area);
    layoutRow(movieProjectsRow, area);
    layoutRow(liveProjectsRow, area);

    area.removeFromTop(10);
    auto buttonRow = area.removeFromTop(34);
    applyButton.setBounds(buttonRow.removeFromLeft(180));
    buttonRow.removeFromLeft(10);
    eulaButton.setBounds(buttonRow.removeFromLeft(140));
    area.removeFromTop(8);
    statusLabel.setBounds(area.removeFromTop(40));
}

void SuiteSettingsPanel::configureRow(PathRow& row, const juce::String& id, const juce::String& labelText)
{
    row.id = id;
    row.label.setText(labelText, juce::dontSendNotification);
    row.label.setColour(juce::Label::textColourId, labelColour());
    addAndMakeVisible(row.label);

    addAndMakeVisible(row.editor);
    row.editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a2230));
    row.editor.setColour(juce::TextEditor::outlineColourId, panelOutline());
    row.editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);

    row.browseButton.onClick = [this, id]
    {
        if (onBrowseRequested)
            onBrowseRequested(id);
    };
    addAndMakeVisible(row.browseButton);
}

void SuiteSettingsPanel::layoutRow(PathRow& row, juce::Rectangle<int>& area)
{
    row.label.setBounds(area.removeFromTop(20));
    auto rowArea = area.removeFromTop(30);
    row.browseButton.setBounds(rowArea.removeFromRight(88));
    rowArea.removeFromRight(8);
    row.editor.setBounds(rowArea);
    area.removeFromTop(10);
}
