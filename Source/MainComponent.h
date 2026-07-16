#pragma once

#include <JuceHeader.h>
#include <vector>
#include "AI/CreationStationContextEngine.h"
#include "AI/CreationStationContextStore.h"
#include "Auth/DesktopAuthSession.h"
#include "Audio/WorkstationAudioEngine.h"
#include "ControlSurface/XTouchControlSurface.h"
#include "Content/ContentLibrary.h"
#include "Content/ContentApiClient.h"
#include "Project/ProjectManager.h"
#include "Language/PatinaArtifactLoader.h"
#include "Views/AuthGateView.h"
#include "Views/AiPanel.h"
#include "Views/ArrangeView.h"
#include "Views/ContentPanel.h"
#include "Views/DslPanel.h"
#include "Views/GraphPanel.h"
#include "Views/MixerPanel.h"
#include "Views/RecordView.h"
#include "Views/SignalLabPanel.h"
#include "Views/TourGuideOverlay.h"

class MainComponent final : public juce::Component
{
public:
    enum class WorkspaceMode
    {
        arrange,
        signal,
        library,
        mix,
        node,
        code,
        record,
        ai
    };

    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ViewModeBar final : public juce::Component
    {
    public:
        ViewModeBar();

        std::function<void(WorkspaceMode)> onModeSelected;

        void setActiveMode(WorkspaceMode newMode);
        void resized() override;
        void paint(juce::Graphics&) override;

    private:
        WorkspaceMode activeMode = WorkspaceMode::arrange;
        juce::Label titleLabel;
        juce::TextButton arrangeButton { "Foley" };
        juce::TextButton signalButton { "Signal" };
        juce::TextButton libraryButton { "Library" };
        juce::TextButton mixButton { "Layers" };
        juce::TextButton nodeButton { "Patch" };
        juce::TextButton codeButton { "Script" };
        juce::TextButton recordButton { "Capture" };
        juce::TextButton aiButton { "AI" };
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
        std::function<void()> onUnloadPlugin;
        std::function<void()> onOpenPluginEditor;
        std::function<void(bool)> onBypassChanged;

        void setContextMaster();
        void setContextTrack(int trackIndex, const juce::String& trackName);
        bool isTrackContext() const noexcept { return context == Context::track; }
        int getTrackIndex() const noexcept { return selectedTrackIndex; }

        void setPluginName(const juce::String& name);
        void setBypassed(bool shouldBypass);
        void setHasPlugin(bool hasPlugin);

        void resized() override;
        void paint(juce::Graphics&) override;

        juce::Label titleLabel;
        juce::Label contextLabel;
        juce::Label pluginNameLabel;
        juce::ToggleButton bypassButton { "Bypass" };
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
        std::function<void()> onStop;
        std::function<void()> onRecord;
        std::function<void()> onSignInRequested;
        std::function<void()> onOpenProfilePageRequested;
        std::function<void()> onLogoutRequested;
        std::function<void()> onProjectMenuRequested;
        std::function<void()> onTourRequested;
        std::function<void()> onAudioRequested;

        void setProfile(const DesktopAuthSession::SessionData& session);
        void clearProfile();
        void setProjectLabel(const juce::String& label);
        void setStatusText(const juce::String& text);
        void setMidiStatusText(const juce::String& text);

        void resized() override;
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent& event) override;

        juce::Label titleLabel;
        juce::Image logoImage;
        juce::Label midiStatusLabel;
        juce::TextButton playButton { "Play" };
        juce::TextButton stopButton { "Stop" };
        juce::TextButton recordButton { "Record" };
        juce::TextButton signInButton { "Sign In" };
        juce::TextButton projectButton { "Project" };
        juce::TextButton audioButton { "Audio" };
        juce::TextButton tourButton { "Tour" };
        juce::Label profileNameLabel;
        juce::Label profileDetailLabel;
        juce::Image profileBadgeImage;
        juce::Label statusLabel;

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
    WorkstationAudioEngine engine;
    XTouchControlSurface midiSurface;
    AuthGateView authGateView;
    TransportBar transportBar;
    ViewModeBar viewModeBar;
    PluginRackBar pluginRackBar;
    ArrangeView arrangeView;
    SignalLabPanel signalLabPanel;
    ContentPanel contentPanel;
    MixerPanel mixerPanel;
    GraphPanel graphPanel;
    DslPanel dslPanel;
    RecordView recordView;
    AiPanel aiPanel;
    TourGuideOverlay tourOverlay;
    std::unique_ptr<juce::FileChooser> pluginChooser;
    std::unique_ptr<juce::DocumentWindow> pluginEditorWindow;
    juce::Component::SafePointer<TransportBar> transportBarSafe;
    juce::Component::SafePointer<PluginRackBar> pluginRackBarSafe;
    juce::Component::SafePointer<MixerPanel> mixerPanelSafe;
    ProjectManager projectManager;
    ContentLibrary contentLibrary;
    ContentApiClient contentApiClient;
    std::unique_ptr<juce::FileChooser> storageRootChooser;
    std::unique_ptr<juce::FileChooser> projectChooser;
    std::unique_ptr<juce::FileChooser> assetChooser;
    std::unique_ptr<juce::FileChooser> patchChooser;
    std::unique_ptr<juce::FileChooser> patinaArtifactChooser;
    std::unique_ptr<juce::FileChooser> contentUploadChooser;
    std::unique_ptr<juce::DocumentWindow> audioDeviceWindow;
    std::vector<bool> armedTracks;
    bool authenticated = false;
    WorkspaceMode activeMode = WorkspaceMode::arrange;

    juce::File getSessionFile() const;
    void saveSessionToDisk() const;
    void loadSessionFromDisk();
    void refreshInsertRack();
    void setWorkspaceMode(WorkspaceMode mode);
    void refreshModeVisibility();
    void refreshAuthState();
    void openLagDaemonProfile();
    void showProjectMenu();
    void createNewProject();
    void openProject();
    void saveProject();
    void revealProjectFolder();
    void showAudioSettings();
    void showTour();
    void importProjectSounds();
    void refreshProjectAssets();
    void refreshFoleyArrangement();
    void refreshContentLibrary();
    void refreshAiContextStore();
    void downloadContentItem(const ContentLibrary::Item& item);
    void activateContentItem(const ContentLibrary::Item& item);
    bool ensureStorageRootConfigured();
    juce::String createRecordingTakeName() const;
    void refreshRecentTakes();
    bool startRecordingSession();
    void stopRecordingSession();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
