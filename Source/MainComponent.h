#pragma once

#include <map>
#include <set>
#include <JuceHeader.h>
#include <array>
#include <vector>
#include "AI/CreationStationContextEngine.h"
#include "AI/CreationStationAppManifest.h"
#include "AI/CreationStationContextStore.h"
#include "AI/CreationStationTaskPlanner.h"
#include "AI/AiProviderSettings.h"
#include <creation/services/SuiteAiChatClient.h>
#include <creation/services/SuiteUndoService.h>
#include "AI/LiteSemRagApiClient.h"
#include <creation/ui/SuiteDesktopAuthSession.h>
#include "Audio/StudioIOModel.h"
#include "Audio/VstPluginCatalog.h"
#include "Audio/WorkstationAudioEngine.h"
#include "Video/VideoCaptureService.h"
#include "Video/VideoDecodeService.h"
#include "ControlSurface/XTouchControlSurface.h"
#include "ControlSurface/ControlSurfaceMappingStore.h"
#include "Content/ContentLibrary.h"
#include "Content/ContentApiClient.h"
#include "Feedback/FeedbackSettingsStore.h"
#include "Feedback/FeedbackMetricsClient.h"
#include "Feedback/MetricsCollector.h"
#include "Language/StationFrustPodService.h"
#include <creation/assets/ProjectContainerService.h>
#include <creation/assets/ProjectAssetService.h>
#include <creation/assets/ProjectSession.h>
#include <creation/ui/CreationSuiteHeaderBar.h>
#include <creation/ui/SuiteShellController.h>
#include <CreationDock/DockManager.h>
#include "Suite/SuiteSettings.h"
#include "Tutorial/GuidedTutorial.h"
#include "Timeline/TimelineModel.h"
#include "Views/AuthGateView.h"
#include "Views/AiPanel.h"
#include "Views/ContentPanel.h"
#include "Views/ProgressTask.h"
#include "Video/VideoPreviewComponent.h"
#include "Video/Gl/VideoGlView.h"
#include "Video/Gl/VideoLayerParams.h"
#include "Video/Gl/VideoPanelHost.h"
#include "Video/VideoScrubPreview.h"
#include "Views/RenderDialog.h"
#include "Views/ToastMessage.h"
#include "Views/DslPanel.h"
#include "Views/MidiEditorPanel.h"
#include "Views/MixerPanel.h"
#include "Views/PluginBrowserList.h"
#include "Views/PluginsPanel.h"
#include "Views/SamplePackBuilderPanel.h"
#include "Views/RecordView.h"
#include "Views/FoleyPanel.h"
#include "Views/SettingsPanel.h"
#include "Views/ScorePanel.h"
#include "Views/SignalLabPanel.h"
#include "Views/TrackerPanel.h"
#include "Views/TourGuideOverlay.h"
#include "Views/FeedbackDialog.h"
#include <creation/ui/SuiteSettingsPanel.h>

class MainComponent final : public juce::Component,
                            private juce::MenuBarModel,
                            private juce::Timer,
                            private juce::KeyListener
{
public:
    enum class WorkspaceMode
    {
        tracker,
        signal,
        library,
        mix,
        plugins,
        code,
        record,
        score,
        settings,
        // Added after settings (not inserted earlier in the list) so every already-persisted
        // layout/session's saved integer mode value keeps meaning what it always meant - the
        // button itself is still positioned right after Tracker in ViewModeBar, since that's a
        // purely visual layout choice independent of this enum's declaration order.
        sampler,
        // Same reasoning as sampler above - appended, not inserted, to preserve ordinal
        // stability for already-persisted layouts.
        foley
    };

    using StartupProgressCallback = std::function<void(const juce::String& statusText, float progress)>;

    MainComponent();
    explicit MainComponent(StartupProgressCallback startupProgressCallback);
    ~MainComponent() override;

    void confirmCloseApplication(const std::function<void(bool shouldClose)>& onDecision);
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

private:
    class ViewModeBar final : public juce::Component
    {
    public:
        ViewModeBar();

        std::function<void(juce::Component&)> onProjectMenuRequested;
        std::function<void(juce::Component&)> onToolsMenuRequested;
        std::function<void(juce::Component&)> onHelpMenuRequested;

        void resized() override;
        void paint(juce::Graphics&) override;

    private:
        juce::TextButton projectButton { "Project" };
        juce::TextButton toolsButton { "Tools" };
        juce::TextButton helpButton { "Help" };
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
    WorkstationAudioEngine engine;
    cs::VideoCaptureService videoCaptureService;
    cw::StationFrustPodService frustPodService;
    XTouchControlSurface midiSurface;
    AuthGateView authGateView;
    CreationSuiteHeaderBar transportBar;
    std::unique_ptr<juce::MenuBarComponent> menuBar;
    std::unique_ptr<CreationDock::DockManager> dockManager;
    PluginRackBar pluginRackBar;
    TrackerPanel trackerPanel;
    SamplePackBuilderPanel samplePackBuilderPanel;
    SignalLabPanel signalLabPanel;
    ContentPanel contentPanel;
    MixerPanel mixerPanel;
    PluginsPanel pluginsPanel;
    DslPanel dslPanel;
    RecordView recordView;
    FoleyPanel foleyPanel;
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
    std::unique_ptr<juce::DocumentWindow> feedbackWindow;
    juce::Component::SafePointer<FeedbackDialog> feedbackDialogPanel;
    creation_station::FeedbackSettingsStore::Settings feedbackSettings;
    creation_station::FeedbackMetricsClient feedbackMetricsClient;
    creation_station::MetricsCollector metricsCollector;
    void showFeedbackWindow();
    std::unique_ptr<juce::DocumentWindow> midiEditorWindow;
    juce::Component::SafePointer<MidiEditorPanel> midiEditorPanel;
    std::array<std::unique_ptr<juce::DocumentWindow>, 12> workspacePopoutWindows;
    juce::Label poppedWorkspacePlaceholder;
    juce::Component::SafePointer<CreationSuiteHeaderBar> transportBarSafe;
    juce::Component::SafePointer<PluginRackBar> pluginRackBarSafe;
    juce::Component::SafePointer<MixerPanel> mixerPanelSafe;
    creation::assets::ProjectSession projectSession;
    // Rendered WAV file for each Signal clip's source patch asset, so playback-target builds (which
    // run on every scrub) don't hit the VFS. Cleared whenever a patch is saved.
    std::map<juce::String, juce::File> signalRenderFiles;

    // Video. The picture is a dock panel (dock it, float it, resize it); the sound is the video's own audio
    // track, decoded once to a WAV so it plays through the clip's mixer track like any other audio clip.
    cs::VideoGlView videoView; // the video picture: drawn by OpenGL, effects run on the GPU
    cs::VideoPanelHost videoPanelHost { videoView }; // what the video panel shows: the picture plus a status strip
    // One decoder per video clip that is on screen at the playhead (each layer of the picture has its own).
    struct VideoLayerFeed
    {
        cs::VideoScrubPreview scrub;
        juce::Image frame;
        juce::String requestKey;
    };
    std::map<juce::String, std::unique_ptr<VideoLayerFeed>> videoFeeds; // clip id -> its decoder and latest frame
    std::vector<juce::String> videoActiveOrder;                          // clip ids at the playhead, bottom layer first
    void refreshVideoLayers();                                           // republish the layers from the feeds and the clips' settings
    void showVideoClipSettings(int clipIndex);
    std::unique_ptr<juce::DocumentWindow> videoSettingsWindow;
    std::map<juce::String, juce::File> videoAudioFiles; // asset id -> local WAV of that video's sound
    std::set<juce::String> videosWithoutAudio;          // asset ids whose video has no sound track
    void updateVideoView(double timelineSeconds);
    void openVideoViewForPlayback();
    // The clip menu's video/sound actions (1 split the sound onto its own track, 2 unlink, 3 link, 4 put back,
    // 5 video effects and layout).
    void handleClipSoundAction(int clipIndex, int action);
    // Right-click > Add Clip...: a picker that offers what fits the track, with a picture and facts for each item.
    void showAddClipPicker(int trackIndex, double startSeconds);
    // Reads what each video/audio/render/patch asset actually is (length, size of picture, channels, a thumbnail)
    // for any asset that has no details yet, in a progress window. Returns false when there was nothing to do.
    bool ensureAssetDetails(std::function<void()> whenDone);
    void splitSoundFromVideo(int clipIndex);
    bool videoClipsNeedAudio() const;
    // Makes sure every video clip's sound is ready (extracting and caching it in the project when it is not),
    // in a progress window. Returns false when nothing needed doing.
    bool prepareVideoAudio(std::function<void()> whenDone = {});
    juce::File getVideoAudioFolder() const;
    static juce::String videoAudioCachePath(const juce::String& assetId);
    // Parsed patch (and its content key) for each Signal clip's patch asset, so timeline refreshes
    // don't re-fetch it from the VFS. Cleared whenever a patch is saved.
    std::map<juce::String, std::pair<juce::String, cw::PatchDocument>> signalPatchDocs;
    // Which saved arrangement/patch/foley-setup (project asset id) is currently active in each
    // tool tab -- used to auto-restore the right one when the project reopens.
    juce::String currentArrangementAssetId;
    juce::String currentSignalLabAssetId;
    juce::String currentFoleyAssetId;
    VstPluginCatalog vstPluginCatalog;
    ContentLibrary contentLibrary;
    ContentApiClient contentApiClient;
    std::unique_ptr<juce::FileChooser> storageRootChooser;
    std::unique_ptr<juce::FileChooser> projectChooser;
    std::unique_ptr<juce::FileChooser> assetChooser;
    std::unique_ptr<juce::FileChooser> renderExportChooser;
    std::unique_ptr<juce::FileChooser> rawAssetExportChooser;
    std::unique_ptr<juce::FileChooser> frustSourceChooser;
    std::unique_ptr<juce::FileChooser> contentUploadChooser;
    std::unique_ptr<juce::FileChooser> suiteDirectoryChooser;
    std::unique_ptr<juce::DocumentWindow> audioDeviceWindow;
    std::unique_ptr<juce::DocumentWindow> midiLearnWindow;
    ControlSurfaceMappingStore controlSurfaceMappings;
    std::vector<bool> armedTracks;
    std::vector<bool> monitoredTracks;
    juce::Array<creation::assets::ProjectContainerService::ProjectSummary> currentProjectMenuProjects;
    juce::String currentProjectMenuListError;
    // Wall-clock timestamp of the last manual write into each track's automation lane (Touch
    // mode's idle-release timer). Parallel to armedTracks, resized alongside it.
    std::vector<double> automationLastManualWriteWallSeconds;
    bool authenticated = false;
    WorkspaceMode activeMode = WorkspaceMode::tracker;
    AiProviderSettings aiProviderSettings;
    // Full suite AI account/routing state (Settings -> Suite AI Accounts) - CreationStation only
    // ever selects among these named accounts, it never creates or edits one itself.
    creation::services::SuiteAiSettings suiteAiSettings;
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
    double transportLoopDelayWaitUntilWallSeconds = 0.0;
    bool transportIsWaitingForLoopDelay = false;
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
    // includeSignalClips=false is the live-playback form: Signal clips are left out of the audio-file
    // targets (they are run live by the engine, see buildSignalClipTargets) but still count toward
    // durationSeconds. true (the default) is the offline/render form, which bakes them to WAV.
    bool buildTrackerPlaybackTargets(juce::Array<WorkstationAudioEngine::PlaybackClipTarget>& targets,
                                     double& durationSeconds,
                                     juce::String& errorMessage,
                                     bool includeSignalClips = true);
    bool buildSignalClipTargets(juce::Array<WorkstationAudioEngine::SignalClipTarget>& targets,
                                juce::String& errorMessage);
    // Reads a Signal clip's patch (cached per asset) and its content key. False (with errorMessage set
    // for a real read error) when the clip has no readable patch.
    bool loadSignalClipPatch(const cs::TimelineClip& clip, cw::PatchDocument& patch, juce::String& patchKey, juce::String& errorMessage);
    void previewScrubAudioAt(double timelineSeconds);
    void refreshMidiPlaybackClips();
    // The render dialog: what to render, where it goes, and in what format. Opens with `preferredDestination`
    // selected; the last settings used are remembered for the session.
    void showRenderDialog(RenderRequest::Destination preferredDestination);
    void beginRender(const RenderRequest& request);
    void runRenderJob(const RenderRequest& request, const juce::File& destinationFile);
    RenderRequest lastRenderRequest;
    juce::Component::SafePointer<juce::DialogWindow> renderDialogWindow;
    // The running render (modal progress window + worker thread); kept alive until it has finished.
    std::unique_ptr<juce::ThreadWithProgressWindow> renderJob;
    // Writes a rendered mix into the project as a Render asset (encoded in memory, no temp file).
    bool saveRenderToProject(const juce::AudioBuffer<float>& buffer, double sampleRate, int bitsPerSample, bool dither,
                             const juce::String& displayName, creation::assets::AssetDescriptor& savedAsset,
                             juce::String& errorMessage);
    void toggleProjectAssetPreview(const creation::assets::AssetDescriptor& asset);
    // Errors get a dialog with room to read them, a Close button and a Copy button - not the header's small
    // status label. Errors that arrive while a dialog is open are collected into the next one, so a burst
    // (say, importing several files that all fail) is one dialog rather than a stack of them.
    void reportError(const juce::String& message);
    // Anything that is not an error (confirmations, "stop playback first", ...): a readable message that
    // clears itself, instead of the header's small status label, which is gone.
    void showToast(const juce::String& message);
    ToastMessage toast;
    void showPendingErrors();
    juce::StringArray pendingErrors;
    bool errorDialogShowing = false;
    juce::String previewingProjectAssetId;
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
    void initialiseDockingWorkspace();
    // Registers (or re-registers) the one named tool panel this suite ships, wiring its
    // title and persistent content component into a fresh CreationDock::DockPanel -- the
    // shared DockManager destroys the DockPanel wrapper on unregisterPanel(), so "showing"
    // a previously-closed panel means calling this again, not reusing an old handle.
    CreationDock::DockPanel* registerNamedDockPanel(const juce::String& panelId, CreationDock::DockTargetZone zone);
    void setWorkspaceMode(WorkspaceMode mode);
    void resetDockLayout();
    void toggleToolWindow(WorkspaceMode mode);
    void toggleAiToolWindow();
    void toggleDockPanel(const juce::String& panelId, CreationDock::DockTargetZone fallbackZone);
    void activateDockPanel(const juce::String& panelId, CreationDock::DockTargetZone fallbackZone);
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
    void openProject(const juce::String& projectId);
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
    // Generic version of the above, for callers that want the raw captured binding instead of
    // having it saved into ControlSurfaceMappingStore under a transport targetId - e.g. Signal
    // Lab's MIDI Control nodes, which keep their own binding on the node itself.
    void requestGenericMidiLearn(const juce::String& displayLabel,
                                 std::function<void(juce::String deviceId, int channel, int number, bool isCC)> onLearned,
                                 WorkstationAudioEngine::MidiLearnKind expectedKind = WorkstationAudioEngine::MidiLearnKind::Any);
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
    void showTour();
    void importProjectSounds();
    void refreshProjectAssets();
    void refreshContentLibrary();
    void refreshTutorialLibrary();
    void refreshAiContextStore();
    void downloadContentItem(const ContentLibrary::Item& item);
    void activateContentItem(const ContentLibrary::Item& item);
    void openProjectAsset(const creation::assets::AssetDescriptor& asset);
    // Silent restores (no workspace-mode switch, no undo entry) for the "last active" named
    // asset in each tool tab, used both by the auto-restore-on-project-open path and by each
    // tool's own interactive Load menu.
    bool restoreArrangementAsset(const creation::assets::AssetDescriptor& asset);
    bool restoreSignalLabAsset(const creation::assets::AssetDescriptor& asset);
    bool restoreFoleyAsset(const creation::assets::AssetDescriptor& asset);
    void restoreLastActiveAssets(const juce::ValueTree& lastActiveAssetsState);
    void placeProjectAssetOnTracker(const creation::assets::AssetDescriptor& asset, double startSeconds = -1.0);
    void exportProjectAssetRaw(const creation::assets::AssetDescriptor& asset);
    int placeAudioAssetOnTracker(const creation::assets::AssetDescriptor& asset,
                                 int targetTrack,
                                 double startSeconds,
                                 const juce::String& sourceTool,
                                 juce::String& errorMessage);
    bool importAudioFilesToTracker(const juce::StringArray& filePaths, int preferredTrack, double startSeconds);
    // Adds an already-uploaded video (its bytes are in the project at `logicalPath`) to the project's asset
    // list and puts a clip for it on the track. Fast; message thread.
    int addImportedVideoToTracker(const juce::File& sourceFile, const juce::String& assetId, const juce::String& logicalPath, juce::int64 fileSize,
                                  const cs::VideoStreamInfo& info, const juce::MemoryBlock& thumbnailJpeg, int targetTrack, double startSeconds,
                                  juce::String& errorMessage);
    void importVideoFilesToTracker(const juce::StringArray& filePaths, int preferredTrack, double startSeconds);
    void runVideoImport(juce::StringArray filePaths, int trackIndex, double startSeconds);
    // The window for whatever long action is running (import, ...): progress bar, status line, Cancel.
    std::unique_ptr<ProgressTask> progressTask;
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
    void refreshAiAccountModelCachesAtStartup();
    void refreshAiPanelAccountsAndModels();
    void selectAiAccountForStation(const juce::String& accountId, const juce::String& modelNameOverride = {});
    WorkspaceMode workspaceModeFromString(const juce::String& modeName) const;
    void configureTutorialOverlay();
    std::vector<TourGuideOverlay::Step> buildTutorialSteps(const cw::tutorial::Script& script);
    void executeTutorialActions(const juce::Array<cw::tutorial::Action>& actions);
    juce::Rectangle<int> tutorialTargetBoundsForId(const juce::String& targetId) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
