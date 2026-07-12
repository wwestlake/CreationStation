#pragma once

#include <JuceHeader.h>

class ArrangeView final : public juce::Component
{
public:
    ArrangeView();

    void setTotalTrackCount(int newTrackCount);
    void setVisibleTrackCount(int newVisibleTrackCount);
    int getVisibleTrackCount() const noexcept { return visibleTrackCount; }
    int getTotalTrackCount() const noexcept { return totalTrackCount; }
    void setTrackName(int trackIndex, const juce::String& name);
    void setSelectedTrack(int trackIndex);

    std::function<void(int)> onTrackSelected;
    std::function<void()> onAddTrackRequested;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class Lane final : public juce::Component
    {
    public:
        Lane(int trackIndex, const juce::String& trackName);

        void setTrackIndex(int newTrackIndex);
        void setTrackName(const juce::String& newTrackName);
        void setSelected(bool shouldSelect);

        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        int trackIndex = 0;
        juce::String trackName;
        bool selected = false;
    };

    class Canvas final : public juce::Component
    {
    public:
        void setTrackCount(int newTrackCount, const juce::StringArray& trackNames);
        void setTrackName(int trackIndex, const juce::String& name);
        void setSelectedTrack(int trackIndex);
        int getTrackCount() const noexcept { return trackCount; }

        std::function<void(int)> onTrackSelected;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        juce::OwnedArray<Lane> lanes;
        juce::StringArray trackNames;
        int trackCount = 0;
        int selectedTrack = -1;
    };

    juce::Label titleLabel;
    juce::Label hintLabel;
    juce::TextButton addTrackButton { "Add Channel" };
    juce::Viewport viewport;
    Canvas canvas;
    juce::StringArray trackNames;
    int totalTrackCount = 32;
    int visibleTrackCount = 8;

    void updateCanvasTrackCount();
};
