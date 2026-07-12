#pragma once

#include <JuceHeader.h>

class RecordView final : public juce::Component
{
public:
    RecordView();

    void setTrackCount(int newTrackCount);
    void setTrackName(int trackIndex, const juce::String& name);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class TrackRow final : public juce::Component
    {
    public:
        TrackRow(int trackIndex, const juce::String& name);

        void setTrackName(const juce::String& name);
        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        int trackIndex = 0;
        juce::String trackName;
        juce::ToggleButton armButton { "Arm" };
        juce::ToggleButton inputMonitorButton { "Input" };
    };

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::OwnedArray<TrackRow> rows;
    int trackCount = 0;
};
