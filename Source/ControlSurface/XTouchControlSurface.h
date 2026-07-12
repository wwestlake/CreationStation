#pragma once

#include <JuceHeader.h>
#include <functional>

class XTouchControlSurface final : public juce::MidiInputCallback
{
public:
    enum class TransportCommand
    {
        play,
        stop,
        record,
        rewind,
        fastForward
    };

    std::function<void(int, float)> onFaderMoved;
    std::function<void(float)> onMasterFaderMoved;
    std::function<void(int, float)> onPanMoved;
    std::function<void(int, bool)> onMuteChanged;
    std::function<void(int, bool)> onSoloChanged;
    std::function<void(int)> onChannelSelected;
    std::function<void(const juce::String&)> onSpecialButtonPressed;
    std::function<void(int)> onBankStep;
    std::function<void(TransportCommand)> onTransportCommand;
    std::function<void(juce::String)> onStatusMessage;

    void attachToDeviceManager(juce::AudioDeviceManager& deviceManager);
    void detachFromDeviceManager(juce::AudioDeviceManager& deviceManager);

    void setTrackCount(int newTrackCount);
    void setBankOffset(int newBankOffset);
    void setChannelName(int trackIndex, const juce::String& name);
    void setChannelGain(int trackIndex, float gain);
    void setMasterFaderValue(float gain);
    void setChannelPan(int trackIndex, float pan);
    void setChannelMuted(int trackIndex, bool muted);
    void setChannelSoloed(int trackIndex, bool soloed);
    void setTransportState(bool playing, bool recording);
    void refreshVisibleWindow();

    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;

private:
    static bool matchesXTouchFamily(const juce::String& deviceName);
    static float midi14BitToUnitFloat(int value);
    static int unitFloatToMidi14Bit(float value);
    static float midi7BitToPanFloat(int value);
    static int panFloatToMidi7Bit(float value);
    static int gainToDisplayValue(float gain);

    void reportStatus(const juce::String& text) const;
    void sendScribbleStripText(int trackIndex, const juce::String& topLineText, const juce::String& bottomLineText);
    void sendFaderValue(int trackIndex, float gain);
    void sendMasterFaderValue(float gain);
    void sendPanValue(int trackIndex, float pan);
    void sendMuteValue(int trackIndex, bool muted);
    void sendSoloValue(int trackIndex, bool soloed);
    void sendTransportValue(int noteNumber, bool active);

    juce::String activeDeviceName;
    juce::StringArray enabledDeviceIds;
    juce::StringArray channelNames;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    int bankOffset = 0;
    int totalTrackCount = 32;
    bool transportPlaying = false;
    bool transportRecording = false;

};
