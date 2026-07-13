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
    void setRecordedClips(const juce::StringArray& clipNames);
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

        class ClipBlock final : public juce::Component
        {
        public:
            void setClipName(const juce::String& name);
            void setLaneIndex(int newLaneIndex);
            void setStartBeat(int newStartBeat);
            void setLengthBeats(int newLengthBeats);
            void setColour(juce::Colour newColour);
            int getStartBeat() const noexcept { return startBeat; }
            int getLengthBeats() const noexcept { return lengthBeats; }
            void updateBoundsFromState();
            void paint(juce::Graphics&) override;
            void mouseDown(const juce::MouseEvent& event) override;
            void mouseDrag(const juce::MouseEvent& event) override;
            void mouseUp(const juce::MouseEvent& event) override;

        private:
            juce::String clipName;
            int laneIndex = 0;
            int startBeat = 0;
            int lengthBeats = 1;
            juce::Colour colour;
            juce::Point<int> dragStart;
            int startBeatOnDrag = 0;
        };

        void setTrackIndex(int newTrackIndex);
        void setTrackName(const juce::String& newTrackName);
        void setClipNames(const juce::StringArray& newClipNames);
        void setSelected(bool shouldSelect);
        void layoutClipBlocks();

        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        int trackIndex = 0;
        juce::String trackName;
        juce::StringArray clipNames;
        juce::OwnedArray<ClipBlock> clipBlocks;
        bool selected = false;

        void rebuildClipBlocks();
    };

    class Canvas final : public juce::Component
    {
    public:
        void setTrackCount(int newTrackCount, const juce::StringArray& trackNames);
        void setTrackName(int trackIndex, const juce::String& name);
        void setRecordedClips(const juce::StringArray& clipNames);
        void setSelectedTrack(int trackIndex);
        int getTrackCount() const noexcept { return trackCount; }

        std::function<void(int)> onTrackSelected;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        juce::OwnedArray<Lane> lanes;
        juce::StringArray trackNames;
        juce::StringArray recordedClips;
        int trackCount = 0;
        int selectedTrack = -1;

        void rebuildLaneClips();
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
