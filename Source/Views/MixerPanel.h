#pragma once

#include <JuceHeader.h>

class WorkstationAudioEngine;

class MixerPanel final : public juce::Component
{
public:
    MixerPanel();

    std::function<void(int, float)> onGainChanged;
    std::function<void(int, float)> onPanChanged;
    std::function<void(int, bool)> onMuteChanged;
    std::function<void(int, bool)> onSoloChanged;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ChannelStrip final : public juce::Component
    {
    public:
        ChannelStrip(int channelIndex, const juce::String& channelName);

        void setGain(float gain);
        void setPan(float pan);
        void setMuted(bool shouldMute);
        void setSoloed(bool shouldSolo);

        void resized() override;
        void paint(juce::Graphics&) override;

    private:
        int index = 0;
        juce::Label nameLabel;
        juce::Slider gainSlider;
        juce::Slider panSlider;
        juce::ToggleButton muteButton { "Mute" };
        juce::ToggleButton soloButton { "Solo" };
    };

    juce::Label headerLabel;
    juce::OwnedArray<ChannelStrip> strips;
};
