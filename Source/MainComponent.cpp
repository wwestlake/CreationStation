#include "MainComponent.h"
#include "Branding.h"

namespace
{
juce::String makeInitials(const juce::String& displayName, const juce::String& email)
{
    auto source = displayName.isNotEmpty() ? displayName : email;
    source = source.trim();

    if (source.isEmpty())
        return "C";

    juce::StringArray words;
    words.addTokens(source, " ._-@", "");
    words.removeEmptyStrings();

    juce::String initials;
    for (int index = 0; index < words.size() && initials.length() < 2; ++index)
        initials << words[index].substring(0, 1).toUpperCase();

    if (initials.isEmpty())
        initials = source.substring(0, 1).toUpperCase();

    return initials;
}

juce::String profileDetailText(const DesktopAuthSession::SessionData& session)
{
    auto tierId = branding::getBestPatreonTierId(session.user.entitlements);
    auto tierName = branding::getPatreonTierDisplayName(tierId);

    if (tierName.isNotEmpty())
        return session.user.email + " • " + tierName;

    return session.user.email;
}

juce::String workspaceModeName(MainComponent::WorkspaceMode mode)
{
    switch (mode)
    {
        case MainComponent::WorkspaceMode::arrange: return "DAW";
        case MainComponent::WorkspaceMode::mix: return "Mix";
        case MainComponent::WorkspaceMode::node: return "Node";
        case MainComponent::WorkspaceMode::code: return "DSL";
        case MainComponent::WorkspaceMode::record: return "Record";
        case MainComponent::WorkspaceMode::ai: return "AI";
    }

    return "DAW";
}

juce::String makeRecordingTimestamp()
{
    return juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
}
}

MainComponent::TransportBar::TransportBar()
{
    setName("Transport");
    logoImage = branding::createCreationStationLogoImage(72);

    titleLabel.setText("Creation Station", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(28.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    midiStatusLabel.setText("MIDI: waiting for controller", juce::dontSendNotification);
    midiStatusLabel.setJustificationType(juce::Justification::centredLeft);
    midiStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(midiStatusLabel);

    projectButton.setButtonText("Project: No project open");
    addAndMakeVisible(projectButton);

    audioButton.onClick = [this]
    {
        if (onAudioRequested)
            onAudioRequested();
    };
    addAndMakeVisible(audioButton);

    tourButton.onClick = [this]
    {
        if (onTourRequested)
            onTourRequested();
    };
    addAndMakeVisible(tourButton);

    statusLabel.setText("Ready for audio work.", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centredRight);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff99a6b8));
    addAndMakeVisible(statusLabel);

    playButton.onClick = [this]
    {
        statusLabel.setText("Transport: play", juce::dontSendNotification);
        if (onPlay)
            onPlay();
    };
    stopButton.onClick = [this]
    {
        statusLabel.setText("Transport: stop", juce::dontSendNotification);
        if (onStop)
            onStop();
    };
    recordButton.onClick = [this]
    {
        statusLabel.setText("Transport: record armed", juce::dontSendNotification);
        if (onRecord)
            onRecord();
    };

    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(recordButton);

    signInButton.onClick = [this]
    {
        if (onSignInRequested)
            onSignInRequested();
    };
    addAndMakeVisible(signInButton);

    projectButton.onClick = [this]
    {
        if (onProjectMenuRequested)
            onProjectMenuRequested();
    };
    addAndMakeVisible(projectButton);

    profileNameLabel.setJustificationType(juce::Justification::centredLeft);
    profileNameLabel.setFont(juce::Font(17.0f).boldened());
    profileNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(profileNameLabel);

    profileDetailLabel.setJustificationType(juce::Justification::centredLeft);
    profileDetailLabel.setFont(juce::Font(13.0f));
    profileDetailLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));
    addAndMakeVisible(profileDetailLabel);

    clearProfile();
}

MainComponent::ViewModeBar::ViewModeBar()
{
    titleLabel.setText("Workspaces", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(18.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    auto setupButton = [this](juce::TextButton& button, WorkspaceMode mode)
    {
        button.setClickingTogglesState(true);
        button.onClick = [this, mode]
        {
            setActiveMode(mode);
            if (onModeSelected)
                onModeSelected(mode);
        };
        addAndMakeVisible(button);
    };

    setupButton(arrangeButton, WorkspaceMode::arrange);
    setupButton(mixButton, WorkspaceMode::mix);
    setupButton(nodeButton, WorkspaceMode::node);
    setupButton(codeButton, WorkspaceMode::code);
    setupButton(recordButton, WorkspaceMode::record);
    setupButton(aiButton, WorkspaceMode::ai);

    setActiveMode(WorkspaceMode::arrange);
}

void MainComponent::ViewModeBar::setActiveMode(WorkspaceMode newMode)
{
    activeMode = newMode;
    arrangeButton.setToggleState(activeMode == WorkspaceMode::arrange, juce::dontSendNotification);
    mixButton.setToggleState(activeMode == WorkspaceMode::mix, juce::dontSendNotification);
    nodeButton.setToggleState(activeMode == WorkspaceMode::node, juce::dontSendNotification);
    codeButton.setToggleState(activeMode == WorkspaceMode::code, juce::dontSendNotification);
    recordButton.setToggleState(activeMode == WorkspaceMode::record, juce::dontSendNotification);
    aiButton.setToggleState(activeMode == WorkspaceMode::ai, juce::dontSendNotification);
    repaint();
}

void MainComponent::ViewModeBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10141a));
    g.setColour(juce::Colour(0xff263140));
    g.drawLine(0.0f,
               static_cast<float>(getHeight()) - 1.0f,
               static_cast<float>(getWidth()),
               static_cast<float>(getHeight()) - 1.0f,
               1.0f);

    g.setColour(juce::Colour(0xff8ea0b7));
        g.setFont(juce::Font(13.0f));
    g.drawText(workspaceModeName(activeMode) + " active", getLocalBounds().reduced(12, 0), juce::Justification::centredRight, true);
}

void MainComponent::ViewModeBar::resized()
{
    auto area = getLocalBounds().reduced(14, 8);
    titleLabel.setBounds(area.removeFromLeft(140));
    area.removeFromLeft(8);
    auto buttonWidth = 86;
    arrangeButton.setBounds(area.removeFromLeft(buttonWidth));
    mixButton.setBounds(area.removeFromLeft(buttonWidth));
    nodeButton.setBounds(area.removeFromLeft(buttonWidth));
    codeButton.setBounds(area.removeFromLeft(buttonWidth));
    recordButton.setBounds(area.removeFromLeft(buttonWidth + 18));
    aiButton.setBounds(area.removeFromLeft(buttonWidth));
}

void MainComponent::TransportBar::setStatusText(const juce::String& text)
{
    statusLabel.setText(text, juce::dontSendNotification);
}

void MainComponent::TransportBar::setMidiStatusText(const juce::String& text)
{
    midiStatusLabel.setText(text, juce::dontSendNotification);
}

void MainComponent::TransportBar::setProjectLabel(const juce::String& label)
{
    projectText = label;
    projectButton.setButtonText(projectText);
    repaint();
}

void MainComponent::TransportBar::setProfile(const DesktopAuthSession::SessionData& session)
{
    profileVisible = true;
    profileNameLabel.setVisible(true);
    profileDetailLabel.setVisible(true);
    signInButton.setVisible(false);

    profileNameLabel.setText(session.user.displayName.isNotEmpty() ? session.user.displayName : session.user.email,
                             juce::dontSendNotification);
    profileDetailLabel.setText(profileDetailText(session), juce::dontSendNotification);
    profileInitials = makeInitials(session.user.displayName, session.user.email);

    auto tierId = branding::getBestPatreonTierId(session.user.entitlements);
    profileTierName = branding::getPatreonTierDisplayName(tierId);
    profileBadgeImage = branding::createPatreonBadgeImage(tierId, 36);

    repaint();
}

void MainComponent::TransportBar::clearProfile()
{
    profileVisible = false;
    profileNameLabel.setText({}, juce::dontSendNotification);
    profileDetailLabel.setText({}, juce::dontSendNotification);
    profileBadgeImage = {};
    profileInitials.clear();
    profileTierName.clear();
    profileNameLabel.setVisible(false);
    profileDetailLabel.setVisible(false);
    signInButton.setButtonText("Sign In");
    signInButton.setVisible(true);
    repaint();
}

void MainComponent::TransportBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f1115));
    if (logoImage.isValid())
        g.drawImageWithin(logoImage, 14, 9, 44, 44, juce::RectanglePlacement::centred, false);
    g.setColour(juce::Colour(0xff242a36));
    g.drawLine(0.0f,
               static_cast<float>(getHeight()) - 1.0f,
               static_cast<float>(getWidth()),
               static_cast<float>(getHeight()) - 1.0f,
               1.0f);

    if (profileVisible)
    {
        auto chip = profileChipBounds.toFloat();
        g.setColour(juce::Colour(0xff151b23));
        g.fillRoundedRectangle(chip.toFloat(), 16.0f);
        g.setColour(juce::Colour(0xff2c394c));
        g.drawRoundedRectangle(chip.toFloat(), 16.0f, 1.0f);

        auto avatar = chip.removeFromLeft(44).reduced(4);
        g.setColour(juce::Colour(0xff223041));
        g.fillEllipse(avatar.toFloat());
        g.setColour(juce::Colour(0xff8ea0b7));
        g.setFont(juce::Font(15.0f).boldened());
        g.drawText(profileInitials, avatar, juce::Justification::centred, false);

        auto badgeArea = chip.removeFromRight(38).withSizeKeepingCentre(28, 28);
        if (profileBadgeImage.isValid())
            g.drawImageWithin(profileBadgeImage, badgeArea.getX(), badgeArea.getY(), badgeArea.getWidth(), badgeArea.getHeight(),
                              juce::RectanglePlacement::centred, false);
        else
        {
            g.setColour(juce::Colour(0xfff2cc60));
            g.setFont(juce::Font(12.0f).boldened());
            g.drawText("★", badgeArea, juce::Justification::centred, false);
        }
    }
}

void MainComponent::TransportBar::mouseDown(const juce::MouseEvent& event)
{
    if (profileVisible && profileChipBounds.contains(event.getPosition()))
    {
        juce::PopupMenu menu;
        menu.addItem(1, "View profile page");
        menu.addSeparator();
        menu.addItem(2, "Log out");

        auto chipTopLeft = localPointToGlobal(profileChipBounds.getTopLeft());
        auto chipOnScreen = juce::Rectangle<int>(chipTopLeft.x,
                                                 chipTopLeft.y,
                                                 profileChipBounds.getWidth(),
                                                 profileChipBounds.getHeight());

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(chipOnScreen),
                           [this](int result)
                           {
                               if (result == 1)
                               {
                                   if (onOpenProfilePageRequested)
                                       onOpenProfilePageRequested();
                               }
                               else if (result == 2)
                               {
                                   if (onLogoutRequested)
                                       onLogoutRequested();
                               }
                           });
    }
}

void MainComponent::TransportBar::resized()
{
    auto area = getLocalBounds().reduced(18, 10);
    area.removeFromLeft(64);
    auto profileArea = area.removeFromRight(268);
    auto left = area.removeFromLeft(520);
    titleLabel.setBounds(left.removeFromTop(28));
    midiStatusLabel.setBounds(left.removeFromTop(20));
    projectButton.setBounds(left.removeFromTop(24).withWidth(260));
    audioButton.setBounds(left.removeFromTop(24).withWidth(76));
    tourButton.setBounds(left.removeFromTop(24).withWidth(72));

    statusLabel.setBounds(area.removeFromRight(260));
    recordButton.setBounds(area.removeFromRight(110));
    stopButton.setBounds(area.removeFromRight(100));
    playButton.setBounds(area.removeFromRight(100));

    auto profileContent = profileArea.reduced(12, 6);
    signInButton.setBounds(profileContent);

    auto profileTextArea = profileContent.withTrimmedLeft(48).withTrimmedRight(42);
    profileNameLabel.setBounds(profileTextArea.removeFromTop(24));
    profileDetailLabel.setBounds(profileTextArea.removeFromTop(18));
    profileChipBounds = profileArea;
    signInButton.setVisible(! profileVisible);
    projectButton.setVisible(true);
    profileNameLabel.setVisible(profileVisible);
    profileDetailLabel.setVisible(profileVisible);
}

MainComponent::PluginRackBar::PluginRackBar()
{
    setName("Plugin Rack");
    titleLabel.setText("Master Insert", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(18.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    contextLabel.setText("Master", juce::dontSendNotification);
    contextLabel.setJustificationType(juce::Justification::centredLeft);
    contextLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(contextLabel);

    pluginNameLabel.setText("No plugin loaded", juce::dontSendNotification);
    pluginNameLabel.setJustificationType(juce::Justification::centredLeft);
    pluginNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(pluginNameLabel);

    bypassButton.onClick = [this]
    {
        if (onBypassChanged)
            onBypassChanged(bypassButton.getToggleState());
    };
    addAndMakeVisible(bypassButton);

    openEditorButton.onClick = [this]
    {
        if (onOpenPluginEditor)
            onOpenPluginEditor();
    };
    addAndMakeVisible(openEditorButton);

    loadButton.onClick = [this]
    {
        if (onLoadPlugin)
            onLoadPlugin();
    };
    addAndMakeVisible(loadButton);

    unloadButton.onClick = [this]
    {
        if (onUnloadPlugin)
            onUnloadPlugin();
    };
    addAndMakeVisible(unloadButton);
}

void MainComponent::PluginRackBar::setContextMaster()
{
    context = Context::master;
    selectedTrackIndex = -1;
    titleLabel.setText("Master Insert", juce::dontSendNotification);
    contextLabel.setText("Master", juce::dontSendNotification);
}

void MainComponent::PluginRackBar::setContextTrack(int trackIndex, const juce::String& trackName)
{
    context = Context::track;
    selectedTrackIndex = trackIndex;
    titleLabel.setText("Track Insert", juce::dontSendNotification);
    contextLabel.setText(trackName.isNotEmpty() ? ("Track " + juce::String(trackIndex + 1) + " - " + trackName)
                                                : ("Track " + juce::String(trackIndex + 1)),
                         juce::dontSendNotification);
}

void MainComponent::PluginRackBar::setPluginName(const juce::String& name)
{
    pluginNameLabel.setText(name.isNotEmpty() ? "Loaded: " + name : "No plugin loaded", juce::dontSendNotification);
    hasPlugin = name.isNotEmpty();
    openEditorButton.setEnabled(hasPlugin);
    bypassButton.setEnabled(hasPlugin);
    unloadButton.setEnabled(hasPlugin);
}

void MainComponent::PluginRackBar::setBypassed(bool shouldBypass)
{
    bypassButton.setToggleState(shouldBypass, juce::dontSendNotification);
}

void MainComponent::PluginRackBar::setHasPlugin(bool shouldHavePlugin)
{
    hasPlugin = shouldHavePlugin;
    if (! hasPlugin)
    {
        openEditorButton.setEnabled(false);
        bypassButton.setEnabled(false);
        unloadButton.setEnabled(false);
    }
}

void MainComponent::PluginRackBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10141a));
    g.setColour(juce::Colour(0xff273243));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 10.0f, 1.0f);
}

void MainComponent::PluginRackBar::resized()
{
    auto area = getLocalBounds().reduced(14, 8);
    titleLabel.setBounds(area.removeFromLeft(150));
    contextLabel.setBounds(area.removeFromLeft(240));
    pluginNameLabel.setBounds(area.removeFromLeft(300));
    unloadButton.setBounds(area.removeFromRight(90));
    loadButton.setBounds(area.removeFromRight(110));
    openEditorButton.setBounds(area.removeFromRight(110));
    bypassButton.setBounds(area.removeFromRight(100));
}

MainComponent::MainComponent()
{
    deviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
    engine.attachToDevice(deviceManager);
    engine.setPlaying(false);

    setSize(1400, 900);
    addAndMakeVisible(authGateView);
    transportBarSafe = &transportBar;
    pluginRackBarSafe = &pluginRackBar;
    mixerPanelSafe = &mixerPanel;
    authGateView.onSignInRequested = [this]
    {
        authSession.beginLogin();
    };
    authGateView.onLogoutRequested = [this]
    {
        authSession.clearSession();
        authenticated = false;
        refreshAuthState();
    };
    authSession.onStatusChanged = [this](const juce::String& text)
    {
        authGateView.setStatusText(text);
        transportBar.setStatusText(text);
    };
    authSession.onError = [this](const juce::String& text)
    {
        authGateView.setStatusText(text);
        transportBar.setStatusText(text);
        authGateView.setBusy(false);
        transportBar.signInButton.setEnabled(true);
    };
    authSession.onBusyChanged = [this](bool shouldBeBusy)
    {
        authGateView.setBusy(shouldBeBusy);
        transportBar.signInButton.setEnabled(! shouldBeBusy);
    };
    authSession.onAuthenticated = [this](const DesktopAuthSession::SessionData& session)
    {
        authenticated = true;
        transportBar.setProfile(session);
        authGateView.setAccountText(session.user.displayName.isNotEmpty()
                                        ? session.user.displayName + " <" + session.user.email + ">"
                                        : session.user.email);
        authGateView.setStatusText("Signed in. Loading your workspace...");
        refreshAuthState();
    };
    authSession.onSessionCleared = [this]
    {
        authenticated = false;
        transportBar.clearProfile();
        authGateView.setAccountText("Not signed in yet.");
        authGateView.setStatusText("Session cleared.");
        refreshAuthState();
    };
    transportBar.onProjectMenuRequested = [this]
    {
        showProjectMenu();
    };
    transportBar.onAudioRequested = [this]
    {
        showAudioSettings();
    };
    transportBar.onTourRequested = [this]
    {
        showTour();
    };
    if (authSession.hasValidSession())
    {
        authenticated = true;
        const auto& session = authSession.getSession();
        transportBar.setProfile(session);
        authGateView.setAccountText(session.user.displayName.isNotEmpty()
                                        ? session.user.displayName + " <" + session.user.email + ">"
                                        : session.user.email);
        authGateView.setStatusText("Restored your saved login.");
    }

    if (! projectManager.loadLastProject())
    {
        juce::String projectError;
        projectManager.createProject("Untitled Project", projectError);
    }
    transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());

    viewModeBar.onModeSelected = [this](WorkspaceMode mode)
    {
        setWorkspaceMode(mode);
    };

    addAndMakeVisible(transportBar);
    addAndMakeVisible(viewModeBar);
    addAndMakeVisible(pluginRackBar);
    addAndMakeVisible(arrangeView);
    addAndMakeVisible(mixerPanel);
    addAndMakeVisible(graphPanel);
    addAndMakeVisible(dslPanel);
    addAndMakeVisible(recordView);
    addAndMakeVisible(aiPanel);
    addChildComponent(tourOverlay);

    pluginRackBar.setContextMaster();
    arrangeView.setTotalTrackCount(engine.getTrackCount());
    arrangeView.setVisibleTrackCount(8);
    recordView.setTrackCount(8);
    armedTracks.resize((size_t) engine.getTrackCount(), false);
    recordView.onTrackArmChanged = [this](int trackIndex, bool shouldArm)
    {
        if (juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()))
            armedTracks[(size_t) trackIndex] = shouldArm;
    };

    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        auto trackName = engine.getTrackName(index);
        arrangeView.setTrackName(index, trackName);
        if (index < 8)
            recordView.setTrackName(index, trackName);
    }

    refreshAuthState();

    transportBar.onPlay = [this]
    {
        engine.setPlaying(true);
        midiSurface.setTransportState(true, false);
    };
    transportBar.onStop = [this]
    {
        stopRecordingSession();
        engine.setPlaying(false);
        midiSurface.setTransportState(false, false);
    };
    transportBar.onRecord = [this]
    {
        if (engine.isRecording())
            stopRecordingSession();
        else if (startRecordingSession())
        {
            engine.setPlaying(true);
            midiSurface.setTransportState(true, true);
        }
    };
    transportBar.onSignInRequested = [this]
    {
        authSession.beginLogin();
    };
    transportBar.onOpenProfilePageRequested = [this]
    {
        openLagDaemonProfile();
    };
    transportBar.onLogoutRequested = [this]
    {
        authSession.clearSession();
    };

    auto refreshInsertRack = [this]
    {
        if (pluginRackBar.isTrackContext())
        {
            auto trackIndex = pluginRackBar.getTrackIndex();
            pluginRackBar.setPluginName(engine.getTrackPluginName(trackIndex));
            pluginRackBar.setBypassed(engine.isTrackPluginBypassed(trackIndex));
        }
        else
        {
            pluginRackBar.setPluginName(engine.getMasterPluginName());
            pluginRackBar.setBypassed(engine.isMasterPluginBypassed());
        }
    };

    auto refreshVisibleBank = [this, refreshInsertRack]
    {
        auto bankOffset = mixerPanel.getBankOffset();
        auto visibleCount = mixerPanel.getVisibleChannelCount();
        auto selectedTrackIndex = pluginRackBar.isTrackContext() ? pluginRackBar.getTrackIndex() : -1;

        mixerPanel.setSelectedChannel(selectedTrackIndex);

        for (int slot = 0; slot < visibleCount; ++slot)
        {
            auto trackIndex = bankOffset + slot;
            if (trackIndex >= engine.getTrackCount())
                continue;

            auto name = engine.getTrackName(trackIndex);
            auto gain = engine.getTrackGain(trackIndex);
            auto pan = engine.getTrackPan(trackIndex);
            auto muted = engine.isTrackMuted(trackIndex);
            auto soloed = engine.isTrackSoloed(trackIndex);
            auto pluginName = engine.getTrackPluginName(trackIndex);
            auto pluginBypassed = engine.isTrackPluginBypassed(trackIndex);

            mixerPanel.setChannelName(trackIndex, name);
            mixerPanel.setChannelInsertName(trackIndex, pluginName.isNotEmpty() ? ("FX: " + pluginName) : "FX: none");
            mixerPanel.setChannelInsertBypassed(trackIndex, pluginBypassed);
            mixerPanel.setChannelGain(trackIndex, gain);
            mixerPanel.setChannelPan(trackIndex, pan);
            mixerPanel.setChannelMuted(trackIndex, muted);
            mixerPanel.setChannelSoloed(trackIndex, soloed);

            midiSurface.setChannelName(trackIndex, name);
            midiSurface.setChannelGain(trackIndex, gain);
            midiSurface.setChannelPan(trackIndex, pan);
            midiSurface.setChannelMuted(trackIndex, muted);
            midiSurface.setChannelSoloed(trackIndex, soloed);
        }

        auto masterGain = engine.getMasterGain();
        mixerPanel.setMasterGain(masterGain);
        midiSurface.setMasterFaderValue(masterGain);
        midiSurface.setBankOffset(bankOffset);
        midiSurface.refreshVisibleWindow();
        refreshInsertRack();
    };

    auto selectTrack = [this, refreshVisibleBank](int trackIndex)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        auto visibleStart = mixerPanel.getBankOffset();
        auto visibleEnd = visibleStart + mixerPanel.getVisibleChannelCount();
        if (trackIndex < visibleStart || trackIndex >= visibleEnd)
            mixerPanel.setBankOffset((trackIndex / mixerPanel.getVisibleChannelCount()) * mixerPanel.getVisibleChannelCount());

        pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
        mixerPanel.setSelectedChannel(trackIndex);
        arrangeView.setSelectedTrack(trackIndex);
        refreshVisibleBank();
    };

    arrangeView.onTrackSelected = [selectTrack](int trackIndex)
    {
        selectTrack(trackIndex);
    };

    arrangeView.onAddTrackRequested = [this]
    {
        recordView.setTrackCount(arrangeView.getVisibleTrackCount());
        for (int index = 0; index < arrangeView.getVisibleTrackCount(); ++index)
            recordView.setTrackName(index, engine.getTrackName(index));
    };

    pluginRackBar.onLoadPlugin = [this, refreshVisibleBank]
    {
        if (pluginEditorWindow != nullptr)
            pluginEditorWindow.reset();

        pluginChooser = std::make_unique<juce::FileChooser>("Load a VST3 plugin", juce::File{}, "*.vst3;*.dll");
        pluginChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                   [this, refreshVisibleBank](const juce::FileChooser& chooser)
                                   {
                                       auto file = chooser.getResult();
                                       pluginChooser.reset();

                                       if (! file.existsAsFile() && ! file.isDirectory())
                                           return;

                                       juce::String errorMessage;
                                       auto loaded = pluginRackBar.isTrackContext()
                                           ? engine.loadTrackPlugin(pluginRackBar.getTrackIndex(), file, errorMessage)
                                           : engine.loadMasterPlugin(file, errorMessage);

                                       if (loaded)
                                       {
                                           refreshVisibleBank();
                                       }
                                       else
                                       {
                                           transportBar.setStatusText("Plugin load failed: " + errorMessage);
                                       }
                                   });
    };

    pluginRackBar.onUnloadPlugin = [this, refreshVisibleBank]
    {
        if (pluginEditorWindow != nullptr)
            pluginEditorWindow.reset();

        if (pluginRackBar.isTrackContext())
            engine.unloadTrackPlugin(pluginRackBar.getTrackIndex());
        else
            engine.unloadMasterPlugin();

        refreshVisibleBank();
    };

    pluginRackBar.onBypassChanged = [this, refreshVisibleBank](bool shouldBypass)
    {
        if (pluginRackBar.isTrackContext())
            engine.setTrackPluginBypassed(pluginRackBar.getTrackIndex(), shouldBypass);
        else
            engine.setMasterPluginBypassed(shouldBypass);

        refreshVisibleBank();
    };

    pluginRackBar.onOpenPluginEditor = [this]
    {
        const auto isTrackContext = pluginRackBar.isTrackContext();
        const auto hasPlugin = isTrackContext ? engine.hasTrackPlugin(pluginRackBar.getTrackIndex())
                                              : engine.hasMasterPlugin();

        if (! hasPlugin)
            return;

        if (pluginEditorWindow != nullptr)
        {
            pluginEditorWindow->toFront(true);
            return;
        }

        auto* editor = isTrackContext ? engine.createTrackPluginEditor(pluginRackBar.getTrackIndex())
                                      : engine.createMasterPluginEditor();

        if (editor != nullptr)
        {
            auto windowTitle = isTrackContext ? ("Track " + juce::String(pluginRackBar.getTrackIndex() + 1) + " Editor")
                                              : "Master Editor";
            auto window = std::make_unique<juce::DocumentWindow>(windowTitle,
                                                                 juce::Colour(0xff11151c),
                                                                 juce::DocumentWindow::closeButton);
            window->setUsingNativeTitleBar(true);
            window->setResizable(true, true);
            window->setContentOwned(editor, true);
            window->centreWithSize(900, 650);
            window->setVisible(true);
            pluginEditorWindow = std::move(window);
        }
    };

    graphPanel.onEnabledChanged = [this](bool shouldEnable)
    {
        engine.setGraphEnabled(shouldEnable);
    };

    graphPanel.onInputChanged = [this](float amount)
    {
        engine.setGraphInput(amount);
    };

    graphPanel.onDriveChanged = [this](float amount)
    {
        engine.setGraphDrive(amount);
    };

    graphPanel.onToneChanged = [this](float amount)
    {
        engine.setGraphTone(amount);
    };

    graphPanel.onEchoChanged = [this](float amount)
    {
        engine.setGraphEcho(amount);
    };

    graphPanel.onWidthChanged = [this](float amount)
    {
        engine.setGraphWidth(amount);
    };

    mixerPanel.onGainChanged = [this](int index, float value)
    {
        if (index == 8)
            engine.setMasterGain(value);
        else
            engine.setTrackGain(index, value);

        if (index == 8)
            midiSurface.setMasterFaderValue(value);
        else
            midiSurface.setChannelGain(index, value);
    };

    mixerPanel.onBankOffsetChanged = [this, refreshVisibleBank](int)
    {
        refreshVisibleBank();
    };

    mixerPanel.onInsertButtonClicked = [this, refreshVisibleBank](int trackIndex)
    {
        pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
        if (pluginEditorWindow != nullptr)
            pluginEditorWindow.reset();
        refreshVisibleBank();
    };

    mixerPanel.onPanChanged = [this](int index, float value)
    {
        if (index < engine.getTrackCount())
            engine.setTrackPan(index, value);

        midiSurface.setChannelPan(index, value);
    };

    mixerPanel.onMuteChanged = [this](int index, bool muted)
    {
        if (index < engine.getTrackCount())
            engine.setTrackMuted(index, muted);

        midiSurface.setChannelMuted(index, muted);
    };

    mixerPanel.onSoloChanged = [this](int index, bool soloed)
    {
        if (index < engine.getTrackCount())
            engine.setTrackSoloed(index, soloed);

        midiSurface.setChannelSoloed(index, soloed);
    };

    midiSurface.onBankStep = [this, refreshVisibleBank](int step)
    {
        mixerPanel.setBankOffset(mixerPanel.getBankOffset() + step);
        refreshVisibleBank();
    };

    midiSurface.onChannelSelected = [this, selectTrack](int trackIndex)
    {
        selectTrack(trackIndex);
    };

    midiSurface.onSpecialButtonPressed = [this, selectTrack, refreshVisibleBank](const juce::String& button)
    {
        auto advanceMode = [this](int step)
        {
            auto modeIndex = static_cast<int>(activeMode);
            modeIndex = (modeIndex + step + 6) % 6;
            setWorkspaceMode(static_cast<WorkspaceMode>(modeIndex));
        };

        if (button == "cursor_left")
        {
            auto trackIndex = pluginRackBar.isTrackContext() ? pluginRackBar.getTrackIndex() - 1 : mixerPanel.getBankOffset();
            selectTrack(juce::jmax(0, trackIndex));
        }
        else if (button == "cursor_right")
        {
            auto trackIndex = pluginRackBar.isTrackContext() ? pluginRackBar.getTrackIndex() + 1 : mixerPanel.getBankOffset();
            selectTrack(juce::jmin(engine.getTrackCount() - 1, trackIndex));
        }
        else if (button == "cursor_up")
        {
            advanceMode(-1);
        }
        else if (button == "cursor_down")
        {
            advanceMode(1);
        }
        else if (button == "zoom")
        {
            if (pluginRackBar.isTrackContext() && engine.hasTrackPlugin(pluginRackBar.getTrackIndex()))
            {
                if (pluginEditorWindow != nullptr)
                    pluginEditorWindow.reset();

                auto* editor = engine.createTrackPluginEditor(pluginRackBar.getTrackIndex());
                if (editor != nullptr)
                {
                    auto window = std::make_unique<juce::DocumentWindow>("Track Editor",
                                                                         juce::Colour(0xff11151c),
                                                                         juce::DocumentWindow::closeButton);
                    window->setUsingNativeTitleBar(true);
                    window->setResizable(true, true);
                    window->setContentOwned(editor, true);
                    window->centreWithSize(900, 650);
                    window->setVisible(true);
                    pluginEditorWindow = std::move(window);
                }
            }
            else
            {
                setWorkspaceMode(WorkspaceMode::node);
            }
        }
        else if (button == "scrub")
        {
            engine.setGraphEnabled(! engine.isGraphEnabled());
            graphPanel.setEnabled(engine.isGraphEnabled());
            setWorkspaceMode(WorkspaceMode::node);
        }
        else if (button == "user_a")
        {
            saveSessionToDisk();
            transportBar.setStatusText("Session saved from X-Touch");
        }
        else if (button == "user_b")
        {
            loadSessionFromDisk();
            refreshVisibleBank();
            transportBar.setStatusText("Session reloaded from X-Touch");
        }
    };

    midiSurface.onFaderMoved = [this](int index, float value)
    {
        engine.setTrackGain(index, value);

        midiSurface.setChannelGain(index, value);

        auto safePanel = mixerPanelSafe;
        if (safePanel != nullptr)
        {
            juce::MessageManager::callAsync([safePanel, index, value]
            {
                if (safePanel != nullptr)
                    safePanel->setChannelGain(index, value);
            });
        }
    };

    midiSurface.onMasterFaderMoved = [this](float value)
    {
        engine.setMasterGain(value);
        mixerPanel.setMasterGain(value);

        auto safeBar = transportBarSafe;
        if (safeBar != nullptr)
        {
            juce::MessageManager::callAsync([safeBar, value]
            {
                if (safeBar != nullptr)
                    safeBar->setStatusText("Master: " + juce::String(value, 2));
            });
        }
    };

    midiSurface.onPanMoved = [this](int index, float value)
    {
        if (index < engine.getTrackCount())
            engine.setTrackPan(index, value);

        auto safePanel = mixerPanelSafe;
        if (safePanel != nullptr)
        {
            juce::MessageManager::callAsync([safePanel, index, value]
            {
                if (safePanel != nullptr)
                    safePanel->setChannelPan(index, value);
            });
        }
    };

    midiSurface.onMuteChanged = [this](int index, bool muted)
    {
        if (index < engine.getTrackCount())
            engine.setTrackMuted(index, muted);

        auto safePanel = mixerPanelSafe;
        if (safePanel != nullptr)
        {
            juce::MessageManager::callAsync([safePanel, index, muted]
            {
                if (safePanel != nullptr)
                    safePanel->setChannelMuted(index, muted);
            });
        }
    };

    midiSurface.onSoloChanged = [this](int index, bool soloed)
    {
        if (index < engine.getTrackCount())
            engine.setTrackSoloed(index, soloed);

        auto safePanel = mixerPanelSafe;
        if (safePanel != nullptr)
        {
            juce::MessageManager::callAsync([safePanel, index, soloed]
            {
                if (safePanel != nullptr)
                    safePanel->setChannelSoloed(index, soloed);
            });
        }
    };

    midiSurface.onTransportCommand = [this](XTouchControlSurface::TransportCommand command)
    {
        auto safeBar = transportBarSafe;

        switch (command)
        {
            case XTouchControlSurface::TransportCommand::play:
                engine.setPlaying(true);
                midiSurface.setTransportState(true, false);
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                            safeBar->setStatusText("Transport: play");
                    });
                break;
            case XTouchControlSurface::TransportCommand::stop:
                stopRecordingSession();
                engine.setPlaying(false);
                midiSurface.setTransportState(false, false);
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                            safeBar->setStatusText("Transport: stop");
                    });
                break;
            case XTouchControlSurface::TransportCommand::record:
                if (engine.isRecording())
                {
                    stopRecordingSession();
                }
                else if (startRecordingSession())
                {
                    engine.setPlaying(true);
                    midiSurface.setTransportState(true, true);
                    if (safeBar != nullptr)
                        juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                            safeBar->setStatusText("Transport: record armed");
                    });
                }
                break;
            case XTouchControlSurface::TransportCommand::rewind:
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                            safeBar->setStatusText("Transport: rewind");
                    });
                break;
            case XTouchControlSurface::TransportCommand::fastForward:
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                            safeBar->setStatusText("Transport: fast forward");
                    });
                break;
        }
    };

    midiSurface.onStatusMessage = [this](juce::String text)
    {
        auto safeBar = transportBarSafe;
        if (safeBar != nullptr)
        {
            juce::MessageManager::callAsync([safeBar, text = std::move(text)]
            {
                if (safeBar != nullptr)
                    safeBar->setMidiStatusText(text);
            });
        }
    };

    midiSurface.attachToDeviceManager(deviceManager);
    midiSurface.setTrackCount(engine.getTrackCount());
    midiSurface.setBankOffset(0);

    mixerPanel.setChannelCount(engine.getTrackCount());
    for (int index = 0; index < engine.getTrackCount(); ++index)
        mixerPanel.setChannelName(index, engine.getTrackName(index));

    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        const auto gain = index == 1 ? 0.18f : index == 2 ? 0.22f : 0.12f;
        const auto pan = index == 1 ? -0.15f : index == 2 ? 0.12f : 0.0f;
        mixerPanel.setChannelGain(index, gain);
        mixerPanel.setChannelPan(index, pan);
        engine.setTrackGain(index, gain);
        engine.setTrackPan(index, pan);
    }

    mixerPanel.setMasterGain(0.5f);
    engine.setMasterGain(0.5f);
    graphPanel.setEnabled(engine.isGraphEnabled());
    graphPanel.setInput(engine.getGraphInput());
    graphPanel.setDrive(engine.getGraphDrive());
    graphPanel.setTone(engine.getGraphTone());
    graphPanel.setEcho(engine.getGraphEcho());
    graphPanel.setWidth(engine.getGraphWidth());
    midiSurface.setMasterFaderValue(0.5f);
    engine.setPlaying(false);
    midiSurface.setTransportState(false, false);

    loadSessionFromDisk();
    refreshVisibleBank();
    refreshInsertRack();
    refreshRecentTakes();

    tourOverlay.setSteps(
        {
            {
                "Welcome",
                "This tour gives you a quick map of the app. Click Next to move through the basics, or click the highlighted areas when you want the walkthrough to advance.",
                nullptr,
                false
            },
            {
                "Transport",
                "Use Play, Stop, and Record here. This is the fastest place to control playback.",
                [this] { return transportBar.getBounds(); },
                true
            },
            {
                "Workspaces",
                "These tabs switch between the DAW, mixer, node graph, DSL, record view, and AI workspace.",
                [this] { return viewModeBar.getBounds(); },
                true
            },
            {
                "Mixer",
                "The mixer is where channels, banks, mute, solo, and the master strip live.",
                [this] { return mixerPanel.getBounds(); },
                false
            },
            {
                "Node Graph",
                "This view is for sound design chains: sources, effects, and sinks.",
                [this] { return graphPanel.getBounds(); },
                false
            },
            {
                "DSL",
                "The DSL lets you write functional-style signal flow and automation as text.",
                [this] { return dslPanel.getBounds(); },
                false
            },
            {
                "Record",
                "Record mode arms tracks and writes audio takes into your project folder.",
                [this] { return recordView.getBounds(); },
                false
            },
            {
                "Done",
                "That’s the basic map. Open this tour again from the Tour button anytime.",
                [this] { return transportBar.getBounds(); },
                false
            }
        });
}

MainComponent::~MainComponent()
{
    saveSessionToDisk();
    pluginEditorWindow.reset();
    midiSurface.detachFromDeviceManager(deviceManager);
    engine.detachFromDevice(deviceManager);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f1115));
    g.setColour(juce::Colour(0xff1c2230));
    auto area = getLocalBounds().toFloat().reduced(18.0f);
    g.fillRoundedRectangle(area, 24.0f);
    g.setColour(juce::Colour(0xff2a3244));
    g.drawRoundedRectangle(area, 24.0f, 1.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    auto transportArea = area.removeFromTop(72);
    transportBar.setBounds(transportArea);

    auto pluginArea = area.removeFromTop(48);
    pluginRackBar.setBounds(pluginArea);

    auto modeArea = area.removeFromTop(42);
    viewModeBar.setBounds(modeArea);

    auto contentArea = area;
    arrangeView.setBounds(contentArea);
    mixerPanel.setBounds(contentArea);
    graphPanel.setBounds(contentArea);
    dslPanel.setBounds(contentArea);
    recordView.setBounds(contentArea);
    aiPanel.setBounds(contentArea);
    authGateView.setBounds(getLocalBounds());
    tourOverlay.setBounds(getLocalBounds());
}

void MainComponent::setWorkspaceMode(WorkspaceMode mode)
{
    activeMode = mode;
    viewModeBar.setActiveMode(mode);
    refreshModeVisibility();
}

void MainComponent::refreshModeVisibility()
{
    transportBar.setVisible(true);
    viewModeBar.setVisible(true);
    pluginRackBar.setVisible(true);
    arrangeView.setVisible(activeMode == WorkspaceMode::arrange);
    mixerPanel.setVisible(activeMode == WorkspaceMode::mix);
    graphPanel.setVisible(activeMode == WorkspaceMode::node);
    dslPanel.setVisible(activeMode == WorkspaceMode::code);
    recordView.setVisible(activeMode == WorkspaceMode::record);
    aiPanel.setVisible(activeMode == WorkspaceMode::ai);
    authGateView.setVisible(false);
    if (tourOverlay.isActive())
        tourOverlay.toFront(true);
}

void MainComponent::refreshAuthState()
{
    refreshModeVisibility();

    if (authenticated)
    {
        const auto& session = authSession.getSession();
        transportBar.setProfile(session);
        transportBar.setStatusText("Signed in. Welcome back.");
    }
    else
    {
        transportBar.clearProfile();
        transportBar.setStatusText("Ready. Sign in from the top-right when you want sync.");
    }
}

void MainComponent::openLagDaemonProfile()
{
    juce::URL("https://lagdaemon.com/profile").launchInDefaultBrowser();
}

void MainComponent::showAudioSettings()
{
    if (audioDeviceWindow != nullptr)
    {
        audioDeviceWindow->toFront(true);
        return;
    }

    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager,
                                                                         0, 2,
                                                                         0, 2,
                                                                         true, true, true, false);

    auto window = std::make_unique<juce::DocumentWindow>("Audio Devices",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::closeButton);
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setContentOwned(selector.release(), true);
    window->centreWithSize(720, 540);
    window->setVisible(true);
    audioDeviceWindow = std::move(window);
}

void MainComponent::showTour()
{
    tourOverlay.start();
    tourOverlay.toFront(true);
}

void MainComponent::showProjectMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "New Project");
    menu.addItem(2, "Open Project…");
    menu.addSeparator();
    menu.addItem(3, "Save Project");
    menu.addItem(4, "Open Project Folder");

    auto screenArea = transportBar.getScreenBounds();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(screenArea),
                       [this](int result)
                       {
                           switch (result)
                           {
                               case 1: createNewProject(); break;
                               case 2: openProject(); break;
                               case 3: saveProject(); break;
                               case 4: revealProjectFolder(); break;
                               default: break;
                           }
                       });
}

void MainComponent::createNewProject()
{
    auto* nameEditor = new juce::AlertWindow("New Project",
                                             "Give the project a name.",
                                             juce::MessageBoxIconType::QuestionIcon);
    nameEditor->addTextEditor("projectName", "Untitled Project");
    nameEditor->addButton("Create", 1);
    nameEditor->addButton("Cancel", 0);

    auto options = juce::Component::SafePointer<MainComponent>(this);
    nameEditor->enterModalState(true, juce::ModalCallbackFunction::create([options, nameEditor](int result) mutable
    {
        std::unique_ptr<juce::AlertWindow> dialog(nameEditor);
        if (result != 1 || options == nullptr)
            return;

        auto name = dialog->getTextEditorContents("projectName").trim();
        juce::String errorMessage;
        if (! options->projectManager.createProject(name, errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Project Error",
                                                   errorMessage);
            return;
        }

        options->transportBar.setProjectLabel("Project: " + options->projectManager.getDisplayLabel());
        options->saveSessionToDisk();
    }), true);
}

void MainComponent::openProject()
{
    projectChooser = std::make_unique<juce::FileChooser>("Open a Creation Station project",
                                                         projectManager.getProjectsRoot(),
                                                         "*",
                                                         true);

    auto chooser = projectChooser.get();
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [this, chooser](const juce::FileChooser& result)
                         {
                             auto directory = result.getResult();
                             if (! directory.exists())
                                 return;

                             juce::String errorMessage;
                             if (! projectManager.openProject(directory, errorMessage))
                             {
                                 juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                                        "Project Error",
                                                                        errorMessage);
                                 return;
                             }

                             transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
                             loadSessionFromDisk();
                             saveSessionToDisk();
                         });
}

void MainComponent::saveProject()
{
    if (! projectManager.hasProject())
    {
        createNewProject();
        return;
    }

    saveSessionToDisk();
    transportBar.setStatusText("Project saved.");
}

juce::String MainComponent::createRecordingTakeName() const
{
    return "Take-" + makeRecordingTimestamp() + ".wav";
}

void MainComponent::refreshRecentTakes()
{
    if (! projectManager.hasProject())
    {
        recordView.setRecentTakes({});
        arrangeView.setRecordedClips({});
        return;
    }

    auto audioDirectory = projectManager.getCurrentProject().audioDirectory;
    juce::Array<juce::File> takeFiles;
    audioDirectory.findChildFiles(takeFiles, juce::File::findFiles, false, "*.wav");

    juce::StringArray names;
    for (int index = 0; index < juce::jmin(10, takeFiles.size()); ++index)
        names.add(takeFiles[(size_t) index].getFileName());

    recordView.setRecentTakes(names);
    arrangeView.setRecordedClips(names);
}

bool MainComponent::startRecordingSession()
{
    if (! projectManager.hasProject())
    {
        juce::String errorMessage;
        if (! projectManager.createProject("Untitled Project", errorMessage))
        {
            transportBar.setStatusText("Could not create a project for recording.");
            return false;
        }
    }

    auto takeFile = projectManager.getCurrentProject().audioDirectory.getChildFile(createRecordingTakeName());
    juce::String errorMessage;
    if (! engine.startRecordingToFile(takeFile, errorMessage))
    {
        transportBar.setStatusText("Record failed: " + errorMessage);
        return false;
    }

    transportBar.setStatusText("Recording: " + takeFile.getFileName());
    recordView.setRecordingState(true, takeFile.getFileName());
    refreshRecentTakes();
    return true;
}

void MainComponent::stopRecordingSession()
{
    if (! engine.isRecording())
        return;

    auto takeName = engine.getRecordingFile().getFileName();
    engine.stopRecording();
    midiSurface.setTransportState(false, false);
    transportBar.setStatusText("Recording stopped.");
    recordView.setRecordingState(false, takeName);
    refreshRecentTakes();
}

void MainComponent::revealProjectFolder()
{
    if (! projectManager.hasProject())
        return;

    projectManager.getCurrentProject().rootDirectory.revealToUser();
}



juce::File MainComponent::getSessionFile() const
{
    auto sessionDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                               .getChildFile("CreationStation");
    return sessionDirectory.getChildFile("session.xml");
}

void MainComponent::saveSessionToDisk() const
{
    auto state = engine.createSessionState();
    state.setProperty("bankOffset", mixerPanel.getBankOffset(), nullptr);
    state.setProperty("insertContext", pluginRackBar.isTrackContext() ? "track" : "master", nullptr);
    state.setProperty("insertTrackIndex", pluginRackBar.getTrackIndex(), nullptr);
    state.setProperty("graphEnabled", engine.isGraphEnabled(), nullptr);
    state.setProperty("graphInput", engine.getGraphInput(), nullptr);
    state.setProperty("graphDrive", engine.getGraphDrive(), nullptr);
    state.setProperty("graphTone", engine.getGraphTone(), nullptr);
    state.setProperty("graphEcho", engine.getGraphEcho(), nullptr);
    state.setProperty("graphWidth", engine.getGraphWidth(), nullptr);
    state.setProperty("arrangeVisibleTracks", arrangeView.getVisibleTrackCount(), nullptr);
    state.setProperty("workspaceMode", static_cast<int>(activeMode), nullptr);
    state.setProperty("dslSource", dslPanel.getSourceText(), nullptr);
    state.addChild(graphPanel.createState(), -1, nullptr);

    projectManager.saveProjectState(state);

    auto sessionFile = getSessionFile();
    sessionFile.getParentDirectory().createDirectory();
    if (auto xml = state.createXml())
        xml->writeTo(sessionFile);
}

void MainComponent::loadSessionFromDisk()
{
    auto state = projectManager.loadProjectState();
    if (! state.isValid())
    {
        auto sessionFile = getSessionFile();
        if (! sessionFile.existsAsFile())
            return;

        auto xml = juce::parseXML(sessionFile);
        if (xml == nullptr)
            return;

        state = juce::ValueTree::fromXml(*xml);
    }

    juce::String errorMessage;
    engine.restoreSessionState(state, errorMessage);

    auto bankOffset = (int) state.getProperty("bankOffset", 0);
    mixerPanel.setBankOffset(bankOffset);
    midiSurface.setBankOffset(bankOffset);

    auto insertContext = state.getProperty("insertContext").toString();
    auto trackIndex = (int) state.getProperty("insertTrackIndex", -1);

    if (insertContext == "track" && juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
    {
        pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
        mixerPanel.setSelectedChannel(trackIndex);
    }
    else
    {
        pluginRackBar.setContextMaster();
        mixerPanel.setSelectedChannel(-1);
    }

    engine.setGraphEnabled((bool) state.getProperty("graphEnabled", true));
    engine.setGraphInput((float) state.getProperty("graphInput", engine.getGraphInput()));
    engine.setGraphDrive((float) state.getProperty("graphDrive", engine.getGraphDrive()));
    engine.setGraphTone((float) state.getProperty("graphTone", engine.getGraphTone()));
    engine.setGraphEcho((float) state.getProperty("graphEcho", engine.getGraphEcho()));
    engine.setGraphWidth((float) state.getProperty("graphWidth", engine.getGraphWidth()));
    graphPanel.setEnabled(engine.isGraphEnabled());
    graphPanel.setInput(engine.getGraphInput());
    graphPanel.setDrive(engine.getGraphDrive());
    graphPanel.setTone(engine.getGraphTone());
    graphPanel.setEcho(engine.getGraphEcho());
    graphPanel.setWidth(engine.getGraphWidth());

    if (auto dslSource = state.getProperty("dslSource").toString(); dslSource.isNotEmpty())
        dslPanel.setSourceText(dslSource);

    if (auto graphState = state.getChildWithName("NodeGraph"); graphState.isValid())
        graphPanel.restoreState(graphState);

    auto visibleTracks = (int) state.getProperty("arrangeVisibleTracks", arrangeView.getVisibleTrackCount());
    arrangeView.setVisibleTrackCount(juce::jlimit(1, engine.getTrackCount(), visibleTracks));
    recordView.setTrackCount(arrangeView.getVisibleTrackCount());

    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        auto trackName = engine.getTrackName(index);
        arrangeView.setTrackName(index, trackName);
        if (index < 8)
            recordView.setTrackName(index, trackName);
    }

    auto savedMode = (int) state.getProperty("workspaceMode", static_cast<int>(WorkspaceMode::arrange));
    setWorkspaceMode(static_cast<WorkspaceMode>(juce::jlimit(0, 5, savedMode)));

    refreshInsertRack();
    transportBar.setProjectLabel("Project: " + projectManager.getDisplayLabel());
    refreshRecentTakes();

}

void MainComponent::refreshInsertRack()
{
    if (pluginRackBar.isTrackContext())
    {
        auto trackIndex = pluginRackBar.getTrackIndex();
        pluginRackBar.setPluginName(engine.getTrackPluginName(trackIndex));
        pluginRackBar.setBypassed(engine.isTrackPluginBypassed(trackIndex));
    }
    else
    {
        pluginRackBar.setPluginName(engine.getMasterPluginName());
        pluginRackBar.setBypassed(engine.isMasterPluginBypassed());
    }
}
