#include "RecordView.h"

namespace
{
juce::Colour panelColour() { return juce::Colour(0xff11151c); }
juce::Colour rowColour() { return juce::Colour(0xff1b2230); }
}

RecordView::TrackRow::TrackRow(int newTrackIndex, const juce::String& name)
    : trackIndex(newTrackIndex), trackName(name)
{
    addAndMakeVisible(armButton);
    addAndMakeVisible(inputMonitorButton);
}

void RecordView::TrackRow::setTrackName(const juce::String& name)
{
    trackName = name;
    repaint();
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
}

void RecordView::setTrackCount(int newTrackCount)
{
    trackCount = juce::jmax(0, newTrackCount);

    while (rows.size() < trackCount)
        rows.add(new TrackRow(rows.size(), "Track " + juce::String(rows.size() + 1)));

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

void RecordView::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());
}

void RecordView::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(32));
    subtitleLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(12);

    auto rowHeight = 44;
    for (auto* row : rows)
    {
        row->setBounds(area.removeFromTop(rowHeight));
        area.removeFromTop(8);
    }
}
