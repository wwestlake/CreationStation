#pragma once

#include <JuceHeader.h>
#include <functional>
#include "../Timeline/TimelineModel.h"

class TrackerPanel final : public juce::Component,
                           private juce::ScrollBar::Listener
{
public:
    TrackerPanel();

    std::function<void()> onAddTrackRequested;
    std::function<void(int)> onRemoveTrackRequested;
    std::function<void(int)> onTrackSelected;
    std::function<void(int, const juce::String&)> onTrackNameChanged;
    std::function<void(int, bool)> onTrackArmChanged;
    std::function<void(int, bool)> onTrackMuteChanged;
    std::function<void(int, bool)> onTrackSoloChanged;
    std::function<void(int, bool)> onTrackMonitorChanged;
    std::function<void(int, float)> onTrackGainChanged;
    std::function<void(int, int)> onTrackInputChanged;
    std::function<void()> onZoomOutRequested;
    std::function<void()> onZoomInRequested;
    std::function<void(double)> onPlayheadPositionChanged;
    std::function<void(int, int, double)> onClipMoved;
    std::function<void()> onClipMoveCommitted;

    void setTrackCount(int newTrackCount);
    void setTrackName(int trackIndex, const juce::String& name);
    void setTrackArmed(int trackIndex, bool armed);
    void setTrackMuted(int trackIndex, bool muted);
    void setTrackSoloed(int trackIndex, bool soloed);
    void setTrackMonitored(int trackIndex, bool monitored);
    void setTrackLevel(int trackIndex, float level);
    void setTrackGain(int trackIndex, float gain);
    void setInputSources(const juce::Array<juce::String>& sourceNames);
    void setTrackInput(int trackIndex, int inputChannel);
    void setSelectedTrack(int trackIndex);
    void setTimelineModel(const cs::TimelineModel* model);
    void refreshTimelineView();
    void centerTransportInView();
    int getSelectedTrack() const noexcept { return selectedTrack; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    class TimelineCanvas final : public juce::Component
    {
    public:
        std::function<void(int)> onTrackSelected;
        std::function<void(int, const juce::String&)> onTrackNameChanged;
        std::function<void(int, bool)> onTrackArmChanged;
        std::function<void(int, bool)> onTrackMuteChanged;
        std::function<void(int, bool)> onTrackSoloChanged;
        std::function<void(int, bool)> onTrackMonitorChanged;
        std::function<void(int, float)> onTrackGainChanged;
        std::function<void(int, int)> onTrackInputChanged;
        std::function<void(double)> onPlayheadPositionChanged;
        std::function<void(int, int, double)> onClipMoved;
        std::function<void()> onClipMoveCommitted;

        void setTrackCount(int newTrackCount);
        void setTrackName(int trackIndex, const juce::String& name);
        void setTrackArmed(int trackIndex, bool armed);
        void setTrackMuted(int trackIndex, bool muted);
        void setTrackSoloed(int trackIndex, bool soloed);
        void setTrackMonitored(int trackIndex, bool monitored);
        void setTrackLevel(int trackIndex, float level);
        void setTrackGain(int trackIndex, float gain);
        void setInputSources(const juce::Array<juce::String>& sourceNames);
        void setTrackInput(int trackIndex, int inputChannel);
        void setSelectedTrack(int trackIndex);
        void setLaneHeight(int newLaneHeight);
        void setTimelineModel(const cs::TimelineModel* model);
        void setScrollSeconds(double seconds);
        double getTransportSeconds() const noexcept;
        double getVisibleDurationSeconds() const noexcept;
        double getTotalDurationSeconds() const noexcept;
        int getSelectedTrack() const noexcept { return selectedTrack; }

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        double xToTimelineSeconds(int x) const noexcept;
        int yToTrackIndex(int y) const noexcept;
        int hitTestClip(juce::Point<int> position) const;

        class TrackHeader final : public juce::Component
        {
        public:
            explicit TrackHeader(int newTrackIndex);

            std::function<void(int)> onSelected;
            std::function<void(int, const juce::String&)> onNameChanged;
            std::function<void(int, bool)> onArmChanged;
            std::function<void(int, bool)> onMuteChanged;
            std::function<void(int, bool)> onSoloChanged;
            std::function<void(int, bool)> onMonitorChanged;
            std::function<void(int, float)> onGainChanged;
            std::function<void(int, int)> onInputChanged;

            void setTrackIndex(int newTrackIndex);
            void setTrackName(const juce::String& name);
            void setSelected(bool selected);
            void setArmed(bool armed);
            void setMuted(bool muted);
            void setSoloed(bool soloed);
            void setMonitored(bool monitored);
            void setLevel(float level);
            void setGain(float gain);
            void setInputSources(const juce::Array<juce::String>& sourceNames);
            void setInputChannel(int inputChannel);

            void paint(juce::Graphics& g) override;
            void paintOverChildren(juce::Graphics& g) override;
            void resized() override;
            void mouseDown(const juce::MouseEvent&) override;

        private:
            int trackIndex = 0;
            float inputLevel = 0.0f;
            bool selected = false;
            juce::TextEditor nameEditor;
            juce::TextButton armButton { "Arm" };
            juce::TextButton muteButton { "M" };
            juce::TextButton soloButton { "S" };
            juce::TextButton monitorButton { "Mon" };
            juce::Label stereoLabel;
            juce::ComboBox inputSelector;
            juce::Label fxLabel;
            juce::Label dbLabel;
            juce::Slider gainSlider;
        };

        int trackCount = 0;
        int selectedTrack = -1;
        int laneHeight = 100;
        int draggingClipIndex = -1;
        int draggingOriginalTrack = -1;
        double draggingOriginalStartSeconds = 0.0;
        bool draggingClipMoved = false;
        double scrollSeconds = 0.0;
        const cs::TimelineModel* timelineModel = nullptr;
        juce::StringArray trackNames;
        std::vector<bool> trackArmed;
        std::vector<bool> trackMuted;
        std::vector<bool> trackSoloed;
        std::vector<bool> trackMonitored;
        std::vector<float> trackLevels;
        std::vector<float> trackGains;
        juce::Array<juce::String> inputSourceNames;
        juce::OwnedArray<TrackHeader> trackHeaders;
    };

    juce::Label titleLabel;
    juce::Label hintLabel;
    juce::Label selectionLabel;
    juce::TextButton addTrackButton { "+ Add Track" };
    juce::TextButton removeTrackButton { "- Remove" };
    juce::TextButton compactButton { "Compact" };
    juce::TextButton comfortButton { "Comfort" };
    juce::TextButton tallButton { "Tall" };
    juce::TextButton zoomOutButton { "Zoom -" };
    juce::TextButton zoomInButton { "Zoom +" };
    juce::ScrollBar timelineScrollBar { false };
    TimelineCanvas canvas;

    int selectedTrack = -1;

    void refreshSelectionLabel();
    void refreshTimelineScrollBar();
};
