#pragma once

#include <JuceHeader.h>
#include <functional>
#include "../Timeline/TimelineModel.h"
#include "../Video/VideoThumbnailCache.h"
#include "../Video/VideoScrubPreview.h"
#include "../Video/VideoPreviewComponent.h"

class TrackerPanel final : public juce::Component,
                           public juce::FileDragAndDropTarget,
                           private juce::ScrollBar::Listener
{
public:
    TrackerPanel();

    std::function<void()> onAddTrackRequested;
    std::function<void(int)> onRemoveTrackRequested;
    std::function<void(int)> onTrackSelected;
    std::function<void(int, const juce::String&)> onTrackNameChanged;
    std::function<void(int, cs::TrackKind)> onTrackKindChanged;
    // Fired once a track header drag exceeds the reorder threshold and is released: move
    // trackIndex to sit at destinationIndex.
    std::function<void(int, int)> onTrackReorderRequested;
    std::function<void(int, bool)> onTrackArmChanged;
    std::function<void(int, bool)> onTrackMuteChanged;
    std::function<void(int, bool)> onTrackSoloChanged;
    std::function<void(int, bool)> onTrackMonitorChanged;
    std::function<void(int, bool)> onTrackStereoChanged;
    std::function<void(int, float)> onTrackGainChanged;
    std::function<void(int, int)> onTrackInputChanged;
    std::function<void(int)> onTrackFxRequested;
    std::function<void(int)> onMoveToFolderRequested;
    std::function<void()> onZoomOutRequested;
    std::function<void()> onZoomInRequested;
    std::function<void(double)> onPlayheadPositionChanged;
    std::function<void(double, double)> onLoopRegionChanged;
    std::function<void()> onLoopRegionCleared;
    std::function<void(bool)> onTimelineSnapChanged;
    std::function<void(double)> onTimelineGridChanged;
    std::function<void()> onMarkerAddRequested;
    std::function<void(const juce::String&)> onMarkerClicked;
    std::function<void(int, int, double)> onClipMoved;
    std::function<void()> onClipMoveCommitted;
    // Non-destructive edge trim: leftEdge selects which boundary moved, boundarySeconds is its
    // new absolute timeline position. Fires on every drag tick like onClipMoved; onClipTrimCommitted
    // fires once on release, mirroring onClipMoveCommitted.
    std::function<void(int clipIndex, bool leftEdge, double boundarySeconds)> onClipTrimmed;
    std::function<void()> onClipTrimCommitted;
    std::function<void(int)> onClipSelected;
    std::function<void(int)> onClipRenameRequested;
    std::function<void(int, double)> onClipSplitRequested;
    std::function<void(int)> onClipDuplicateRequested;
    std::function<void(int)> onClipDeleteRequested;
    std::function<void(int)> onClipEditRequested;
    std::function<void(int, double)> onEmptyMidiClipRequested;
    std::function<void(double)> onTempoChanged;
    std::function<void(int, int)> onTimeSignatureChanged;
    std::function<void(const juce::String&)> onKeyChanged;
    // Automation tracks: fired when the header's Target button is clicked (build/show the
    // target picker), when a point/segment is edited live (push a fresh snapshot to the engine,
    // no save), and when an edit gesture completes (save-worthy).
    std::function<void(int)> onAutomationTargetRequested;
    std::function<void(int)> onAutomationPointChanged;
    std::function<void(int)> onAutomationDataCommitted;
    std::function<void(int, cs::AutomationRecordMode)> onAutomationRecordModeChanged;
    std::function<void(int, int)> onAutomationRecordingRateChanged;
    std::function<void(double noteHz)> onPitchPipeTriggered;
    std::function<void(const juce::String& targetId, const juce::String& displayLabel)> onLearnMidiRequested;
    std::function<void(const juce::StringArray&, int, double)> onAudioFilesDropped;

    void setTrackCount(int newTrackCount);
    void setTrackName(int trackIndex, const juce::String& name);
    void setTrackKind(int trackIndex, cs::TrackKind kind);
    void setTrackArmed(int trackIndex, bool armed);
    void setTrackMuted(int trackIndex, bool muted);
    void setTrackSoloed(int trackIndex, bool soloed);
    void setTrackMonitored(int trackIndex, bool monitored);
    void setTrackStereo(int trackIndex, bool stereo);
    void setTrackLevel(int trackIndex, float level);
    void setTrackGain(int trackIndex, float gain);
    void setInputSources(const juce::Array<juce::String>& sourceNames);
    void setTrackInput(int trackIndex, int inputChannel);
    void setTrackFxSummary(int trackIndex, int pluginCount);
    void setSelectedTrack(int trackIndex);
    void setSelectedClip(int clipIndex);
    void setTimingInfo(double bpm, int numerator, int denominator, const juce::String& key);
    void setTimelineModel(cs::TimelineModel* model);
    void refreshTimelineView();
    void centerTransportInView();
    // Drives the scrub preview overlay: finds whichever video clip covers timelineSeconds (if
    // any) and requests a decode of the matching source-relative frame. Called every playback
    // tick from MainComponent::timerCallback - see VideoScrubPreview for how it stays cheap
    // under a fast, continuously-moving call rate.
    void updateVideoPreview(double timelineSeconds);
    int getSelectedTrack() const noexcept { return selectedTrack; }
    void setAutomationTargetLabel(int trackIndex, const juce::String& label);
    void setAutomationRecordMode(int trackIndex, cs::AutomationRecordMode mode);
    void setAutomationRecordingRate(int trackIndex, int pointsPerSecond);
    void setTrackIndented(int trackIndex, bool indented);
    // Tints the header's left accent badge - used so a folder and everything routed into it
    // share a color at a glance. Pass juce::Colour() (transparent) to clear back to the default.
    void setTrackAccentColour(int trackIndex, juce::Colour colour);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;
    int trackIndexForDropY(int y) const noexcept;
    double timelineSecondsForDropX(int x) const noexcept;
    void applyWheelNavigation(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel);
    void scrollTimelineTo(double newRangeStart);

    class TimelineCanvas final : public juce::Component
    {
    public:
        std::function<void(int)> onTrackSelected;
        std::function<void(int, const juce::String&)> onTrackNameChanged;
        std::function<void(int, cs::TrackKind)> onTrackKindChanged;
        std::function<void(int, int)> onTrackReorderRequested;
        std::function<void(int, bool)> onTrackArmChanged;
        std::function<void(int, bool)> onTrackMuteChanged;
        std::function<void(int, bool)> onTrackSoloChanged;
        std::function<void(int, bool)> onTrackMonitorChanged;
        std::function<void(int, bool)> onTrackStereoChanged;
        std::function<void(int, float)> onTrackGainChanged;
        std::function<void(int, int)> onTrackInputChanged;
        std::function<void(int)> onTrackFxRequested;
        std::function<void(int)> onTrackRemoveRequested;
        std::function<void(int)> onMoveToFolderRequested;
        std::function<void()> onAddTrackRequested;
        std::function<void(double)> onPlayheadPositionChanged;
        std::function<void(double, double)> onLoopRegionChanged;
        std::function<void()> onLoopRegionCleared;
        std::function<void(const juce::String&)> onMarkerClicked;
        std::function<void(int, int, double)> onClipMoved;
        std::function<void()> onClipMoveCommitted;
        std::function<void(int, bool, double)> onClipTrimmed;
        std::function<void()> onClipTrimCommitted;
        std::function<void(int)> onClipSelected;
        std::function<void(int)> onClipRenameRequested;
        std::function<void(int, double)> onClipSplitRequested;
        std::function<void(int)> onClipDuplicateRequested;
        std::function<void(int)> onClipDeleteRequested;
        std::function<void(int)> onClipEditRequested;
        std::function<void(int, double)> onEmptyMidiClipRequested;
        std::function<void(int)> onAutomationTargetRequested;
        std::function<void(int)> onAutomationPointChanged;
        std::function<void(int)> onAutomationDataCommitted;
        std::function<void(int, cs::AutomationRecordMode)> onAutomationRecordModeChanged;
        std::function<void(int, int)> onAutomationRecordingRateChanged;
        std::function<void(double)> onScrollRequested;
        std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onMouseWheelUsed;

        void setTrackCount(int newTrackCount);
        void setTrackName(int trackIndex, const juce::String& name);
        void setTrackKind(int trackIndex, cs::TrackKind kind);
        void setTrackArmed(int trackIndex, bool armed);
        void setTrackMuted(int trackIndex, bool muted);
        void setTrackSoloed(int trackIndex, bool soloed);
        void setTrackMonitored(int trackIndex, bool monitored);
        void setTrackStereo(int trackIndex, bool stereo);
        void setTrackLevel(int trackIndex, float level);
        void setTrackGain(int trackIndex, float gain);
        void setInputSources(const juce::Array<juce::String>& sourceNames);
        void setTrackInput(int trackIndex, int inputChannel);
        void setTrackFxSummary(int trackIndex, int pluginCount);
        void setSelectedTrack(int trackIndex);
        void setSelectedClip(int clipIndex);
        void setLaneHeight(int newLaneHeight);
        void setTimelineModel(cs::TimelineModel* model);
        void setAutomationTargetLabel(int trackIndex, const juce::String& label);
        void setAutomationRecordMode(int trackIndex, cs::AutomationRecordMode mode);
        void setAutomationRecordingRate(int trackIndex, int pointsPerSecond);
        void setTrackIndented(int trackIndex, bool indented);
        void setTrackAccentColour(int trackIndex, juce::Colour colour);
        void setScrollSeconds(double seconds);
        void updateVideoPreview(double timelineSeconds);
        double getTransportSeconds() const noexcept;
        double getVisibleDurationSeconds() const noexcept;
        double getTotalDurationSeconds() const noexcept;
        int getTrackCount() const noexcept { return trackCount; }
        int getLaneHeight() const noexcept { return laneHeight; }
        int getSelectedTrack() const noexcept { return selectedTrack; }

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseMove(const juce::MouseEvent& event) override;
        void mouseDoubleClick(const juce::MouseEvent& event) override;
        void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    private:
        enum class LoopEdge { none, left, right };
        // Which boundary of a clip a trim-drag is grabbing - deliberately separate from LoopEdge
        // even though the shape is identical, since they gate unrelated drag gestures.
        enum class ClipEdge { none, left, right };

        double xToTimelineSeconds(int x) const noexcept;
        int xForTimelineSeconds(double seconds) const noexcept;
        int yToTrackIndex(int y) const noexcept;
        int hitTestClip(juce::Point<int> position) const;
        // Only reports a hit within a few pixels of a clip's left/right edge (outClipIndex is set
        // only when the result isn't ClipEdge::none) - checked before hitTestClip in mouseDown so
        // grabbing near a border trims instead of moving the whole clip.
        ClipEdge hitTestClipEdge(juce::Point<int> position, int& outClipIndex) const;
        juce::String hitTestMarker(int x) const;
        LoopEdge hitTestLoopEdge(int x) const noexcept;
        bool hitTestLoopClose(juce::Point<int> position) const noexcept;

        // Automation lane editing (only meaningful when the hovered track's kind is automation).
        juce::Rectangle<int> getAutomationLaneBounds(int trackIndex) const;
        juce::String hitTestAutomationPoint(int trackIndex, juce::Point<int> position) const;
        juce::String hitTestAutomationTensionHandle(int trackIndex, juce::Point<int> position) const;
        void drawAutomationLane(juce::Graphics& g, const cs::TimelineClip& clip, juce::Rectangle<int> lane) const;
        // Placeholder film-strip rendering until the video decode service (Phase 1's own
        // sub-phase) can hand back real per-frame thumbnails - draws sprocket-hole ticks and the
        // source filename so a video clip reads as unmistakably different from an audio one.
        void drawVideoThumbnailStrip(juce::Graphics& g, const cs::TimelineClip& clip, juce::Rectangle<int> area) const;
        void showAutomationSegmentMenu(int trackIndex, const juce::String& pointId, juce::Point<int> screenAnchor);
        double autoScrollWhileDragging(int x);

        class TrackHeader final : public juce::Component
        {
        public:
            explicit TrackHeader(int newTrackIndex);

            std::function<void(int)> onSelected;
            std::function<void(int, const juce::String&)> onNameChanged;
            std::function<void(int, cs::TrackKind)> onKindChanged;
            std::function<void(int, bool)> onArmChanged;
            std::function<void(int, bool)> onMuteChanged;
            std::function<void(int, bool)> onSoloChanged;
            std::function<void(int, bool)> onMonitorChanged;
            std::function<void(int, bool)> onStereoChanged;
            std::function<void(int, float)> onGainChanged;
            std::function<void(int, int)> onInputChanged;
            std::function<void(int)> onFxRequested;
            std::function<void(int)> onRemoveRequested;
            std::function<void(int)> onMoveToFolderRequested;
            std::function<void(int)> onTargetRequested;
            std::function<void(int, cs::AutomationRecordMode)> onRecordModeChanged;
            std::function<void(int, int)> onRecordingRateChanged;
            // int argument is the drag position in the OWNING TimelineCanvas's coordinate space
            // (this header's getY() + the event's local y), not this component's own local y -
            // the canvas is what knows how to turn a y position into a target track index.
            std::function<void(int, int)> onReorderDragged;
            std::function<void(int, int)> onReorderCommitted;

            void setTrackIndex(int newTrackIndex);
            void setTrackName(const juce::String& name);
            void setTrackKind(cs::TrackKind kind);
            void setSelected(bool selected);
            void setArmed(bool armed);
            void setMuted(bool muted);
            void setSoloed(bool soloed);
            void setMonitored(bool monitored);
            void setStereo(bool stereo);
            void setLevel(float level);
            void setGain(float gain);
            void setInputSources(const juce::Array<juce::String>& sourceNames);
            void setInputChannel(int inputChannel);
            void setFxSummary(int pluginCount);
            // Automation tracks have no source/output of their own - this shows what control the
            // lane is currently driving instead of a gain slider (e.g. "Track 3 -> Volume").
            void setAutomationTargetLabel(const juce::String& label);
            void setAutomationRecordMode(cs::AutomationRecordMode mode);
            void setAutomationRecordingRate(int pointsPerSecond);
            // Purely cosmetic left-inset on the name editor so a track nested under a folder is
            // visually distinguishable from a top-level one - no effect on lane layout/height.
            void setIndented(bool shouldIndent);
            void setAccentColour(juce::Colour colour);

            void paint(juce::Graphics& g) override;
            void paintOverChildren(juce::Graphics& g) override;
            void resized() override;
            void mouseDown(const juce::MouseEvent&) override;
            void mouseDrag(const juce::MouseEvent&) override;
            void mouseUp(const juce::MouseEvent&) override;

        private:
            void refreshInputSelectorForKind();
            void populateMidiChannelOptions();

            int trackIndex = 0;
            float inputLevel = 0.0f;
            bool selected = false;
            juce::TextEditor nameEditor;
            cs::TrackKind trackKind = cs::TrackKind::audio;
            juce::TextButton kindButton;
            juce::TextButton armButton { "Arm" };
            juce::TextButton muteButton { "M" };
            juce::TextButton soloButton { "S" };
            juce::TextButton monitorButton { "Mon" };
            juce::TextButton stereoButton { "Mono" };
            juce::TextButton menuButton;
            juce::ComboBox inputSelector;
            juce::Array<juce::String> lastAudioSourceNames;
            juce::TextButton fxButton { "FX" };
            juce::Label dbLabel;
            juce::Slider gainSlider;
            juce::TextButton targetButton { "No Target" };
            juce::TextButton recordModeButton { "Touch" };
            cs::AutomationRecordMode recordMode = cs::AutomationRecordMode::touch;
            int recordingRate = 10;
            bool indented = false;
            juce::Colour accentColour = juce::Colour();
            bool reorderDragActive = false;
        };

        int trackCount = 0;
        int selectedTrack = -1;
        int selectedClipIndex = -1;
        int laneHeight = 100;
        int draggingClipIndex = -1;
        int draggingOriginalTrack = -1;
        double draggingOriginalStartSeconds = 0.0;
        bool draggingClipMoved = false;
        int trimmingClipIndex = -1;
        ClipEdge trimmingClipEdge = ClipEdge::none;
        double trimOriginalStartSeconds = 0.0;
        double trimOriginalDurationSeconds = 0.0;
        // mutable: populated lazily from drawVideoThumbnailStrip, a const paint helper (matching
        // drawAutomationLane's own const-ness, since neither touches this component's layout).
        mutable cs::VideoThumbnailCache videoThumbnailCache;
        cs::VideoScrubPreview scrubPreview;
        cs::VideoPreviewComponent videoPreview;
        bool draggingLoopRegion = false;
        bool loopRegionMoved = false;
        double loopDragStartSeconds = 0.0;
        double loopDragCurrentSeconds = 0.0;
        LoopEdge resizingLoopEdge = LoopEdge::none;
        double loopEdgeOtherSeconds = 0.0; // the edge NOT being dragged, held fixed during a resize
        double loopEdgeLiveSeconds = 0.0;  // live (unsnapped-preview) position of the edge being dragged
        double scrollSeconds = 0.0;
        cs::TimelineModel* timelineModel = nullptr;
        int draggingAutomationTrackIndex = -1;
        juce::String draggingAutomationPointId;
        int draggingAutomationTensionTrackIndex = -1;
        juce::String draggingAutomationTensionPointId;
        int reorderDragTrackIndex = -1;
        int reorderDragTargetIndex = -1;
        juce::StringArray trackNames;
        std::vector<cs::TrackKind> trackKinds;
        std::vector<bool> trackArmed;
        std::vector<bool> trackMuted;
        std::vector<bool> trackSoloed;
        std::vector<bool> trackMonitored;
        std::vector<bool> trackStereo;
        std::vector<float> trackLevels;
        std::vector<float> trackGains;
        juce::Array<juce::String> inputSourceNames;
        juce::OwnedArray<TrackHeader> trackHeaders;
    };

    juce::Label titleLabel;
    juce::Label hintLabel;
    juce::Label selectionLabel;
    juce::Label timingLabel;
    juce::TextEditor bpmEditor;
    juce::TextEditor timeSignatureEditor;
    juce::ComboBox keySelector;
    juce::TextButton compactButton { "Compact" };
    juce::TextButton comfortButton { "Comfort" };
    juce::TextButton tallButton { "Tall" };
    juce::TextButton zoomOutButton { "Zoom -" };
    juce::TextButton zoomInButton { "Zoom +" };
    juce::TextButton addMarkerButton { "+ Marker" };
    juce::TextButton pitchPipeButton { "Pitch Pipe" };
    juce::TextButton snapToggleButton { "Snap" };
    juce::ComboBox gridResolutionCombo;
    juce::ScrollBar timelineScrollBar { false };
    TimelineCanvas canvas;
    cs::TimelineModel* timelineModel = nullptr;
    bool fileDragActive = false;

    int selectedTrack = -1;
    int selectedClipIndex = -1;

    void refreshSelectionLabel();
    void refreshTimelineScrollBar();
    void commitTimingEdits();
};
