#pragma once

#include <JuceHeader.h>
#include "Auth/DesktopAuthSession.h"
#include "Audio/WorkstationAudioEngine.h"
#include "ControlSurface/XTouchControlSurface.h"
#include "Project/ProjectManager.h"
#include "Views/AuthGateView.h"
#include "Views/AiPanel.h"
#include "Views/ArrangeView.h"
#include "Views/DslPanel.h"
#include "Views/GraphPanel.h"
#include "Views/MixerPanel.h"
#include "Views/RecordView.h"

class MainComponent final : public juce::Component
{
public:
    enum class WorkspaceMode
    {
        arrange,
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
        juce::TextButton arrangeButton { "DAW" };
        juce::TextButton mixButton { "Mix" };
        juce::TextButton nodeButton { "Node" };
        juce::TextButton codeButton { "DSL" };
        juce::TextButton recordButton { "Record" };
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
    WorkstationAudioEngine engine;
    XTouchControlSurface midiSurface;
    AuthGateView authGateView;
    TransportBar transportBar;
    ViewModeBar viewModeBar;
    PluginRackBar pluginRackBar;
    ArrangeView arrangeView;
    MixerPanel mixerPanel;
    GraphPanel graphPanel;
    DslPanel dslPanel;
    RecordView recordView;
    AiPanel aiPanel;
    std::unique_ptr<juce::FileChooser> pluginChooser;
    std::unique_ptr<juce::DocumentWindow> pluginEditorWindow;
    juce::Component::SafePointer<TransportBar> transportBarSafe;
    juce::Component::SafePointer<PluginRackBar> pluginRackBarSafe;
    juce::Component::SafePointer<MixerPanel> mixerPanelSafe;
    ProjectManager projectManager;
    std::unique_ptr<juce::FileChooser> projectChooser;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
