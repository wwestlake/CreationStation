#include "RecordView.h"

namespace
{
juce::Colour panelColour() { return juce::Colour(0xff11151c); }
juce::Colour rowColour() { return juce::Colour(0xff1b2230); }
}

RecordView::TrackRow::TrackRow(int newTrackIndex, const juce::String& name)
    : trackIndex(newTrackIndex), trackName(name)
{
    armButton.onClick = [this]
    {
        armed = armButton.getToggleState();
        if (onArmChanged)
            onArmChanged(trackIndex, armed);
    };
    addAndMakeVisible(armButton);
    addAndMakeVisible(inputMonitorButton);
}

void RecordView::TrackRow::setTrackName(const juce::String& name)
{
    trackName = name;
    repaint();
}

void RecordView::TrackRow::setArmed(bool shouldArm)
{
    armed = shouldArm;
    armButton.setToggleState(armed, juce::dontSendNotification);
}

void RecordView::TrackRow::paint(juce::Graphics& g)
{
    g.setColour(rowColour());
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 10.0f);
    g.setColour(juce::Colour(0xff2d384a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 10.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f).boldened());
    g.drawText(juce::String(trackIndex + 1) + ". " + trackName, 16, 0, 220, getHeight(), juce::Justification::centredLeft);
}

void RecordView::TrackRow::resized()
{
    auto area = getLocalBounds().reduced(12);
    inputMonitorButton.setBounds(area.removeFromRight(82));
    armButton.setBounds(area.removeFromRight(82));
}

RecordView::RecordView()
{
    titleLabel.setText("Record View", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Arm takes, monitor inputs, and prep sessions.", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(subtitleLabel);

    recordStateLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd7deea));
    recordStateLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(recordStateLabel);

    recentTakesEditor.setMultiLine(true);
    recentTakesEditor.setReadOnly(true);
    recentTakesEditor.setScrollbarsShown(true);
    recentTakesEditor.setCaretVisible(false);
    recentTakesEditor.setPopupMenuEnabled(false);
    recentTakesEditor.setText("Recent takes will appear here.");
    recentTakesEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff171d27));
    recentTakesEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2d384a));
    recentTakesEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffd7deea));
    addAndMakeVisible(recentTakesEditor);
}

void RecordView::setTrackCount(int newTrackCount)
{
    trackCount = juce::jmax(0, newTrackCount);

    while (rows.size() < trackCount)
    {
        auto* row = new TrackRow(rows.size(), "Track " + juce::String(rows.size() + 1));
        row->onArmChanged = [this](int rowIndex, bool shouldArm)
        {
            if (onTrackArmChanged)
                onTrackArmChanged(rowIndex, shouldArm);
        };
        rows.add(row);
    }

    while (rows.size() > trackCount)
        rows.removeLast();

    for (auto* row : rows)
        addAndMakeVisible(row);

    resized();
}

void RecordView::setTrackName(int trackIndex, const juce::String& name)
{
    if (juce::isPositiveAndBelow(trackIndex, rows.size()))
        rows[trackIndex]->setTrackName(name);
}

void RecordView::setTrackArmed(int trackIndex, bool shouldArm)
{
    if (juce::isPositiveAndBelow(trackIndex, rows.size()))
        rows[trackIndex]->setArmed(shouldArm);
}

void RecordView::setRecordingState(bool shouldRecord, const juce::String& takeName)
{
    recording = shouldRecord;
    currentTakeName = takeName;

    if (recording)
        recordStateLabel.setText("Recording: " + currentTakeName, juce::dontSendNotification);
    else
        recordStateLabel.setText("Recording stopped.", juce::dontSendNotification);
}

void RecordView::setRecentTakes(const juce::StringArray& takeNames)
{
    recentTakes = takeNames;
    juce::String text;
    if (recentTakes.isEmpty())
    {
        text = "No recorded clips yet.";
    }
    else
    {
        for (const auto& take : recentTakes)
            text << take << "\n";
    }

    recentTakesEditor.setText(text, juce::dontSendNotification);
}

void RecordView::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());
}

void RecordView::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(32));
    subtitleLabel.setBounds(area.removeFromTop(22));
    recordStateLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(12);

    auto listArea = area.removeFromBottom(110);
    recentTakesEditor.setBounds(listArea);
    area.removeFromBottom(12);

    auto rowHeight = 44;
    for (auto* row : rows)
    {
        row->setBounds(area.removeFromTop(rowHeight));
        area.removeFromTop(8);
    }
}
