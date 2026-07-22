#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "AI/CreationStationContextEngine.h"
#include "AI/CreationStationAppManifest.h"
#include "AI/CreationStationContextStore.h"
#include "AI/CreationStationTaskPlanner.h"
#include "AI/OpenAiChatClient.h"
#include "AI/LiteSemRagApiClient.h"
#include "AI/OpenAiModelCatalogClient.h"
#include "Auth/DesktopAuthSession.h"
#include "Audio/StudioIOModel.h"
#include "Audio/VstPluginCatalog.h"
#include "Audio/WorkstationAudioEngine.h"
#include "ControlSurface/XTouchControlSurface.h"
#include "Content/ContentLibrary.h"
#include "Content/ContentApiClient.h"
#include "Project/ProjectManager.h"
#include "Tutorial/GuidedTutorial.h"
#include "Timeline/TimelineModel.h"
#include "Language/PatinaArtifactLoader.h"
#include "Views/AuthGateView.h"
#include "Views/AiPanel.h"
#include "Views/ArrangeView.h"
#include "Views/ContentPanel.h"
#include "Views/DslPanel.h"
#include "Views/GraphPanel.h"
#include "Views/MixerPanel.h"
#include "Views/PluginsPanel.h"
#include "Views/RecordView.h"
#include "Views/SettingsPanel.h"
#include "Views/ScorePanel.h"
#include "Views/SignalLabPanel.h"
#include "Views/TrackerPanel.h"
#include "Views/TourGuideOverlay.h"

class MainComponent final : public juce::Component,
                            private juce::Timer
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
        settings
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
        juce::TextButton arrangeButton { "Foley" };
        juce::TextButton signalButton { "Signal" };
        juce::TextButton libraryButton { "Library" };
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
        juce::TextButton loadButton { "Load VST3" };
        juce::TextButton unloadButton { "Unload" };

    private:
        Context context = Context::master;
        int selectedTrackIndex = -1;
        bool hasPlugin = false;
    };

    class TransportBar final : public juce::Component
    {
    public:
        TransportBar();

        std::function<void()> onPlay;
        std::function<void()> onPause;
        std::function<void()> onStop;
        std::function<void()> onRecord;
        std::function<void()> onRewind;
        std::function<void()> onFastForward;
        std::function<void(bool)> onLoopChanged;
        std::function<void(bool)> onClickChanged;
        std::function<void()> onSignInRequested;
        std::function<void()> onOpenProfilePageRequested;
        std::function<void()> onLogoutRequested;
        std::function<void()> onProjectMenuRequested;
        std::function<void()> onTourRequested;
        std::function<void()> onAudioRequested;

        void setProfile(const DesktopAuthSession::SessionData& session);
        void clearProfile();
        void setProjectLabel(const juce::String& label);
        juce::Rectangle<int> getProjectButtonScreenBounds() const;
        void setStatusText(const juce::String& text);
        void setMidiStatusText(const juce::String& text);
        void setPlaybackVisualState(bool playing, bool recording);

        void resized() override;
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent& event) override;

        juce::Label titleLabel;
        juce::Image logoImage;
        juce::Label midiStatusLabel;
        juce::TextButton playButton { "Play" };
        juce::TextButton pauseButton { "Pause" };
        juce::TextButton stopButton { "Stop" };
        juce::TextButton recordButton { "Record" };
        juce::ToggleButton loopButton { "Loop" };
        juce::ToggleButton clickButton { "Click" };
        juce::TextButton rewindButton { "Rew" };
        juce::TextButton fastForwardButton { "Fwd" };
        juce::TextButton signInButton { "Sign In" };
        juce::TextButton projectButton { "Project" };
        juce::TextButton audioButton { "Audio" };
        juce::TextButton tourButton { "Tour" };
        juce::Label profileNameLabel;
        juce::Label profileDetailLabel;
        juce::Image profileBadgeImage;
        juce::Label statusLabel;
        juce::Rectangle<int> transportControlBounds;

    private:
        juce::String profileInitials;
        juce::String profileTierName;
        juce::String projectText;
        juce::Rectangle<int> profileChipBounds;
        bool profileVisible = false;
    };

    juce::AudioDeviceManager deviceManager;
    DesktopAuthSession authSession { "creative-workstation" };
    CreationStationContextEngine contextEngine;
    CreationStationContextStore contextStore;
    CreationStationTaskPlanner taskPlanner;
    CreationStationAppManifest appManifest;
    LiteSemRagApiClient semanticApiClient;
    OpenAiChatClient openAiChatClient;
    OpenAiModelCatalogClient modelCatalogClient;
    WorkstationAudioEngine engine;
    XTouchControlSurface midiSurface;
    AuthGateView authGateView;
    TransportBar transportBar;
    ViewModeBar viewModeBar;
    PluginRackBar pluginRackBar;
    TrackerPanel trackerPanel;
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
    TourGuideOverlay tourOverlay;
    std::unique_ptr<juce::FileChooser> pluginChooser;
    std::unique_ptr<juce::DocumentWindow> pluginEditorWindow;
    std::array<std::unique_ptr<juce::DocumentWindow>, 11> workspacePopoutWindows;
    juce::Label poppedWorkspacePlaceholder;
    juce::Component::SafePointer<TransportBar> transportBarSafe;
    juce::Component::SafePointer<PluginRackBar> pluginRackBarSafe;
    juce::Component::SafePointer<MixerPanel> mixerPanelSafe;
    ProjectManager projectManager;
    VstPluginCatalog vstPluginCatalog;
    ContentLibrary contentLibrary;
    ContentApiClient contentApiClient;
    std::unique_ptr<juce::FileChooser> storageRootChooser;
    std::unique_ptr<juce::FileChooser> projectChooser;
    std::unique_ptr<juce::FileChooser> assetChooser;
    std::unique_ptr<juce::FileChooser> renderExportChooser;
    std::unique_ptr<juce::FileChooser> rawAssetExportChooser;
    std::unique_ptr<juce::FileChooser> patchChooser;
    std::unique_ptr<juce::FileChooser> patinaArtifactChooser;
    std::unique_ptr<juce::FileChooser> contentUploadChooser;
    std::unique_ptr<juce::DocumentWindow> audioDeviceWindow;
    std::vector<bool> armedTracks;
    std::vector<bool> monitoredTracks;
    bool authenticated = false;
    WorkspaceMode activeMode = WorkspaceMode::tracker;
    AiProviderSettings aiProviderSettings;
    bool autoloadLastProject = false;
    bool aiSidebarCollapsed = false;
    bool layoutDirty = false;
    double layoutLastChangeWallSeconds = 0.0;
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
    cs::TimelineModel timelineModel;
    double transportStartWallSeconds = 0.0;
    double transportStartTimelineSeconds = 0.0;
    bool metronomeEnabled = false;
    int activeRecordingTrack = -1;
    int selectedClipIndex = -1;
    bool clipDragUndoCaptured = false;
    std::vector<juce::ValueTree> timelineUndoStack;
    std::vector<juce::ValueTree> timelineRedoStack;

    void timerCallback() override;
    bool keyPressed(const juce::KeyPress& key) override;
    juce::File getSessionFile() const;
    juce::ValueTree createProjectStateForSave();
    void remapTemplateStateFilesToCurrentProject(juce::ValueTree& state) const;
    void saveSessionToDisk(bool userInitiated = false);
    void loadSessionFromDisk();
    bool prepareTrackerPlayback();
    bool buildTrackerPlaybackTargets(juce::Array<WorkstationAudioEngine::PlaybackClipTarget>& targets,
                                     double& durationSeconds,
                                     juce::String& errorMessage) const;
    bool renderFullMixToProject();
    void exportFullMixAsWav();
    void pushTimelineUndoState();
    void pushTimelineUndoState(const juce::ValueTree& stateBeforeEdit);
    void undoTimelineEdit();
    void redoTimelineEdit();
    void restoreTimelineEditState(const juce::ValueTree& state, const juce::String& statusText);
    void splitClipAt(int clipIndex, double splitSeconds);
    void duplicateClip(int clipIndex);
    void deleteClip(int clipIndex);
    void renameClip(int clipIndex);
    juce::File getAppSettingsFile() const;
    void saveAppSettings();
    void loadAppSettings();
    void applySelectedAudioDeviceSettings();
    void refreshInsertRack();
    void syncTrackViews();
    void refreshTrackInputSources();
    void refreshAudioDeviceSettingsView();
    void setAudioSystem(const juce::String& audioSystem);
    void setAudioInputDevice(const juce::String& inputDeviceName);
    void setAudioOutputDevice(const juce::String& outputDeviceName);
    void addTrack();
    void removeTrack(int trackIndex);
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
    void createNewProject();
    void beginCreateNewProject();
    void createProjectFromTemplate();
    void beginCreateProjectFromTemplate();
    void openProject();
    void beginOpenProject();
    void saveProject();
    void saveProjectAs();
    void saveProjectAsTemplate();
    void guardUnsavedProjectChange(const juce::String& actionName, const std::function<void()>& action);
    void revealProjectFolder();
    void showAudioSettings();
    void configureVstSearchPaths();
    void editControlSurfaceMappings();
    void rescanVstCatalog();
    void showPluginLoadMenu(const std::function<void(const juce::File&)>& onPluginChosen);
    void refreshPluginsPanel();
    void loadPluginIntoCurrentInsert(const juce::File& file);
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
    void openProjectAsset(const ProjectManager::ProjectAsset& asset);
    void placeProjectAssetOnTracker(const ProjectManager::ProjectAsset& asset);
    void exportProjectAssetRaw(const ProjectManager::ProjectAsset& asset);
    void launchTutorialItem(const ContentPanel::TutorialItem& item);
    bool chooseStorageRoot(bool promptWhenAlreadyConfigured = false);
    bool ensureStorageRootConfigured();
    juce::String createRecordingTakeName() const;
    void refreshRecentTakes();
    bool startRecordingSession();
    void stopRecordingSession();
    void executeAiTaskStep(const CreationStationTaskPlanner::TaskStep& step);
    void launchAiCompletion(const CreationStationContextEngine::ContextPacket& packet);
    void showAiSidebar();
    void setAiSidebarCollapsed(bool shouldCollapse);
    void syncSemanticAppContext();
    void refreshAiModelCatalog();
    WorkspaceMode workspaceModeFromString(const juce::String& modeName) const;
    void configureTutorialOverlay();
    std::vector<TourGuideOverlay::Step> buildTutorialSteps(const cw::tutorial::Script& script);
    void executeTutorialActions(const juce::Array<cw::tutorial::Action>& actions);
    juce::Rectangle<int> tutorialTargetBoundsForId(const juce::String& targetId) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
