#include "MainComponent.h"

MainComponent::TransportBar::TransportBar()
{
    titleLabel.setText("Creative Workstation", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(juce::FontOptions(28.0f)).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    midiStatusLabel.setText("MIDI: waiting for controller", juce::dontSendNotification);
    midiStatusLabel.setJustificationType(juce::Justification::centredLeft);
    midiStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(midiStatusLabel);

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
}

void MainComponent::TransportBar::setStatusText(const juce::String& text)
{
    statusLabel.setText(text, juce::dontSendNotification);
}

void MainComponent::TransportBar::setMidiStatusText(const juce::String& text)
{
    midiStatusLabel.setText(text, juce::dontSendNotification);
}

void MainComponent::TransportBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f1115));
    g.setColour(juce::Colour(0xff242a36));
    g.drawLine(0.0f,
               static_cast<float>(getHeight()) - 1.0f,
               static_cast<float>(getWidth()),
               static_cast<float>(getHeight()) - 1.0f,
               1.0f);
}

void MainComponent::TransportBar::resized()
{
    auto area = getLocalBounds().reduced(18, 10);
    auto left = area.removeFromLeft(420);
    titleLabel.setBounds(left.removeFromTop(28));
    midiStatusLabel.setBounds(left.removeFromTop(20));
    statusLabel.setBounds(area.removeFromRight(260));
    recordButton.setBounds(area.removeFromRight(110));
    stopButton.setBounds(area.removeFromRight(100));
    playButton.setBounds(area.removeFromRight(100));
}

MainComponent::MainComponent()
{
    deviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
    engine.attachToDevice(deviceManager);

    setSize(1400, 900);

    transportBar.setSize(1400, 64);
    addAndMakeVisible(transportBar);
    transportBarSafe = &transportBar;
    mixerPanelSafe = &mixerPanel;

    transportBar.onPlay = [this] { engine.setPlaying(true); };
    transportBar.onStop = [this] { engine.setPlaying(false); };
    transportBar.onRecord = [this]
    {
        engine.setPlaying(true);
        transportBar.setStatusText("Transport: record armed (demo tone)");
    };

    tabbedWorkspace.addTab("Mixer", juce::Colour(0xff171a21), &mixerPanel, false);
    tabbedWorkspace.addTab("Node Graph", juce::Colour(0xff13171d), &graphPanel, false);
    tabbedWorkspace.addTab("DSL", juce::Colour(0xff11151c), &dslPanel, false);
    tabbedWorkspace.addTab("AI", juce::Colour(0xff141820), &aiPanel, false);
    addAndMakeVisible(tabbedWorkspace);

    mixerPanel.onGainChanged = [this](int index, float value)
    {
        if (index == 8)
            engine.setMasterGain(value);
        else
            engine.setTrackGain(index, value);
    };

    mixerPanel.onPanChanged = [this](int index, float value)
    {
        if (index < 8)
            engine.setTrackPan(index, value);
    };

    mixerPanel.onMuteChanged = [this](int index, bool muted)
    {
        if (index < 8)
            engine.setTrackMuted(index, muted);
    };

    mixerPanel.onSoloChanged = [this](int index, bool soloed)
    {
        if (index < 8)
            engine.setTrackSoloed(index, soloed);
    };

    midiSurface.onFaderMoved = [this](int index, float value)
    {
        if (index == 8)
            engine.setMasterGain(value);
        else
            engine.setTrackGain(index, value);

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

    midiSurface.onPanMoved = [this](int index, float value)
    {
        if (index < 8)
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
        if (index < 8)
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
        if (index < 8)
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
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                            safeBar->setStatusText("Transport: play");
                    });
                break;
            case XTouchControlSurface::TransportCommand::stop:
                engine.setPlaying(false);
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                            safeBar->setStatusText("Transport: stop");
                    });
                break;
            case XTouchControlSurface::TransportCommand::record:
                engine.setPlaying(true);
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                            safeBar->setStatusText("Transport: record armed");
                    });
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

    for (int index = 0; index < 8; ++index)
    {
        const auto gain = index == 1 ? 0.7f : index == 2 ? 0.78f : 0.85f;
        mixerPanel.setChannelGain(index, gain);
        mixerPanel.setChannelPan(index, index == 1 ? -0.15f : index == 2 ? 0.12f : 0.0f);
        engine.setTrackGain(index, gain);
    }

    mixerPanel.setChannelGain(8, 0.8f);
    engine.setMasterGain(0.8f);
}

MainComponent::~MainComponent()
{
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
    transportBar.setBounds(area.removeFromTop(64));
    tabbedWorkspace.setBounds(area.reduced(12));
}
