#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "AI/CreationStationContextEngine.h"
#include "AI/CreationStationAppManifest.h"
#include "AI/CreationStationContextStore.h"
#include "AI/CreationStationTaskPlanner.h"
#include <creation/services/SuiteAiChatClient.h>
#include <creation/services/SuiteUndoService.h>
#include "AI/LiteSemRagApiClient.h"
#include "AI/OpenAiModelCatalogClient.h"
#include <creation/ui/SuiteDesktopAuthSession.h>
#include "Audio/StudioIOModel.h"
#include "Audio/VstPluginCatalog.h"
#include "Audio/WorkstationAudioEngine.h"
#include "ControlSurface/XTouchControlSurface.h"
#include "ControlSurface/ControlSurfaceMappingStore.h"
#include "Content/ContentLibrary.h"
#include "Content/ContentApiClient.h"
#include <creation/assets/ProjectSession.h>
#include <creation/ui/CreationSuiteHeaderBar.h>
#include <creation/ui/SuiteShellController.h>
#include "Suite/SuiteSettings.h"
#include "Tutorial/GuidedTutorial.h"
#include "Timeline/TimelineModel.h"
#include "Views/AuthGateView.h"
#include "Views/AiPanel.h"
#include "Views/ArrangeView.h"
#include "Views/ContentPanel.h"
#include "Views/DslPanel.h"
#include "Views/GraphPanel.h"
#include "Views/MidiEditorPanel.h"
#include "Views/MixerPanel.h"
#include "Views/PluginBrowserList.h"
#include "Views/PluginsPanel.h"
#include "Views/SamplePackBuilderPanel.h"
#include "Views/RecordView.h"
#include "Views/SettingsPanel.h"
#include "Views/ScorePanel.h"
#include "Views/SignalLabPanel.h"
#include "Views/TrackerPanel.h"
#include "Views/TourGuideOverlay.h"
#include <creation/ui/SuiteSettingsPanel.h>

class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private juce::KeyListener
{
public:
    enum class WorkspaceMode
    {
        tracker,
        arrange,
        signal,
        library,
        mix,
        plugins,
        node,
        code,
        record,
        score,
        settings,
        // Added after settings (not inserted earlier in the list) so every already-persisted
        // layout/session's saved integer mode value keeps meaning what it always meant - the
        // button itself is still positioned right after Tracker in ViewModeBar, since that's a
        // purely visual layout choice independent of this enum's declaration order.
        sampler
    };

    using StartupProgressCallback = std::function<void(const juce::String& statusText, float progress)>;

    MainComponent();
    explicit MainComponent(StartupProgressCallback startupProgressCallback);
    ~MainComponent() override;

    void confirmCloseApplication(const std::function<void(bool shouldClose)>& onDecision);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ViewModeBar final : public juce::Component
    {
    public:
        ViewModeBar();

        std::function<void(WorkspaceMode)> onModeSelected;
        std::function<void()> onPopOutRequested;

        void setActiveMode(WorkspaceMode newMode);
        void resized() override;
        void paint(juce::Graphics&) override;

    private:
        WorkspaceMode activeMode = WorkspaceMode::tracker;
        juce::Label titleLabel;
        juce::TextButton trackerButton { "Tracker" };
        juce::TextButton samplerButton { "Sampler" };
        juce::TextButton arrangeButton { "Foley" };
        juce::TextButton signalButton { "Signal" };
        juce::TextButton mixButton { "Layers" };
        juce::TextButton pluginsButton { "Plugins" };
        juce::TextButton nodeButton { "Patch" };
        juce::TextButton codeButton { "Script" };
        juce::TextButton recordButton { "Capture" };
        juce::TextButton scoreButton { "Score" };
        juce::TextButton settingsButton { "Settings" };
        juce::TextButton popOutButton { "Pop Out" };
    };

    class PluginRackBar final : public juce::Component
    {
    public:
        enum class Context
        {
            master,
            track
        };

        PluginRackBar();

        std::function<void()> onLoadPlugin;
        std::function<void()> onManagePluginPaths;
        std::function<void()> onUnloadPlugin;
        std::function<void()> onOpenPluginEditor;
        std::function<void()> onOpenFxStack;
        std::function<void(bool)> onBypassChanged;

        void setContextMaster();
        void setContextTrack(int trackIndex, const juce::String& trackName);
        bool isTrackContext() const noexcept { return context == Context::track; }
        int getTrackIndex() const noexcept { return selectedTrackIndex; }

        void setPluginName(const juce::String& name);
        void setCatalogSummary(const juce::String& summary);
        void setBypassed(bool shouldBypass);
        void setHasPlugin(bool hasPlugin);

        void resized() override;
        void paint(juce::Graphics&) override;

        juce::Label titleLabel;
        juce::Label contextLabel;
        juce::Label pluginNameLabel;
        juce::Label catalogLabel;
        juce::ToggleButton bypassButton { "Bypass" };
        juce::TextButton pathsButton { "VST Paths" };
        juce::TextButton openEditorButton { "Open UI" };
        juce::TextButton fxStackButton { "FX Stack" };
        juce::TextButton loadButton { "Load VST3" };
        juce::TextButton unloadButton { "Unload" };

    private:
        Context context = Context::master;
        int selectedTrackIndex = -1;
        bool hasPlugin = false;
    };

    class FxStackPanel final : public juce::Component,
                               private juce::ListBoxModel
    {
    public:
        FxStackPanel();

        std::function<void(const VstPluginCatalog::Entry&)> onAddPlugin;
        std::function<void(int, const VstPluginCatalog::Entry&)> onInsertPlugin;
        std::function<void(int)> onRemovePlugin;
        std::function<void(int, int)> onMovePlugin;
        std::function<void(int)> onOpenPluginEditor;
        std::function<void(int, bool)> onBypassChanged;
        std::function<void()> onRescanRequested;

        void setTrackName(const juce::String& name);
        void setPlugins(const juce::StringArray& names, const juce::Array<bool>& bypassStates);
        void setCatalog(const juce::Array<VstPluginCatalog::Entry>& entries);
        int getSelectedSlot() const noexcept;

        void resized() override;
        void paint(juce::Graphics&) override;

    private:
        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void selectedRowsChanged(int lastRowSelected) override;
        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

        void refreshButtonState();
        void addSelectedCatalogEntry();
        void insertSelectedCatalogEntry();

        juce::Label titleLabel;
        juce::Label trackLabel;
        juce::ListBox pluginList { "Track FX Stack", this };
        juce::TextButton removeButton { "Remove" };
        juce::TextButton upButton { "Up" };
        juce::TextButton downButton { "Down" };
        juce::TextButton bypassButton { "Bypass" };
        juce::TextButton openButton { "Open UI" };

        juce::Label catalogLabel;
        juce::TextEditor searchBox;
        PluginBrowserList catalogBrowser;
        juce::TextButton addButton { "+ Add" };
        juce::TextButton insertButton { "Insert" };
        juce::TextButton rescanButton { "Rescan" };

        juce::StringArray pluginNames;
        juce::Array<bool> pluginBypassStates;
    };

    juce::AudioDeviceManager deviceManager;
    creation::ui::SuiteDesktopAuthSession authSession { "creative-workstation" };
    creation::ui::SuiteShellController suiteShellController;
    CreationStationContextEngine contextEngine;
    CreationStationContextStore contextStore;
    CreationStationTaskPlanner taskPlanner;
    CreationStationAppManifest appManifest;
    LiteSemRagApiClient semanticApiClient;
    creation::services::SuiteAiChatClient openAiChatClient;
    OpenAiModelCatalogClient modelCatalogClient;
    WorkstationAudioEngine engine;
    XTouchControlSurface midiSurface;
    AuthGateView authGateView;
    CreationSuiteHeaderBar transportBar;
    ViewModeBar viewModeBar;
    PluginRackBar pluginRackBar;
    TrackerPanel trackerPanel;
    SamplePackBuilderPanel samplePackBuilderPanel;
    ArrangeView arrangeView;
    SignalLabPanel signalLabPanel;
    ContentPanel contentPanel;
    MixerPanel mixerPanel;
    PluginsPanel pluginsPanel;
    GraphPanel graphPanel;
    DslPanel dslPanel;
    RecordView recordView;
    ScorePanel scorePanel;
    AiPanel aiPanel;
    SettingsPanel settingsPanel;
    SuiteSettingsStore suiteSettingsStore;
    SuiteSettings suiteSettings;
    TourGuideOverlay tourOverlay;
    // Component::setTooltip() only stores the string - nothing ever displays it without a live
    // TooltipWindow somewhere in the app. None existed, which is why none of the tooltips added
    // throughout the UI have ever shown. Parent nullptr so it works across popped-out workspace
    // windows too, not just the main window.
    juce::TooltipWindow tooltipWindow { nullptr, 500 };
    std::unique_ptr<juce::FileChooser> pluginChooser;
    struct PluginEditorWindowEntry
    {
        juce::String key;
        int trackIndex = -1;
        std::unique_ptr<juce::DocumentWindow> window;
    };
    std::vector<PluginEditorWindowEntry> pluginEditorWindows;
    std::unique_ptr<juce::DocumentWindow> fxStackWindow;
    juce::Component::SafePointer<FxStackPanel> fxStackPanel;
    std::unique_ptr<juce::DocumentWindow> suiteSettingsWindow;
    juce::Component::SafePointer<SuiteSettingsPanel> suiteSettingsPanel;
    std::unique_ptr<juce::DocumentWindow> midiEditorWindow;
    juce::Component::SafePointer<MidiEditorPanel> midiEditorPanel;
    std::array<std::unique_ptr<juce::DocumentWindow>, 12> workspacePopoutWindows;
    juce::Label poppedWorkspacePlaceholder;
    juce::Component::SafePointer<CreationSuiteHeaderBar> transportBarSafe;
    juce::Component::SafePointer<PluginRackBar> pluginRackBarSafe;
    juce::Component::SafePointer<MixerPanel> mixerPanelSafe;
    creation::assets::ProjectSession projectSession;
    VstPluginCatalog vstPluginCatalog;
    ContentLibrary contentLibrary;
    ContentApiClient contentApiClient;
    std::unique_ptr<juce::FileChooser> storageRootChooser;
    std::unique_ptr<juce::FileChooser> projectChooser;
    std::unique_ptr<juce::FileChooser> assetChooser;
    std::unique_ptr<juce::FileChooser> renderExportChooser;
    std::unique_ptr<juce::FileChooser> rawAssetExportChooser;
    std::unique_ptr<juce::FileChooser> patchChooser;
    std::unique_ptr<juce::FileChooser> celSourceChooser;
    std::unique_ptr<juce::FileChooser> contentUploadChooser;
    std::unique_ptr<juce::FileChooser> suiteDirectoryChooser;
    std::unique_ptr<juce::DocumentWindow> audioDeviceWindow;
    std::unique_ptr<juce::DocumentWindow> midiLearnWindow;
    ControlSurfaceMappingStore controlSurfaceMappings;
    std::vector<bool> armedTracks;
    std::vector<bool> monitoredTracks;
    // Wall-clock timestamp of the last manual write into each track's automation lane (Touch
    // mode's idle-release timer). Parallel to armedTracks, resized alongside it.
    std::vector<double> automationLastManualWriteWallSeconds;
    bool authenticated = false;
    WorkspaceMode activeMode = WorkspaceMode::tracker;
    AiProviderSettings aiProviderSettings;
    bool autoloadLastProject = false;
    bool aiSidebarCollapsed = false;
    bool layoutDirty = false;
    double layoutLastChangeWallSeconds = 0.0;
    bool midiPlaybackRefreshPending = false;
    double midiPlaybackRefreshLastChangeWallSeconds = 0.0;
    bool pluginStateAutosavePending = false;
    double pluginStateLastChangeWallSeconds = 0.0;
    double pluginStateLastPollWallSeconds = 0.0;
    juce::String lastObservedPluginStateSignature;
    bool appContextSyncInProgress = false;
    juce::String appContextLastPublishedChecksum;
    juce::String pendingAiPrompt;
    CreationStationContextEngine::ContextPacket pendingAiContextPacket;
    bool pendingAiContextPacketValid = false;
    bool aiCompletionInFlight = false;
    bool projectDirty = false;
    cs::StudioIOModel studioIOModel;
    juce::String selectedStudioAudioSystem;
    juce::String selectedStudioInputDevice;
    juce::String selectedStudioOutputDevice;
    juce::StringArray disabledMidiInputDeviceIds;
    cs::TimelineModel timelineModel;
    double transportStartWallSeconds = 0.0;
    double transportStartTimelineSeconds = 0.0;
    bool midiScrubModeEnabled = false;
    bool midiEditorPreviewPlaying = false;
    bool midiEditorPreviewLoopEnabled = false;
    double midiEditorPreviewStartWallSeconds = 0.0;
    double midiEditorPreviewStartLocalSeconds = 0.0;
    double midiEditorPreviewLastLocalSeconds = 0.0;
    double midiEditorPreviewEndLocalSeconds = 0.0;
    double midiEditorPreviewLoopStartSeconds = 0.0;
    double midiEditorPreviewLoopEndSeconds = 0.0;
    juce::StringArray midiEditorPreviewActiveNoteIds;
    int activeRecordingTrack = -1;
    int selectedClipIndex = -1;
    bool clipDragUndoCaptured = false;
    creation::services::SuiteUndoService undoService;
    static constexpr const char* timelineUndoContextId = "creation-station.timeline";
    static constexpr const char* signalUndoContextId = "creation-station.signal-lab";

    void timerCallback() override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void parentHierarchyChanged() override;
    bool handleGlobalKeyPress(const juce::KeyPress& key);
    juce::ValueTree createProjectStateForSave();
    void remapTemplateStateFilesToCurrentProject(juce::ValueTree& state) const;
    void saveSessionToDisk(bool userInitiated = false);
    void loadSessionFromDisk();
    void pollHostedPluginStateAutosave();
    bool prepareTrackerPlayback();
    void refreshTrackerPlaybackClips();
    bool buildTrackerPlaybackTargets(juce::Array<WorkstationAudioEngine::PlaybackClipTarget>& targets,
                                     double& durationSeconds,
                                     juce::String& errorMessage) const;
    void previewScrubAudioAt(double timelineSeconds);
    void refreshMidiPlaybackClips();
    bool renderFullMixToProject();
    void exportFullMixAsWav();
    void pushTimelineUndoState();
    void pushTimelineUndoState(const juce::ValueTree& stateBeforeEdit);
    void undoTimelineEdit();
    void redoTimelineEdit();
    void restoreTimelineEditState(const juce::ValueTree& state, const juce::String& statusText);
    void pushSignalUndoState(const juce::ValueTree& stateBeforeEdit, const juce::String& label);
    void undoSignalEdit();
    void redoSignalEdit();
    void restoreSignalEditState(const juce::ValueTree& state, const juce::String& statusText);
    void splitClipAt(int clipIndex, double splitSeconds);
    void duplicateClip(int clipIndex);
    void deleteClip(int clipIndex);
    void renameClip(int clipIndex);
    juce::File getAppSettingsFile() const;
    void saveAppSettings();
    void loadAppSettings();
    void applySelectedAudioDeviceSettings();
    void refreshMidiDeviceSettings();
    void refreshInsertRack();
    void syncTrackViews();
    void refreshTrackInputSources();
    // Pushes an automation track's current target + curve to the engine's lock-free snapshot,
    // so the audio thread's per-block automation pass sees it. Call after any edit to that
    // track's automation points or target (point add/move/delete, target reassignment, load).
    void pushAutomationDataToEngine(int trackIndex);
    // Builds and shows the "what should this automation lane control?" popup: every other
    // track's Volume/Pan plus a submenu per loaded plugin insert enumerating its parameters.
    void showAutomationTargetPicker(int trackIndex);
    // Builds and shows the "which folder should this track live in?" popup: every current
    // Folder-kind track plus "None (top-level)". Rejects and reports a status message instead of
    // silently no-op'ing if the model rejects the assignment (e.g. a cycle).
    void showMoveToFolderPicker(int trackIndex);
    // Shared by drag-reorder and the folder-assignment auto-group: moves trackIndex (and its
    // whole block if it's a folder) to destinationIndex in both TimelineModel and the engine,
    // keeping their track arrays in index-parity. Returns false if the model rejected the move.
    bool performTrackMove(int trackIndex, int destinationIndex);
    // Derives a stable-across-reorders accent colour for a track's header badge: a folder gets
    // its own colour (hashed from its id, so reordering tracks doesn't reshuffle folder colours),
    // and a track routed into a folder inherits that folder's colour. Everything else gets none
    // (transparent - the header falls back to its default neutral badge).
    juce::Colour computeTrackAccentColour(int trackIndex) const;
    // "Riding the fader": while playing, if an armed automation track targets this exact
    // track+control, a manual change (header slider, mixer fader/pan, X-Touch fader/pan) is
    // written into that lane's curve at the current transport position instead of being lost.
    // normalizedValue is 0..1, matching AutomationPoint's convention. Call this from every
    // manual (user-driven, not automation-driven) gain/pan change call site.
    void recordAutomationWriteIfArmed(int targetTrackIndex, cs::AutomationTargetKind kind, float normalizedValue);
    // Runs every timer tick: drives Write mode's continuous sampling, and Touch mode's
    // auto-release after a short idle period; resets all write-active flags when not playing.
    void updateAutomationRecordModes();
    void refreshAudioDeviceSettingsView();
    void setAudioSystem(const juce::String& audioSystem);
    void setAudioInputDevice(const juce::String& inputDeviceName);
    void setAudioOutputDevice(const juce::String& outputDeviceName);
    void addTrack();
    void removeTrack(int trackIndex);
    void performTrackRemoval(int trackIndex);
    void setWorkspaceMode(WorkspaceMode mode);
    void refreshModeVisibility();
    juce::Component* getWorkspaceComponent(WorkspaceMode mode);
    bool isWorkspacePoppedOut(WorkspaceMode mode) const;
    void popOutActiveWorkspace();
    void popOutWorkspace(WorkspaceMode mode, const juce::Rectangle<int>* bounds = nullptr);
    void dockWorkspace(WorkspaceMode mode);
    void markLayoutDirty();
    void saveLayoutToDisk(bool userInitiated = false);
    void loadLayoutFromDisk();
    juce::ValueTree createLayoutState() const;
    void restoreLayoutState(const juce::ValueTree& state);
    void refreshAuthState();
    void openLagDaemonProfile();
    void showProjectMenu();
    void showSuiteSettingsWindow();
    void closeSuiteSettingsWindow();
    void chooseSuiteDirectory(const juce::String& fieldId);
    void applySuiteSettings(const SuiteSettings& settings);
    void createNewProject();
    void beginCreateNewProject();
    void createProjectFromTemplate();
    void beginCreateProjectFromTemplate();
    void openProject();
    void openProject(const juce::File& containerFile);
    void beginOpenProject();
    void saveProject();
    void saveProjectAs();
    void saveProjectAsTemplate();
    void guardUnsavedProjectChange(const juce::String& actionName, const std::function<void()>& action);
    void revealProjectFolder();
    void showAudioSettings();
    void configureVstSearchPaths();
    void importVstPathList();
    static juce::StringArray parseVstPathList(const juce::String& rawList);
    void editControlSurfaceMappings();
    void showMidiLearnDialog(const juce::String& targetId, const juce::String& displayLabel);
    void applyLearnedMidiBinding(const juce::String& targetId, const juce::String& deviceId, int channel, int number, bool isCC);
    void rescanVstCatalog();
    void showPluginLoadMenu(const std::function<void(const juce::File&)>& onPluginChosen);
    void refreshPluginsPanel();
    void loadPluginIntoCurrentInsert(const juce::File& file);
    void showFxStackWindow();
    void showMidiEditorWindow(int clipIndex);
    bool startMidiEditorPreview();
    void stopMidiEditorPreview(bool resetPlayheadToLoopStart = false);
    void updateMidiEditorPreviewNotes(double currentLocalSeconds, bool restartCycle = false);
    void releaseMidiEditorPreviewNotes();
    void refreshFxStackWindow();
    juce::DocumentWindow* findPluginEditorWindow(const juce::String& key) const;
    void closePluginEditorWindow(const juce::String& key);
    void closePluginEditorWindowsForTrack(int trackIndex);
    void refreshTrackPluginEditorState(int trackIndex);
    void openTrackPluginEditor(int trackIndex, int slotIndex);
    void pollPluginEditorReady(const juce::String& windowKey,
                               int trackIndex,
                               juce::Component::SafePointer<juce::Component> editorPointer,
                               int attemptsRemaining);
    void assignPluginToGraphNode(const juce::File& file);
    void showTour();
    void importProjectSounds();
    void refreshProjectAssets();
    void refreshFoleyArrangement();
    void refreshContentLibrary();
    void refreshTutorialLibrary();
    void refreshAiContextStore();
    void downloadContentItem(const ContentLibrary::Item& item);
    void activateContentItem(const ContentLibrary::Item& item);
    void openProjectAsset(const creation::assets::AssetDescriptor& asset);
    void placeProjectAssetOnTracker(const creation::assets::AssetDescriptor& asset);
    void exportProjectAssetRaw(const creation::assets::AssetDescriptor& asset);
    int placeAudioAssetOnTracker(const creation::assets::AssetDescriptor& asset,
                                 int targetTrack,
                                 double startSeconds,
                                 const juce::String& sourceTool,
                                 juce::String& errorMessage);
    bool importAudioFilesToTracker(const juce::StringArray& filePaths, int preferredTrack, double startSeconds);
    std::optional<creation::assets::AssetDescriptor> resolveTimelineClipAsset(const cs::TimelineClip& clip) const;
    void resolveTrackerClipAssetFiles();
    void launchTutorialItem(const ContentPanel::TutorialItem& item);
    bool chooseStorageRoot(bool promptWhenAlreadyConfigured = false);
    bool ensureStorageRootConfigured();
    bool ensureProjectSessionActive(juce::String& errorMessage);
    juce::String createRecordingTakeName() const;
    void refreshRecentTakes();
    bool startRecordingSession();
    void stopRecordingSession();
    void executeAiTaskStep(const CreationStationTaskPlanner::TaskStep& step);
    void launchAiCompletion(const CreationStationContextEngine::ContextPacket& packet);
    void showAiSidebar();
    void setAiSidebarCollapsed(bool shouldCollapse);
    void syncSemanticAppContext();
    bool loadSuiteAiProviderSettings();
    bool saveSuiteAiProviderSettings(const AiProviderSettings& settings, juce::String& errorMessage);
    void refreshAiModelCatalog();
    WorkspaceMode workspaceModeFromString(const juce::String& modeName) const;
    void configureTutorialOverlay();
    std::vector<TourGuideOverlay::Step> buildTutorialSteps(const cw::tutorial::Script& script);
    void executeTutorialActions(const juce::Array<cw::tutorial::Action>& actions);
    juce::Rectangle<int> tutorialTargetBoundsForId(const juce::String& targetId) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
