#pragma once

#include <JuceHeader.h>

class WorkstationAudioEngine;

class MixerPanel final : public juce::Component
{
public:
    MixerPanel();

    void setChannelCount(int totalChannelCount);
    void setBankOffset(int newBankOffset);
    int getBankOffset() const noexcept { return bankOffset; }
    int getVisibleChannelCount() const noexcept { return visibleChannelCount; }

    void setChannelName(int channelIndex, const juce::String& name);
    void setChannelInsertName(int channelIndex, const juce::String& name);
    void setChannelInsertBypassed(int channelIndex, bool isBypassed);
    void setSelectedChannel(int channelIndex);
    void setChannelGain(int channelIndex, float gain);
    void setChannelPan(int channelIndex, float pan);
    void setChannelMuted(int channelIndex, bool shouldMute);
    void setChannelSoloed(int channelIndex, bool shouldSolo);
    void setMasterGain(float gain);

    std::function<void(int)> onBankOffsetChanged;
    std::function<void(int)> onInsertButtonClicked;
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
        ChannelStrip(int channelIndex, const juce::String& channelName, bool isMasterStrip = false);

        void setChannelIndex(int newChannelIndex);
        void setChannelName(const juce::String& name);
        void setInsertName(const juce::String& name);
        void setInsertBypassed(bool isBypassed);
        void setSelected(bool isSelected);
        void setGain(float gain);
        void setPan(float pan);
        void setMuted(bool shouldMute);
        void setSoloed(bool shouldSolo);

        void resized() override;
        void paint(juce::Graphics&) override;

    private:
        int index = 0;
        bool master = false;
        bool insertLoaded = false;
        bool insertBypassed = false;
        bool selected = false;
        juce::Label nameLabel;
        juce::Label insertLabel;
        juce::Label indexLabel;
        juce::Slider gainSlider;
        juce::Slider panSlider;
        juce::ToggleButton muteButton { "Mute" };
        juce::ToggleButton soloButton { "Solo" };
        juce::TextButton fxButton { "FX" };
    };

    juce::Label headerLabel;
    juce::Label bankLabel;
    juce::TextButton bankLeftButton { "Bank -" };
    juce::TextButton bankRightButton { "Bank +" };
    juce::OwnedArray<ChannelStrip> strips;
    std::unique_ptr<ChannelStrip> masterStrip;
    int totalChannelCount = 32;
    int bankOffset = 0;
    static constexpr int visibleChannelCount = 8;

    void refreshVisibleStripLabels();
};
