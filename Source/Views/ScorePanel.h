#pragma once

#include <JuceHeader.h>

class ScorePanel final : public juce::Component
{
public:
    struct NoteEvent
    {
        int measure = 0;
        float beat = 1.0f;
        float durationBeats = 1.0f;
        int midiNote = 60;
        bool isRest = false;
        juce::String lyric;
    };

    struct PlaybackRequest
    {
        juce::Array<NoteEvent> notes;
        int tempoBpm = 96;
        juce::String songTitle;
    };

    ScorePanel();

    juce::ValueTree createState() const;
    void restoreState(const juce::ValueTree& state);
    PlaybackRequest createPlaybackRequest() const;

    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void(const PlaybackRequest&)> onPlayRequested;

private:
    enum class EditorSurface
    {
        staff,
        pianoRoll
    };

    class StaffCanvas final : public juce::Component
    {
    public:
        void setNotes(const juce::Array<NoteEvent>& notes, int selectedIndex);
        void updateLayoutForWidth(int width);
        std::function<void(int)> onNoteSelected;
        std::function<void(int, int, int, float)> onNoteDragged;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        struct DragState
        {
            int noteIndex = -1;
            int startMeasure = 0;
            float startBeat = 1.0f;
            int startMidiNote = 60;
            juce::Point<float> startPosition;
        };

        juce::Array<NoteEvent> notes;
        int selectedIndex = -1;
        juce::Array<juce::Rectangle<float>> noteBounds;
        DragState dragState;
        int measuresPerSystem = 4;

        int getTotalMeasures() const;
        int getSystemCount() const;
        juce::Rectangle<float> getSystemArea(int systemIndex) const;
        juce::Rectangle<float> getTrebleStaffArea(int systemIndex) const;
        juce::Rectangle<float> getBassStaffArea(int systemIndex) const;
        juce::Point<float> getNotePosition(const NoteEvent& note) const;
        static int midiForY(float y, juce::Rectangle<float> systemArea);
        float beatGridForX(float x, int systemIndex, int& measureOut) const;
    };

    class PianoRollCanvas final : public juce::Component
    {
    public:
        void setNotes(const juce::Array<NoteEvent>& notes, int selectedIndex);
        void updateLayoutForViewport(int viewportWidth);
        std::function<void(int)> onNoteSelected;
        std::function<void(int, int, int, float)> onNoteDragged;
        std::function<void(int, float)> onNoteResized;
        std::function<void(int, float, float)> onNoteCreated;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseDoubleClick(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        struct DragState
        {
            enum class Mode
            {
                none,
                move,
                resize
            };

            int noteIndex = -1;
            Mode mode = Mode::none;
            float anchorBeat = 0.0f;
        };

        juce::Array<NoteEvent> notes;
        int selectedIndex = -1;
        juce::Array<juce::Rectangle<float>> noteBounds;
        DragState dragState;
        int visibleMeasures = 8;

        int getTotalMeasures() const;
        float getPixelsPerBeat() const;
        juce::Rectangle<float> getGridArea() const;
        juce::Rectangle<float> getNoteBounds(const NoteEvent& note) const;
        int midiForY(float y) const;
        float snappedBeatForX(float x, int& measureOut) const;
    };

    void refreshEditors();
    void refreshEditorSurface();
    void applySelectedNoteEditors();
    void addNote();
    void removeSelectedNote();
    void nudgeSelectedNotePitch(int semitones);
    void nudgeSelectedNoteBeat(float delta);
    void updateStatusText();
    static juce::String midiName(int midiNote);

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;
    juce::Label songTitleLabel;
    juce::TextEditor songTitleEditor;
    juce::Label tempoLabel;
    juce::Slider tempoSlider;
    juce::Label keyLabel;
    juce::ComboBox keySelector;
    juce::Label timeSigLabel;
    juce::ComboBox timeSigSelector;
    juce::TextButton scoreViewButton { "Staff" };
    juce::TextButton pianoRollViewButton { "Piano Roll" };
    juce::TextButton playScoreButton { "Play Score" };
    juce::TextButton addNoteButton { "Add Note" };
    juce::TextButton removeNoteButton { "Remove Note" };
    juce::ToggleButton restToggleButton { "Rest" };
    juce::TextButton noteUpButton { "Note Up" };
    juce::TextButton noteDownButton { "Note Down" };
    juce::TextButton beatLeftButton { "Beat Left" };
    juce::TextButton beatRightButton { "Beat Right" };
    juce::Label selectedNoteLabel;
    juce::Slider pitchSlider;
    juce::Slider measureSlider;
    juce::Slider beatSlider;
    juce::Slider durationSlider;
    juce::TextEditor lyricEditor;
    juce::Label aiCoachLabel;
    juce::TextEditor aiCoachEditor;
    juce::Viewport scoreViewport;
    StaffCanvas staffCanvas;
    PianoRollCanvas pianoRollCanvas;

    juce::String songTitle { "Untitled Song" };
    int tempoBpm = 96;
    juce::String keySignature { "C Major" };
    juce::String timeSignature { "4/4" };
    juce::Array<NoteEvent> notes;
    int selectedNoteIndex = -1;
    EditorSurface activeSurface = EditorSurface::staff;
    bool suppressEditorCallbacks = false;
};
