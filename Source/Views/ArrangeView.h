#pragma once

#include <JuceHeader.h>
#include "../Timeline/TimelineTypes.h"

class ArrangeView final : public juce::Component
{
public:
    struct ClipPlacement
    {
        juce::String displayName;
        juce::String assetFileName;
        int trackIndex = 0;
        int startBeat = 0;
        int lengthBeats = 4;
        double trimStart = 0.0;
        double trimEnd = 1.0;
        float gainDecibels = 0.0f;
        float fadeInNormalized = 0.0f;
        float fadeOutNormalized = 0.0f;
        bool reverse = false;
        bool normalize = false;
    };

    ArrangeView();

    void setTotalTrackCount(int newTrackCount);
    void setVisibleTrackCount(int newVisibleTrackCount);
    int getVisibleTrackCount() const noexcept { return visibleTrackCount; }
    int getTotalTrackCount() const noexcept { return totalTrackCount; }
    void setTrackName(int trackIndex, const juce::String& name);
    void setTrackKind(int trackIndex, cs::TrackKind kind);
    void setRecordedClips(const juce::StringArray& clipNames);
    void setAssetFiles(const juce::Array<juce::File>& files);
    void setSelectedTrack(int trackIndex);
    void addAssetClipToSelectedTrack(const juce::String& clipName);
    juce::ValueTree createState() const;
    void restoreState(const juce::ValueTree& state);
    double getTrimStart() const noexcept { return trimStart; }
    double getTrimEnd() const noexcept { return trimEnd; }
    float getGainDecibels() const noexcept { return gainDecibels; }
    float getFadeInNormalized() const noexcept { return fadeInNormalized; }
    float getFadeOutNormalized() const noexcept { return fadeOutNormalized; }
    bool isReverseEnabled() const noexcept { return reverseButton.getToggleState(); }
    bool isNormalizeEnabled() const noexcept { return normalizeButton.getToggleState(); }
    const juce::Array<ClipPlacement>& getPlacedClips() const noexcept { return placedClips; }
    int getSelectedClipIndex() const noexcept { return selectedClipIndex; }

    std::function<void(int)> onTrackSelected;
    std::function<void()> onAddTrackRequested;
    std::function<void(int)> onRemoveTrackRequested;
    std::function<void()> onImportAssetRequested;
    std::function<void(const juce::File&)> onAssetPreviewRequested;
    std::function<void()> onArrangementChanged;

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
            void setClipData(const ClipPlacement& placementData, int newModelIndex, juce::Colour newColour);
            void setSelected(bool shouldSelect);
            void updateBoundsFromState();
            void paint(juce::Graphics&) override;
            void mouseDown(const juce::MouseEvent& event) override;
            void mouseDrag(const juce::MouseEvent& event) override;
            void mouseUp(const juce::MouseEvent& event) override;

            std::function<void(int modelIndex, int newStartBeat)> onMoved;
            std::function<void(int modelIndex)> onSelected;

        private:
            ClipPlacement placement;
            int modelIndex = -1;
            juce::Colour colour;
            juce::Point<int> dragStart;
            int startBeatOnDrag = 0;
            bool selected = false;
        };

        struct LaneClipView
        {
            ClipPlacement placement;
            int modelIndex = -1;
            juce::Colour colour;
        };

        void setTrackIndex(int newTrackIndex);
        void setTrackName(const juce::String& newTrackName);
        void setTrackKind(cs::TrackKind kind);
        void setClips(const juce::Array<LaneClipView>& newClips, int selectedClipIndex);
        void setSelected(bool shouldSelect);
        void layoutClipBlocks();

        std::function<void(int modelIndex, int newStartBeat)> onClipMoved;
        std::function<void(int modelIndex)> onClipSelected;

        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        int trackIndex = 0;
        juce::String trackName;
        cs::TrackKind trackKind = cs::TrackKind::foley;
        juce::Array<LaneClipView> clips;
        juce::OwnedArray<ClipBlock> clipBlocks;
        bool selected = false;
        int selectedClipIndex = -1;

        void rebuildClipBlocks();
    };

    class Canvas final : public juce::Component
    {
    public:
        void setTrackCount(int newTrackCount, const juce::StringArray& trackNames);
        void setTrackName(int trackIndex, const juce::String& name);
        void setTrackKind(int trackIndex, cs::TrackKind kind);
        void setSeedClips(const juce::StringArray& clipNames);
        void setPlacedClips(const juce::Array<ClipPlacement>& clipPlacements, int selectedClipIndex);
        void setSelectedTrack(int trackIndex);

        std::function<void(int)> onTrackSelected;
        std::function<void(int modelIndex, int newStartBeat)> onClipMoved;
        std::function<void(int modelIndex)> onClipSelected;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        juce::OwnedArray<Lane> lanes;
        juce::StringArray trackNames;
        std::vector<cs::TrackKind> trackKinds;
        juce::StringArray seedClips;
        juce::Array<ClipPlacement> placedClips;
        int trackCount = 0;
        int selectedClipIndex = -1;

        void rebuildLaneClips();
    };

    class WaveformPanel final : public juce::Component
    {
    public:
        WaveformPanel();

        void setAudioFile(const juce::File& file);
        void paint(juce::Graphics&) override;

    private:
        juce::AudioFormatManager formatManager;
        juce::AudioThumbnailCache thumbnailCache { 8 };
        juce::AudioThumbnail thumbnail;
        juce::String emptyText { "Select a Foley sound to shape it." };
    };

    juce::Label titleLabel;
    juce::Label hintLabel;
    juce::Label assetLabel;
    juce::ComboBox assetSelector;
    juce::TextButton importAssetButton { "Import Foley" };
    juce::TextButton previewSliceButton { "Audition" };
    juce::TextButton placeAssetButton { "Place" };
    juce::TextButton duplicateClipButton { "Duplicate" };
    juce::TextButton deleteClipButton { "Delete" };
    juce::TextButton addTrackButton { "+ Layer" };
    juce::TextButton removeTrackButton { "- Layer" };
    juce::Label trimLabel;
    juce::Label clipInspectorLabel;
    juce::Label clipNameLabel;
    juce::TextEditor clipNameEditor;
    juce::Label clipTrackLabel;
    juce::ComboBox clipTrackSelector;
    juce::Label clipStartBeatLabel;
    juce::Slider clipStartBeatSlider;
    juce::Label clipLengthLabel;
    juce::Slider clipLengthSlider;
    juce::Label clipSelectionLabel;
    juce::Slider trimStartSlider;
    juce::Slider trimEndSlider;
    juce::Label actionLabel;
    juce::Slider gainSlider;
    juce::Slider fadeInSlider;
    juce::Slider fadeOutSlider;
    juce::ToggleButton reverseButton { "Reverse" };
    juce::ToggleButton normalizeButton { "Normalize" };
    juce::Label trimInfoLabel;
    WaveformPanel waveformPanel;
    juce::Viewport viewport;
    Canvas canvas;
    juce::StringArray trackNames;
    std::vector<cs::TrackKind> trackKinds;
    juce::Array<juce::File> assetFiles;
    int totalTrackCount = 0;
    int visibleTrackCount = 0;
    int selectedTrack = 0;
    int selectedAssetIndex = -1;
    double trimStart = 0.0;
    double trimEnd = 1.0;
    float gainDecibels = 0.0f;
    float fadeInNormalized = 0.0f;
    float fadeOutNormalized = 0.0f;
    juce::Array<ClipPlacement> placedClips;
    int selectedClipIndex = -1;
    bool suppressInspectorCallbacks = false;

    void updateCanvasTrackCount();
    void refreshAssetSelector();
    void refreshTrimUi();
    void selectAssetIndex(int index);
    juce::String makePlacedClipLabel() const;
    void placeSelectedAssetOnCurrentLayer();
    void notifyArrangementChanged();
    void refreshClipActionButtons();
    void refreshClipInspector();
    void applyEditorValuesToSelectedClip();
};
