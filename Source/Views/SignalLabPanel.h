#pragma once

#include <JuceHeader.h>
#include "../Audio/PatchRuntimePlayer.h"
#include "../Audio/PatchLiveVoice.h"
#include "../Patch/PatchModel.h"

// A juce::Slider that also reports a right-click -- used for the Scope
// panel's front-panel knobs, which need a "Learn MIDI..." context menu the
// way a real bench scope's knobs would just be knobs (no menu at all, but
// this is the closest UI-only equivalent to "point a hardware controller at
// this and turn it").
class LearnableKnob final : public juce::Slider
{
public:
    std::function<void()> onRightClick;

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isRightButtonDown() && onRightClick)
        {
            onRightClick();
            return;
        }
        juce::Slider::mouseDown(event);
    }
};

class SignalLabPanel final : public juce::Component,
                             private juce::Timer
{
public:
    struct SignalRecipe
    {
        SignalRecipe();

        juce::String name { "Signal-Lab-Render" };
        juce::String description;
        double sampleRate = 48000.0;
        double durationSeconds = 5.0;
        juce::String sinkMode { "audio" }; // "audio" (live device) or "wave" (render to project file)
        float baseFrequencyHz = 180.0f;
        float sineLevel = 0.0f;
        float sawLevel = 0.0f;
        float squareLevel = 0.0f;
        float triangleLevel = 0.0f;
        float noiseLevel = 0.0f;
        juce::String filterMode { "lowpass" };
        float filterCutoffHz = 3600.0f;
        float filterResonance = 0.90f;
        float filterEnvelopeAmount = 0.35f;
        float macroHardness = 0.50f;
        float macroWeight = 0.50f;
        float macroAir = 0.50f;
        float macroGrit = 0.25f;
        float macroSize = 0.50f;
        juce::String envelopeCurveMode { "smooth" };
        juce::String automationCurveMode { "smooth" };
        float pitchSweepSemitones = 0.0f;
        juce::Array<cw::PatchAutomationPoint> envelopePoints;
        juce::Array<cw::PatchAutomationLane> automationLanes;
    };

    SignalLabPanel();

    // Mirrors WorkstationAudioEngine::LiveMidiControlChange without pulling
    // that (heavy, engine-level) header into this UI panel -- MainComponent
    // adapts one to the other when forwarding.
    struct MidiControlChange
    {
        juce::String deviceId;
        int channel = 1;
        int number = 0;
        bool isController = false;
        float value = 0.0f;
    };

    juce::ValueTree createState() const;
    void restoreState(const juce::ValueTree& state);
    void resetToBlankSignal();
    bool loadPatchDocument(const cw::PatchDocument& document, juce::String& errorMessage);
    void applyAiTemplate(const juce::String& templateName);
    bool previewCurrentSignal();
    // Called at UI-timer rate (see MainComponent::timerCallback) with any
    // MIDI control values that changed since the last call. Updates every
    // placed midiFader/midiButton node whose learned binding matches one of
    // these changes -- if a PatchLiveVoice playthrough is currently active,
    // pushes the new value straight into it via onLiveMidiValueChanged so
    // the already-playing sound updates immediately, no rebuild/recompile
    // involved. Also marks the graph dirty so the offline Preview/
    // Render-to-Project path (unrelated to live playback) picks up the new
    // value whenever it's next used.
    void applyLiveMidiControlChanges(const juce::Array<MidiControlChange>& changes);

    std::function<void(const juce::ValueTree& stateBeforeEdit, const juce::String& label)> onUndoCheckpointRequested;
    std::function<void()> onInteractionStarted;
    std::function<void(const juce::AudioBuffer<float>&, double sampleRate, const juce::String& suggestedName)> onPreviewRequested;
    std::function<void(const juce::AudioBuffer<float>&, double sampleRate, const juce::String& suggestedName)> onRenderRequested;
    std::function<void(const juce::String& patchJson, const juce::String& suggestedName)> onPatchExportRequested;
    std::function<void(const juce::String& patchJson, const juce::String& suggestedName)> onPatchSaveToLibraryRequested;
    std::function<void()> onPatchLoadRequested;
    std::function<void()> onStopRequested;
    std::function<void()> onAudioSettingsRequested;
    // Signal Lab live playback -- see PatchLiveVoice.h. Wired by
    // MainComponent to the matching WorkstationAudioEngine wrapper methods
    // (engine.rebuildSignalLabLiveGraph, etc.), same callback-injection
    // pattern as onPreviewRequested/onRenderRequested above rather than
    // holding a direct engine reference.
    std::function<void(const cw::PatchDocument&, const PatchLiveBindingMap&)> onLiveGraphRebuildRequested;
    std::function<void(double durationSeconds)> onLiveStartRequested;
    std::function<void()> onLiveStopRequested;
    std::function<bool()> onLiveIsActiveRequested;
    std::function<bool()> onLiveFinishedFlagRequested;
    std::function<void(const juce::String& nodeId, float value)> onLiveMidiValueChanged;
    // Fires whenever a Fader Control node's live value changes and it was
    // learned from a real motorized fader (pitch-wheel binding). Lets the
    // physical fader's motor track the software value in real time instead
    // of springing back to a stale position whenever it's released -- see
    // XTouchControlSurface::sendRawFaderFeedback.
    std::function<void(int channel, float value)> onMidiFaderFeedbackRequested;
    // Fires with the full current set of MIDI channels bound to a learned
    // Fader Control node, whenever that set changes (Learn completes, a
    // node is deleted, a patch loads). Lets the control surface stop
    // treating those channels as Tracker channel-strip faders -- otherwise
    // both systems drive the same physical fader from the same incoming
    // message.
    std::function<void(const juce::Array<int>&)> onFaderChannelClaimsChanged;
    // Live scope: pulls up to numSamplesRequested of live samples for
    // nodeId's tap into dest, returns how many were actually copied.
    std::function<int(const juce::String& nodeId, juce::AudioBuffer<float>& dest, int numSamplesRequested)> onLiveScopeSamplesRequested;
    // Pushes the current set of entity node ids that need a live tap --
    // see resolveScopeTapNodeIds()/refreshLiveScopeTaps().
    std::function<void(const juce::Array<juce::String>&)> onLiveScopeTapsChanged;
    std::function<double()> onLiveScopeSampleRateRequested;
    // Requests the app-level "Learn MIDI Binding" dialog (shared with the
    // transport buttons' MIDI learn -- see MainComponent::requestGenericMidiLearn).
    // onLearned fires once with the captured (deviceId, channel, number, isController).
    // wantsContinuousControl tells the capture step what kind of message to
    // actually wait for (true = a fader/knob's CC or pitch-wheel data,
    // false = a button's Note/CC) so it doesn't grab the wrong thing --
    // e.g. a motorized fader's touch-sensor Note firing before its real
    // pitch-wheel position data. See WorkstationAudioEngine::MidiLearnKind
    // for the full reasoning; kept as a plain bool here rather than pulling
    // that engine-level enum into this UI header.
    std::function<void(const juce::String& displayLabel, bool wantsContinuousControl,
                       std::function<void(juce::String deviceId, int channel, int number, bool isController)> onLearned)> onMidiLearnRequested;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct GraphNodeModel
    {
        juce::String id;
        juce::String type;
        juce::String title;
        juce::String targetParameter;
        juce::Point<int> position;
        juce::Colour accent;
        bool locked = false;
        bool required = false;

        // Mixer only: one entry per signal input's volume (0..1), default
        // two inputs. Variable-arity -- grown via the node's own "+ Input"
        // button rather than being a fixed registry shape like every other
        // node type's ports.
        juce::Array<float> mixerInputVolumes { 1.0f, 1.0f };

        // Oscillator/noise sources only (sine/saw/square/triangle/noise):
        // each instance owns its own level and frequency instead of sharing
        // the recipe's old singleton fields -- that's what made two Sine
        // nodes edit the same value. frequencyHz is ignored for noise.
        float oscillatorLevel = 0.5f;
        float oscillatorFrequencyHz = 180.0f;

        // Filter instances only: same reasoning as oscillators above -- each
        // Filter node owns its own cutoff/resonance/mode instead of the
        // recipe's old singleton fields, so multiple Filters in different
        // chain positions can have different settings.
        juce::String filterMode { "lowpass" };
        float filterCutoffHz = 3600.0f;
        float filterResonance = 0.90f;
        float filterEnvelopeAmount = 0.35f;

        // Envelope instances only: each Envelope node owns its own curve.
        juce::String envelopeCurveMode { "smooth" };
        juce::Array<cw::PatchAutomationPoint> envelopePoints;

        // MIDI Control nodes only (midiFader / midiButton): the learned
        // hardware binding (empty deviceId + number 0 == not learned yet)
        // and this node's current live value, updated from incoming MIDI in
        // applyLiveMidiControlChanges(). midiButtonMode ("momentary" |
        // "toggle") only applies to midiButton.
        bool midiLearned = false;
        juce::String midiDeviceId;
        juce::String midiDeviceLabel;
        int midiChannel = 1;
        int midiNumber = 0;
        bool midiIsController = true;
        juce::String midiButtonMode { "momentary" };
        float midiLiveValue = 0.0f;
    };

    enum class PortValueType { Float, Int, Bool };

    struct GraphPort
    {
        juce::String portId;
        juce::String label;
        bool isOutput = false;
        bool isExec = false;
        PortValueType valueType = PortValueType::Float;
        juce::Point<float> position;
    };

    struct PortHit
    {
        bool found = false;
        int nodeIndex = -1;
        GraphPort port;
    };

    // What a port is actually carrying right now -- the default, a
    // manually-set value, or a live-fed-in one -- so the canvas can show
    // real data next to each port instead of just a static label.
    struct PortValueDisplay
    {
        juce::String text;
        bool isLive = false; // wired to a Get-variable or MIDI Control node
    };

    struct GraphConnection
    {
        juce::String id;
        juce::String fromNodeId;
        juce::String fromPortId;
        juce::String toNodeId;
        juce::String toPortId;
        bool isExec = false;
        PortValueType valueType = PortValueType::Float;
        juce::Array<juce::Point<int>> waypoints;
    };

    struct GraphValidationError
    {
        juce::String nodeId;
        juce::String message;
    };

    struct LocalControlVariable
    {
        juce::String id;
        juce::String name;
        juce::String description;
        juce::String valueType { "Float" };
        juce::String accessScope { "Private" };
        juce::String targetParameter;
        float value = 0.5f;
        bool exposedToAutomation = true;
    };

    struct ProbeSettings
    {
        bool scopeEnabled = false;
        bool analyzerEnabled = false;
        double analyzerMinHz = 20.0;
        double analyzerMaxHz = 20000.0;
        double analyzerDbFloor = -96.0;
        double analyzerSmoothing = 0.55;
    };

    class EnvelopeEditor final : public juce::Component
    {
    public:
        EnvelopeEditor();

        void setRecipe(const SignalRecipe& recipe);
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseDoubleClick(const juce::MouseEvent& event) override;

        std::function<void()> onGestureBegin;
        std::function<void()> onGestureEnd;
        std::function<void(const juce::String& label)> onDiscreteEditRequested;
        std::function<void(const juce::Array<cw::PatchAutomationPoint>&)> onEnvelopeChanged;

    private:
        SignalRecipe recipe;
        int dragIndex = -1;

        juce::Rectangle<float> getPlotArea() const;
        juce::Point<float> toScreen(float normalizedX, float normalizedY) const;
        juce::Point<float> getPoint(int index) const;
        int findPointAt(juce::Point<float> position) const;
    };

    // Self-contained oscilloscope: owns its own timebase/start-time/level-zoom/
    // trigger-level controls (like a real bench scope's front panel) so it
    // works identically wherever it's dropped -- the inline inspector preview
    // and the detachable tool window both just call setBuffer().
    class ScopePanel final : public juce::Component,
                              private juce::Timer
    {
    public:
        ScopePanel();
        // Static/offline path: a post-render snapshot, shown when nothing
        // is playing (or before Play has ever been pressed). Live playback
        // supersedes this via the timer below, which repaints from fresh
        // engine data instead.
        void setBuffer(const juce::AudioBuffer<float>& buffer, double newSampleRate);
        // Which entity this scope probes -- its own node id if wired to a
        // specific point in the graph, resolved by SignalLabPanel; needed
        // to ask the live engine for the right tap. Safe to call at any
        // time, including before the first live snapshot request.
        void setNodeId(const juce::String& newNodeId) { nodeId = newNodeId; }
        const juce::String& getNodeId() const noexcept { return nodeId; }
        // Which entity id to actually ask the live engine for -- NOT the
        // same as nodeId (this scope's own graph-node id). A Scope node's
        // id identifies the probe itself; the tap ring buffer it reads
        // from is keyed by whatever entity that probe's input wire
        // resolves to (or "__master__"'s resolved id when unwired). Pushed
        // down by SignalLabPanel::applyResolvedScopeTapIds() whenever the
        // wiring is (re-)resolved.
        void setResolvedTapEntityId(const juce::String& entityId) { resolvedTapEntityId = entityId; }
        void paint(juce::Graphics& g) override;
        void resized() override;

        // Same shape as SignalLabPanel::onMidiLearnRequested -- wired by
        // SignalLabPanel to the same app-level Learn dialog every MIDI
        // Control node already uses, so a BCR2000-style knob controller can
        // bind straight to the scope's own front-panel knobs.
        std::function<void(const juce::String& displayLabel, bool wantsContinuousControl, std::function<void(juce::String, int, int, bool)> onLearned)> onLearnRequested;

        // Polled at ~30Hz while a playthrough is active and the scope isn't
        // Held: asks the live engine for a fresh snapshot of whatever this
        // scope's nodeId is tapping. Returns samples actually copied (0 if
        // nothing to show yet).
        std::function<int(const juce::String& nodeId, juce::AudioBuffer<float>& dest, int numSamplesRequested)> onLiveSnapshotRequested;
        std::function<bool()> onLiveIsActiveRequested;
        std::function<double()> onLiveSampleRateRequested;

        // Checks the change against all four knob bindings; applies and
        // returns true on the first match. Safe to call for every open
        // scope (inline or tool window) on every polled MIDI change --
        // a change that matches nothing here is just a no-op.
        bool tryApplyMidiChange(const MidiControlChange& change);

    private:
        void timerCallback() override;
        void exportCaptureToCsv();
        struct KnobBinding
        {
            bool learned = false;
            juce::String deviceId;
            int channel = 1;
            int number = 0;
            bool isController = true;
        };

        juce::AudioBuffer<float> displayBuffer;
        double sampleRate = 44100.0;
        juce::String nodeId;
        juce::String resolvedTapEntityId;
        bool isHeld = false;
        bool isLive = false; // true once the first live snapshot has landed; drives the LIVE/HELD/STATIC status text

        juce::TextButton holdButton { "Hold" };
        juce::TextButton exportButton { "Export CSV..." };
        juce::Label statusLabel;

        LearnableKnob timebaseSlider, startTimeSlider, levelZoomSlider, triggerSlider;
        juce::Label timebaseLabel, startTimeLabel, levelZoomLabel, triggerLabel;
        KnobBinding timebaseBinding, startTimeBinding, levelZoomBinding, triggerBinding;

        void configureControlSlider(LearnableKnob& slider, juce::Label& label, const juce::String& text, KnobBinding& binding, int knobIndex);
        void showLearnMenu(int knobIndex);
        // Knob 0=timebase, 1=start, 2=level, 3=trigger -- looked up fresh
        // by index rather than captured by reference, so async Learn
        // callbacks (which can outlive a while) never hold a stale
        // reference into a ScopePanel that's since been destroyed; pair
        // with a SafePointer<ScopePanel> null-check before calling this.
        juce::Label& knobLabel(int knobIndex);
        KnobBinding& knobBinding(int knobIndex);
        LearnableKnob& knobSlider(int knobIndex);
        double getTotalDurationMs() const;
        void refreshSliderRanges();
        juce::Rectangle<int> getPlotArea() const;
        // Rising-edge crossing of triggerSlider's value at or after
        // fromSample, searched on channel 0 -- returns -1 (no trigger,
        // caller free-runs from fromSample instead) if none found within
        // the search window.
        int findTriggerCrossing(int fromSample, int searchLimitSamples) const;
        // The window currently on screen -- Time/Div + Start + trigger
        // lock, resolved once and shared by paint() and CSV export so
        // "what you see is what you export" is actually true.
        int computeVisibleWindowStart(int& outWindowSamples) const;
    };

    class SpectrumPanel final : public juce::Component
    {
    public:
        void setBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate);
        void paint(juce::Graphics& g) override;

    private:
        juce::Array<float> magnitudes;
    };

    class AutomationLaneEditor final : public juce::Component
    {
    public:
        AutomationLaneEditor() = default;

        void setLane(const cw::PatchAutomationLane& newLane, juce::Colour accentColour);
        const cw::PatchAutomationLane& getLane() const noexcept { return lane; }
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseDoubleClick(const juce::MouseEvent& event) override;

        std::function<void()> onGestureBegin;
        std::function<void()> onGestureEnd;
        std::function<void(const juce::String& label)> onDiscreteEditRequested;
        std::function<void(const cw::PatchAutomationLane&)> onLaneChanged;

    private:
        juce::Rectangle<float> getPlotArea() const;
        juce::Point<float> getPoint(int index) const;
        int findPointAt(juce::Point<float> position) const;
        double pointValueFromY(float y) const;

        cw::PatchAutomationLane lane;
        juce::Colour laneAccent;
        int dragIndex = -1;
    };

    class NodeGraphCanvas final : public juce::Component
    {
    public:
        explicit NodeGraphCanvas(SignalLabPanel& ownerRef) : owner(ownerRef) {}

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseDoubleClick(const juce::MouseEvent& event) override;
        void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
        bool keyPressed(const juce::KeyPress& key) override;

    private:
        SignalLabPanel& owner;
        int dragNodeIndex = -1;
        juce::Point<int> dragOffset;
        bool dragMoved = false;
        bool panning = false;
        juce::Point<int> panAnchor;
        juce::Point<int> viewportAnchor;

        bool wireDragging = false;
        int wireDragNodeIndex = -1;
        GraphPort wireDragPort;
        juce::Point<int> wireDragCurrentPoint;

        int waypointDragConnectionIndex = -1;
        int waypointDragIndex = -1;
    };

    class NodeToolboxPane final : public juce::Component
    {
    public:
        class VariableButton final : public juce::TextButton
        {
        public:
            explicit VariableButton(const juce::String& variableIdToUse) : variableId(variableIdToUse) {}

            void mouseDown(const juce::MouseEvent& event) override;
            void mouseDrag(const juce::MouseEvent& event) override;
            void mouseUp(const juce::MouseEvent& event) override;

            std::function<void(const juce::String& variableId, juce::Point<int> screenPoint)> onDragStarted;
            std::function<void(const juce::String& variableId, juce::Point<int> screenPoint)> onDragMoved;
            std::function<void(const juce::String& variableId, juce::Point<int> screenPoint)> onDragEnded;

            juce::String variableId;

        private:
            bool dragStarted = false;
            juce::Point<int> mouseDownScreenPosition;
        };

        NodeToolboxPane();
        void resized() override;
        void paint(juce::Graphics& g) override;

        void setVariables(const juce::Array<LocalControlVariable>& variables);
        int getRequiredHeight() const;

        std::function<void()> onAddVariableRequested;
        std::function<void(const juce::String& variableId)> onPlaceVariableRequested;
        std::function<void(const juce::String& variableId, juce::Point<int> screenPoint)> onVariableDragStarted;
        std::function<void(const juce::String& variableId, juce::Point<int> screenPoint)> onVariableDragMoved;
        std::function<void(const juce::String& variableId, juce::Point<int> screenPoint)> onVariableDragEnded;
        std::function<void(const juce::String& variableId)> onVariableSelected;
        std::function<void(const juce::String& variableId)> onVariableRemoveRequested;

    private:
        juce::Label titleLabel;
        juce::TextButton addVariableButton { "+ Variable" };
        juce::OwnedArray<VariableButton> variableButtons;
        juce::OwnedArray<juce::TextButton> removeButtons;
        juce::Array<LocalControlVariable> localVariables;
    };

    class NodeSearchPanel final : public juce::Component
    {
    public:
        struct Entry
        {
            juce::String label;
            juce::String type;
            juce::String payload;
        };

        NodeSearchPanel();
        void setEntries(juce::Array<Entry> entries);
        void resized() override;
        void paint(juce::Graphics& g) override;

        std::function<void(const juce::String& type, const juce::String& payload)> onEntryChosen;
        std::function<void()> onDismissRequested;

    private:
        void refreshResults();

        struct CompactLookAndFeel final : public juce::LookAndFeel_V4
        {
            juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::Font(11.0f); }
        };

        CompactLookAndFeel compactLookAndFeel;
        juce::TextEditor searchEditor;
        juce::OwnedArray<juce::TextButton> resultButtons;
        juce::Array<Entry> allEntries;
        juce::Array<Entry> visibleEntries;
    };

    class FloatingWindow final : public juce::Component
    {
    public:
        enum class Kind
        {
            NodeEditor,
            ControlPad
        };

        FloatingWindow(SignalLabPanel& ownerRef, Kind kindToUse);

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;

    private:
        SignalLabPanel& owner;
        Kind kind;
        juce::Point<int> dragAnchor;
        juce::Rectangle<int> startBounds;
    };

    class SectionPanel final : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;
    };

    void configureSlider(juce::Slider& slider, double min, double max, double step);
    void regenerateSignal();
    void ensureAudioRendered();
    const juce::AudioBuffer<float>& getDisplayBufferForNode(const juce::String& nodeId) const;
    juce::AudioBuffer<float> buildSignalBuffer(const SignalRecipe& recipe) const;
    cw::PatchDocument buildPatchDocument(const SignalRecipe& recipe) const;
    void applyTemplate(const juce::String& templateName);
    void showSignalMenu();
    void createNewSignal();
    void refreshControlsFromRecipe();
    void updateStatusText();
    void syncAutomationEditors();
    void rebuildAutomationChrome();
    void captureUndoCheckpoint(const juce::String& label);
    void beginUndoGesture(const juce::String& label);
    void endUndoGesture();
    void noteInteraction();
    void rebuildNodeGraphFromRecipe();
    void seedOscillatorNodesFromRecipeLevels();
    bool findWiredParameterValue(const juce::String& nodeId, const juce::String& portId, double& outValue) const;
    juce::String findWiredMidiSourceNodeId(const juce::String& nodeId, const juce::String& portId) const;
    // True only when the port is wired to a Fader Control node specifically
    // (not a Button Control, not a Get-variable) -- these are the only
    // wired sources where the on-screen slider should stay live rather than
    // being disabled, since the physical fader and the on-screen slider are
    // meant to be two views of the same continuously-adjustable value.
    bool isPortFaderDriven(const juce::String& nodeId, const juce::String& portId) const;
    void notifyFaderChannelClaims() const;
    // Writes a manual on-screen-slider drag straight through to the driving
    // Fader Control node's live value (and echoes it to the node's bound
    // physical fader, if any) instead of a now-ignored local field -- see
    // normalizedFromRealValue() in the .cpp.
    void pushLiveMidiFaderValue(const juce::String& midiNodeId, float rawNormalizedValue);
    // If portId is currently wired to a Fader Control node, converts
    // realValue back to raw 0..1 and pushes it through that node (and out
    // to its physical fader) instead of the caller writing its own local
    // manual field. Returns false (does nothing) when the port isn't
    // Fader-Control-driven, so the caller falls back to its normal manual
    // write.
    bool tryPushFaderDrivenValue(const juce::String& nodeId, const juce::String& portId, const juce::String& parameterId, double realValue);
    PortValueDisplay describePortValue(int nodeIndex, const GraphPort& port) const;
    PatchLiveBindingMap buildLiveBindingMap(const cw::PatchDocument& patch) const;
    // Which entity node ids currently need a live scope tap -- one per
    // currently-open Scope (the inline one, if selected, plus every
    // tool-window instance), resolved from patch.connections the same way
    // PatchRuntimePlayer's offline resolveSingleInput does (an unwired
    // Scope resolves to the sentinel "__master__", which PatchLiveVoice
    // maps to whatever it resolves as the final/output entity).
    juce::Array<juce::String> resolveScopeTapNodeIds(const cw::PatchDocument& patch) const;
    // Re-resolves and pushes the current tap set without a full graph
    // rebuild -- for when a Scope tool window opens/closes, or the inline
    // scope's selection changes, while the DSP topology itself hasn't.
    void refreshLiveScopeTaps();
    // Pushes each open scope's resolved entity id (see
    // ScopePanel::setResolvedTapEntityId) so it asks the live engine for
    // the right tap -- a scope's own node id and the entity id it probes
    // are different things, and only the latter is what tap slots are
    // keyed by. Must be called (via refreshLiveScopeTaps() or directly
    // from Play, both already do) any time tapNodeIds is recomputed, or
    // the two stay out of sync.
    void applyResolvedScopeTapIds(const cw::PatchDocument& patch);
    void updateInspectorForSelection();
    void layoutFloatingWindows();
    void showCanvasActionMenu(juce::Point<int> canvasPosition, bool anchorToButton = false);
    void showNodeContextMenu(int nodeIndex, juce::Point<int> canvasPosition);
    void addGraphNode(const juce::String& type, juce::Point<int> canvasPosition = {}, const juce::String& payload = {});
    void removeSelectedGraphNode();
    int findGraphNodeAt(juce::Point<int> position) const;
    juce::Rectangle<int> getGraphNodeBounds(int index) const;
    int getGraphNodeHeight(int index) const;
    juce::Rectangle<int> getMixerAddInputButtonBounds(int index) const;
    void setSelectedGraphNodeIndex(int index);
    bool hasGraphNodeType(const juce::String& type) const;
    bool hasSetterNodeForVariable(const juce::String& variableId) const;
    void addMixerInput(int nodeIndex);
    void openNodeEditorForSelection();
    void closeNodeEditor();
    void toggleControlPad();
    void ensureDefaultLocalControls();
    void rebuildLocalControlChrome();
    void applyLocalControlToRecipe(const LocalControlVariable& control);
    void refreshVariablePanel();
    void refreshSelectedVariableEditor();
    void timerCallback() override;
    void triggerTransportPlay();
    void stopTransport();
    juce::Array<GraphValidationError> validateGraph() const;
    void compileGraph();
    void showCompileErrorWindow();
    void centerCanvasOnGraphNode(const juce::String& nodeId);
    void updateCanvasWorkspace();
    juce::Point<int> graphToCanvas(juce::Point<int> position) const;
    juce::Point<float> graphToCanvas(juce::Point<float> position) const;
    juce::Rectangle<int> graphToCanvas(juce::Rectangle<int> bounds) const;
    juce::Point<int> canvasToGraph(juce::Point<int> position) const;
    juce::Point<float> canvasToGraph(juce::Point<float> position) const;
    juce::Point<float> getControlPadOutputPort(int index) const;
    juce::Array<GraphPort> getNodePorts(int nodeIndex) const;
    PortHit findPortAt(juce::Point<int> canvasPosition) const;
    juce::Point<int> resolvePortPosition(const juce::String& nodeId, const juce::String& portId, bool wantOutput) const;
    int findConnectionAt(juce::Point<int> canvasPosition, int* outWaypointIndex) const;
    juce::Path buildConnectionPath(int connectionIndex) const;
    void tryCompleteConnection(int fromNodeIndex, const GraphPort& fromPort, juce::Point<int> releaseCanvasPosition);
    void removeConnection(int index);
    void showConnectionContextMenu(int connectionIndex, int waypointIndex, juce::Point<int> canvasPosition);
    static juce::Colour portValueColour(PortValueType type);

    SignalRecipe recipe;
    ProbeSettings probeSettings;
    juce::Array<GraphNodeModel> graphNodes;
    juce::Array<GraphConnection> graphConnections;
    int selectedConnectionIndex = -1;
    juce::Array<GraphValidationError> validationErrors;
    juce::Array<LocalControlVariable> localControls;
    juce::AudioBuffer<float> generatedBuffer;
    juce::Array<PatchRuntimePlayer::TapCapture> nodeTapBuffers; // per Scope/Analyzer node id, its real tapped signal
    bool audioDirty = true; // set by regenerateSignal() on every graph/recipe edit; only ensureAudioRendered() clears it, and only actual playback/render/export call that -- rendering happens on demand, not on every edit
    // Separate from audioDirty on purpose: audioDirty exclusively governs the offline
    // Preview/Render-to-Project path (ensureAudioRendered()) and must not be touched by the live
    // engine, or Render-to-Project would silently use a stale buffer after a live-only Play.
    // liveGraphDirty governs PatchLiveVoice's topology instead -- set alongside audioDirty by
    // regenerateSignal(), cleared only by triggerTransportPlay() after a live rebuild succeeds. A
    // MIDI Control node's value changing does NOT set this -- see applyLiveMidiControlChanges().
    bool liveGraphDirty = true;
    bool variableDragActive = false;
    juce::String draggedVariableId;
    juce::Point<int> draggedVariableScreenPoint;
    float canvasZoom = 1.0f;
    juce::Point<int> canvasWorkspaceSize { 2600, 1800 };
    juce::Point<int> graphOrigin;
    juce::Point<int> canvasPixelOffset;
    bool mixNodeEnabled = false;
    bool filterNodeEnabled = false;
    bool envelopeNodeEnabled = false;
    bool graphViewportInitialized = false;
    bool repeatEnabled = false;
    double repeatDelaySeconds = 0.0;
    bool suppressCallbacks = false;
    bool undoGestureActive = false;
    bool nameEditUndoCaptured = false;
    bool descriptionEditUndoCaptured = false;
    PatchRuntimePlayer runtimePlayer;
    int selectedGraphNodeIndex = -1;
    int editingNodeIndex = -1;
    bool nodeEditorVisible = false;
    bool controlPadVisible = false;
    juce::Rectangle<int> nodeEditorBounds { 760, 120, 360, 520 };
    juce::Rectangle<int> controlPadBounds { 72, 420, 340, 240 };
    struct OpenNodeWindow
    {
        juce::String nodeId;
        std::unique_ptr<juce::DocumentWindow> window;
    };
    juce::OwnedArray<OpenNodeWindow> openNodeWindows;
    std::unique_ptr<juce::DocumentWindow> compileErrorWindow;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;
    juce::TextButton signalMenuButton { "Signal" };
    juce::TextButton compileButton { "Compile" };
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton repeatButton { "Repeat" };
    juce::Slider repeatDelaySlider; // seconds between repeats -- 0 = back to back
    juce::Label propertiesHeaderLabel;
    juce::Label signalSectionLabel;
    juce::Label variablesSectionLabel;
    juce::Label selectedVariableSectionLabel;
    juce::Label signalMetaLabel;
    SectionPanel signalPropertiesPanel;
    SectionPanel variablesPanel;
    SectionPanel variableDetailsPanel;
    NodeToolboxPane toolboxPane;
    juce::Viewport variablesViewport;
    juce::Component variableDetailsContent;
    juce::Viewport variableDetailsViewport;
    juce::Viewport graphViewport;
    NodeGraphCanvas nodeGraphCanvas { *this };
    FloatingWindow nodeEditorWindow { *this, FloatingWindow::Kind::NodeEditor };
    FloatingWindow controlPadWindow { *this, FloatingWindow::Kind::ControlPad };
    juce::TextButton nodeEditorCloseButton { "×" };
    juce::TextButton controlPadCloseButton { "×" };
    juce::TextButton addLocalControlButton { "+ Variable" };
    juce::Label inspectorTitleLabel;
    juce::Label inspectorBodyLabel;
    juce::Label nameLabel;
    juce::TextEditor nameEditor;
    juce::Label descriptionLabel;
    juce::TextEditor descriptionEditor;
    juce::Label variableNameLabel;
    juce::TextEditor variableNameEditor;
    juce::Label variableDescriptionLabel;
    juce::TextEditor variableDescriptionEditor;
    juce::Label variableTypeLabel;
    juce::ComboBox variableTypeSelector;
    juce::Label variableAccessLabel;
    juce::ComboBox variableAccessSelector;
    juce::Label variableValueLabel;
    juce::TextEditor variableValueEditor;
    juce::ToggleButton variableAutomationToggle { "Expose to automation" };
    juce::Label templateLabel;
    juce::ComboBox templateSelector;
    juce::Label frequencyLabel;
    juce::Slider frequencySlider;
    juce::Label durationLabel;
    juce::Slider durationSlider;
    juce::Label pitchLabel;
    juce::Slider pitchSlider;
    juce::Label filterModeLabel;
    juce::ComboBox filterModeSelector;
    juce::Label filterCutoffLabel;
    juce::Slider filterCutoffSlider;
    juce::Label filterResonanceLabel;
    juce::Slider filterResonanceSlider;
    juce::Label filterEnvelopeLabel;
    juce::Slider filterEnvelopeSlider;
    juce::Label envelopeCurveLabel;
    juce::ComboBox envelopeCurveSelector;
    juce::Label automationCurveLabel;
    juce::ComboBox automationCurveSelector;
    juce::Label macroHardnessLabel;
    juce::Slider macroHardnessSlider;
    juce::Label macroWeightLabel;
    juce::Slider macroWeightSlider;
    juce::Label macroAirLabel;
    juce::Slider macroAirSlider;
    juce::Label macroGritLabel;
    juce::Slider macroGritSlider;
    juce::Label macroSizeLabel;
    juce::Slider macroSizeSlider;
    juce::Label sineLabel;
    juce::Slider sineSlider;
    juce::Label sawLabel;
    juce::Slider sawSlider;
    juce::Label squareLabel;
    juce::Slider squareSlider;
    juce::Label triangleLabel;
    juce::Slider triangleSlider;
    juce::Label noiseLabel;
    juce::Slider noiseSlider;
    juce::Label probeControlALabel;
    juce::Slider probeControlASlider;
    juce::Label probeControlBLabel;
    juce::Slider probeControlBSlider;
    juce::Label probeControlCLabel;
    juce::Slider probeControlCSlider;
    juce::Label probeControlDLabel;
    juce::Slider probeControlDSlider;
    juce::TextButton previewButton { "Preview Signal" };
    juce::TextButton renderButton { "Render To Project" };
    juce::TextButton exportPatchButton { "Export File" };
    juce::TextButton savePatchButton { "Save Sound" };
    juce::TextButton loadPatchButton { "Load Sound" };
    juce::TextButton addAutomationLaneButton { "Add Motion Lane" };
    EnvelopeEditor envelopeEditor;
    juce::Component automationHost;
    juce::Viewport automationViewport;
    juce::OwnedArray<AutomationLaneEditor> automationLaneEditors;
    juce::OwnedArray<juce::ComboBox> automationTargetSelectors;
    juce::OwnedArray<juce::ComboBox> automationCurveSelectors;
    juce::OwnedArray<juce::TextButton> removeAutomationLaneButtons;
    juce::OwnedArray<juce::TextEditor> localControlNameEditors;
    juce::OwnedArray<juce::ComboBox> localControlTargetSelectors;
    juce::OwnedArray<juce::Slider> localControlValueSliders;
    juce::OwnedArray<juce::TextButton> removeLocalControlButtons;
    ScopePanel scopePanel;
    // Tool-window Scope instances are created fresh (new ScopePanel()) each
    // time a scope node's window is opened and owned by that window's
    // content -- tracked here (SafePointer, so a closed window just drops
    // out) purely so applyLiveMidiControlChanges()/resolveScopeTapNodeIds()
    // can also reach their knob bindings and tap requests, not just the
    // inline scopePanel above. nodeId is which graph node this instance
    // represents (set once at creation, never changes for a tool-window
    // scope -- unlike the inline scopePanel, which is a single shared
    // instance whose represented node changes with selection).
    struct LiveScopeEntry
    {
        juce::String nodeId;
        juce::Component::SafePointer<ScopePanel> panel;
    };
    juce::Array<LiveScopeEntry> liveScopePanels;
    SpectrumPanel spectrumPanel;
    int selectedLocalControlIndex = -1;
};
