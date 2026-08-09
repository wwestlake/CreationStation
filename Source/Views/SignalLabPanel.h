#pragma once

#include <JuceHeader.h>
#include "../Audio/PatchRuntimePlayer.h"
#include "../Patch/PatchModel.h"
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

    juce::ValueTree createState() const;
    void restoreState(const juce::ValueTree& state);
    void resetToBlankSignal();
    bool loadPatchDocument(const cw::PatchDocument& document, juce::String& errorMessage);
    void applyAiTemplate(const juce::String& templateName);
    bool previewCurrentSignal();

    std::function<void(const juce::ValueTree& stateBeforeEdit, const juce::String& label)> onUndoCheckpointRequested;
    std::function<void()> onInteractionStarted;
    std::function<void(const juce::AudioBuffer<float>&, double sampleRate, const juce::String& suggestedName)> onPreviewRequested;
    std::function<void(const juce::AudioBuffer<float>&, double sampleRate, const juce::String& suggestedName)> onRenderRequested;
    std::function<void(const juce::String& patchJson, const juce::String& suggestedName)> onPatchExportRequested;
    std::function<void(const juce::String& patchJson, const juce::String& suggestedName)> onPatchSaveToLibraryRequested;
    std::function<void()> onPatchLoadRequested;
    std::function<void()> onStopRequested;
    std::function<void()> onAudioSettingsRequested;

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
        double scopeTimebaseMs = 20.0;
        double scopeGainA = 1.0;
        double scopeGainB = 1.0;
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

    class ScopePanel final : public juce::Component
    {
    public:
        void setBuffer(const juce::AudioBuffer<float>& buffer);
        void paint(juce::Graphics& g) override;

    private:
        juce::AudioBuffer<float> displayBuffer;
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
    SpectrumPanel spectrumPanel;
    int selectedLocalControlIndex = -1;
};
