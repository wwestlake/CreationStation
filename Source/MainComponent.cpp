#include "MainComponent.h"

MainComponent::TransportBar::TransportBar()
{
    titleLabel.setText("Creative Workstation", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(juce::FontOptions(28.0f)).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

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
    titleLabel.setBounds(area.removeFromLeft(420));
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

    transportBar.onPlay = [this] { engine.setPlaying(true); };
    transportBar.onStop = [this] { engine.setPlaying(false); };
    transportBar.onRecord = [this]
    {
        engine.setPlaying(true);
        transportBar.statusLabel.setText("Transport: record armed (demo tone)", juce::dontSendNotification);
    };

    tabbedWorkspace.addTab("Mixer", juce::Colour(0xff171a21), &mixerPanel, false);
    tabbedWorkspace.addTab("Node Graph", juce::Colour(0xff13171d), &graphPanel, false);
    tabbedWorkspace.addTab("DSL", juce::Colour(0xff11151c), &dslPanel, false);
    tabbedWorkspace.addTab("AI", juce::Colour(0xff141820), &aiPanel, false);
    addAndMakeVisible(tabbedWorkspace);

    mixerPanel.onGainChanged = [this](int index, float value)
    {
        if (index == 4)
            engine.setMasterGain(value);
        else
            engine.setTrackGain(index, value);
    };

    mixerPanel.onPanChanged = [this](int index, float value)
    {
        if (index < 4)
            engine.setTrackPan(index, value);
    };

    mixerPanel.onMuteChanged = [this](int index, bool muted)
    {
        if (index < 4)
            engine.setTrackMuted(index, muted);
    };

    mixerPanel.onSoloChanged = [this](int index, bool soloed)
    {
        if (index < 4)
            engine.setTrackSoloed(index, soloed);
    };

    // Seed the UI with the engine defaults so the faders and DSP state agree.
    for (int index = 0; index < 5; ++index)
    {
        const auto gain = index == 4 ? 0.8f : 0.75f;
        if (index == 4)
            mixerPanel.onGainChanged(index, gain);
        else
        {
            mixerPanel.onGainChanged(index, gain);
            mixerPanel.onPanChanged(index, index == 1 ? -0.15f : index == 2 ? 0.1f : 0.0f);
        }
    }
}

MainComponent::~MainComponent()
{
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
