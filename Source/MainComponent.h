#pragma once

#include <JuceHeader.h>
#include "Audio/WorkstationAudioEngine.h"
#include "Views/AiPanel.h"
#include "Views/DslPanel.h"
#include "Views/GraphPanel.h"
#include "Views/MixerPanel.h"

class MainComponent final : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class TransportBar final : public juce::Component
    {
    public:
        TransportBar();

        std::function<void()> onPlay;
        std::function<void()> onStop;
        std::function<void()> onRecord;

        void resized() override;
        void paint(juce::Graphics&) override;

        juce::Label titleLabel;
        juce::TextButton playButton { "Play" };
        juce::TextButton stopButton { "Stop" };
        juce::TextButton recordButton { "Record" };
        juce::Label statusLabel;
    };

    juce::AudioDeviceManager deviceManager;
    WorkstationAudioEngine engine;
    TransportBar transportBar;
    juce::TabbedComponent tabbedWorkspace { juce::TabbedButtonBar::TabsAtTop };
    MixerPanel mixerPanel;
    GraphPanel graphPanel;
    DslPanel dslPanel;
    AiPanel aiPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
