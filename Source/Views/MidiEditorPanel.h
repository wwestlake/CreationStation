#pragma once

#include <JuceHeader.h>
#include "../Timeline/TimelineModel.h"

class MidiEditorPanel final : public juce::Component,
                              private juce::Timer
{
public:
    MidiEditorPanel();

    void setClip(cs::TimelineModel* model, int clipIndex);
    void closeClip();
    bool isEditingClip(int clipIndex) const noexcept { return timelineModel != nullptr && editingClipIndex == clipIndex; }
    int getEditingClipIndex() const noexcept { return editingClipIndex; }
    double getLocalTransportSeconds() const noexcept { return localTransportSeconds; }
    bool isLocalLoopEnabled() const noexcept { return localLoopEnabled; }
    double getLocalLoopStartSeconds() const noexcept { return localLoopStartSeconds; }
    double getLocalLoopEndSeconds() const noexcept { return localLoopEndSeconds; }

    std::function<void()> onNotesChanged;
    std::function<void()> onCloseRequested;
    std::function<void()> onPlayRequested;
    std::function<void()> onStopRequested;
    std::function<void(bool)> onLoopEnabledChanged;
    std::function<void(double)> onTransportChanged;
    std::function<void(double, double)> onLoopRegionChanged;
    std::function<void()> onLoopRegionCleared;
    // Fired to audition a pitch live through the track's instrument, the way clicking/dragging a
    // note in Reaper's piano roll does. isOn=true on note start, isOn=false to release it.
    std::function<void(int pitch, int velocity, bool isOn)> onAuditionNote;

    void setPlaybackState(bool playing, bool recording);
    void setDisplayedTransportSeconds(double seconds, bool fromMainTransport);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class PianoLane final : public juce::Component
    {
    public:
        void setPanel(MidiEditorPanel* owner) { panel = owner; }
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        MidiEditorPanel* panel = nullptr;
    };

    class GridView final : public juce::Component
    {
    public:
        void setPanel(MidiEditorPanel* owner) { panel = owner; }
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseDoubleClick(const juce::MouseEvent& event) override;
        void mouseMove(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;
        void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    private:
        MidiEditorPanel* panel = nullptr;
    };

    class TimeRuler final : public juce::Component
    {
    public:
        void setPanel(MidiEditorPanel* owner) { panel = owner; }
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseMove(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;
        void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    private:
        MidiEditorPanel* panel = nullptr;
    };

    class VelocityLane final : public juce::Component
    {
    public:
        void setPanel(MidiEditorPanel* owner) { panel = owner; }
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;

    private:
        MidiEditorPanel* panel = nullptr;
    };

    class CCLane final : public juce::Component
    {
    public:
        void setPanel(MidiEditorPanel* owner) { panel = owner; }
        void setController(int newController) { controller = newController; repaint(); }
        int getController() const noexcept { return controller; }
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;

    private:
        void drawPointAt(const juce::MouseEvent& event);

        MidiEditorPanel* panel = nullptr;
        int controller = 1;
    };

    friend class PianoLane;
    friend class TimeRuler;
    friend class GridView;
    friend class VelocityLane;
    friend class CCLane;

    enum class DragMode
    {
        none,
        moveNotes,
        resizeNotes,
        marquee
    };

    struct DragState
    {
        enum class ResizeEdge
        {
            none,
            left,
            right
        };

        struct NoteSnapshot
        {
            juce::String id;
            double startBeats = 0.0;
            double lengthBeats = 0.0;
            int pitch = 60;
        };

        DragMode mode = DragMode::none;
        juce::Point<float> startPos;
        juce::String primaryNoteId;
        ResizeEdge resizeEdge = ResizeEdge::none;
        juce::Array<NoteSnapshot> noteSnapshots;
    };

    double pixelsPerBeat() const noexcept { return 60.0 * horizontalZoom; }
    float pixelsPerPitch() const noexcept { return 14.0f; }
    int totalBeatsVisible() const;
    double xToBeat(float x) const;
    int yToPitch(float y) const;
    float beatToX(double beat) const;
    float pitchToY(int pitch) const;
    double snapBeat(double beat) const;

    void rebuildNoteBounds();
    void handleGridMouseDown(const juce::MouseEvent& event);
    void handleGridMouseDrag(const juce::MouseEvent& event);
    void handleGridMouseUp(const juce::MouseEvent& event);
    void handleGridDoubleClick(const juce::MouseEvent& event);
    void handleGridMouseMove(const juce::MouseEvent& event);
    void handleGridMouseExit();
    void handlePianoLaneMouseDown(const juce::MouseEvent& event);
    void handlePianoLaneMouseDrag(const juce::MouseEvent& event);
    void handlePianoLaneMouseUp();
    void handleVelocityMouseDown(const juce::MouseEvent& event);
    void handleVelocityMouseDrag(const juce::MouseEvent& event);
    void handleTimeRulerMouseMove(const juce::MouseEvent& event);
    void handleTimeRulerMouseDown(const juce::MouseEvent& event);
    void handleTimeRulerMouseDrag(const juce::MouseEvent& event);
    void handleTimeRulerMouseUp(const juce::MouseEvent& event);
    void handleMouseWheel(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel, bool fromGrid);
    void updateHoverBeat(double beat);
    void clearHoverBeat();
    void applyHorizontalZoom(double factor, float anchorX);
    void keepPlayheadVisible();
    void selectNote(const juce::String& noteId, bool addToSelection);
    void clearSelection();
    void deleteSelectedNotes();
    void quantizeSelectedNotes();
    void applySwingToSelection();
    void humanizeSelection();
    void updateLaneVisibility();
    void generateTestBeat();
    void extendClipToFitNotes();
    bool keyPressed(const juce::KeyPress& key) override;
    void timerCallback() override;

    juce::Label titleLabel;
    juce::Label clipLabel;
    juce::TextButton closeButton { "Close" };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::ToggleButton loopToggle;
    juce::ComboBox gridSizeCombo;
    juce::ToggleButton snapToggle;
    juce::Slider quantizeStrengthSlider;
    juce::TextButton quantizeButton { "Quantize" };
    juce::Slider swingSlider;
    juce::TextButton swingButton { "Swing" };
    juce::TextButton zoomInButton { "+" };
    juce::TextButton zoomOutButton { "-" };

    juce::TextButton humanizeButton { "Humanize" };
    juce::Slider humanizeTimingSlider;
    juce::Slider humanizeVelocitySlider;
    juce::Slider humanizeLengthSlider;
    juce::ComboBox laneSelectorCombo;
    juce::TextButton generateTestBeatButton { "Fill Test Beat (1 min)" };
    juce::Label hintLabel;

    PianoLane pianoLane;
    TimeRuler timeRuler;
    juce::Viewport gridViewport;
    GridView gridView;
    VelocityLane velocityLane;
    CCLane ccLane;

    cs::TimelineModel* timelineModel = nullptr;
    int editingClipIndex = -1;
    double horizontalZoom = 1.0;
    juce::StringArray selectedNoteIds;
    juce::Array<juce::Rectangle<float>> noteBounds; // parallel to clip's midiNotes vector
    DragState dragState;
    juce::Rectangle<float> marqueeRect;
    juce::Point<int> lastViewPosition;
    int auditioningPitch = -1;
    double hoveredBeat = -1.0;
    bool isPlaying = false;
    bool isRecording = false;
    bool rulerDraggingLoop = false;
    bool rulerLoopSelectionChanged = false;
    double rulerDragStartBeat = 0.0;
    double rulerDragCurrentBeat = 0.0;
    bool sustainNoteClickAudition = false;
    double localTransportSeconds = 0.0;
    double displayedTransportSeconds = 0.0;
    bool transportDrivenByMain = false;
    bool localLoopEnabled = false;
    double localLoopStartSeconds = 0.0;
    double localLoopEndSeconds = 0.0;
};
