#pragma once

#include <JuceHeader.h>

class RecordView final : public juce::Component
{
public:
    RecordView();

    void setTrackCount(int newTrackCount);
    void setTrackName(int trackIndex, const juce::String& name);
    void setTrackArmed(int trackIndex, bool shouldArm);
    void setRecordingState(bool shouldRecord, const juce::String& takeName);
    void setRecentTakes(const juce::StringArray& takeNames);

    std::function<void(int, bool)> onTrackArmChanged;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class TrackRow final : public juce::Component
    {
    public:
        TrackRow(int trackIndex, const juce::String& name);

        void setTrackName(const juce::String& name);
        void setArmed(bool shouldArm);
        void paint(juce::Graphics&) override;
        void resized() override;

        std::function<void(int, bool)> onArmChanged;

    private:
        int trackIndex = 0;
        juce::String trackName;
        juce::ToggleButton armButton { "Arm" };
        juce::ToggleButton inputMonitorButton { "Input" };
        bool armed = false;
    };

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label recordStateLabel;
    juce::TextEditor recentTakesEditor;
    juce::OwnedArray<TrackRow> rows;
    juce::StringArray recentTakes;
    int trackCount = 0;
    bool recording = false;
    juce::String currentTakeName;
};
