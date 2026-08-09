#pragma once

#include <JuceHeader.h>
#include <functional>
#include <map>
#include "ControlSurfaceMappingStore.h"

class WorkstationAudioEngine;

class XTouchControlSurface final : public juce::MidiInputCallback,
                                   private juce::Timer
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
    std::function<void(int)> onJogWheelMoved;
    std::function<void(int)> onBankStep;
    std::function<void(TransportCommand)> onTransportCommand;
    std::function<void(juce::String)> onStatusMessage;

    void attachToDeviceManager(juce::AudioDeviceManager& deviceManager);
    void detachFromDeviceManager(juce::AudioDeviceManager& deviceManager);

    // Lets any device the mapping store knows about (BCR2000, custom devicePattern entries, not
    // just the hardcoded Mackie protocol notes above) trigger transport via configurable
    // channel/number bindings instead of requiring the hardware to speak Mackie Control natively.
    void setControlSurfaceMappings(const ControlSurfaceMappingStore& store);

    // BCR2000/X-Touch are deliberately excluded from the engine's own MIDI input registration
    // (so their button presses don't trigger notes on instruments), but they still need to be
    // learnable - this lets handleIncomingMidiMessage report capturable messages into the same
    // learn-capture slot the engine exposes to everything else.
    void setEngineForMidiLearn(WorkstationAudioEngine& engineRef) noexcept { learnEngine = &engineRef; }

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

    // Motorized-fader position echo for MIDI Control (Fader Control) nodes: these are bound
    // directly to a raw MIDI channel via Learn, not to a Tracker track/bank slot, so this skips
    // the trackIndex/bankOffset remap sendFaderValue() does and writes the channel straight
    // through. Without this, a motorized fader's motor never learns where the software value
    // moved to, so on touch-release it snaps back to whatever position it last held.
    void sendRawFaderFeedback(int channel, float unitValue);

    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;

private:
    static bool matchesXTouchFamily(const juce::String& deviceName);
    static float midi14BitToUnitFloat(int value);
    static int unitFloatToMidi14Bit(float value);
    static float midi7BitToPanFloat(int value);
    static int panFloatToMidi7Bit(float value);
    static int gainToDisplayValue(float gain);

    static bool tryDispatchMappedTransport(const ControlSurfaceMappingStore& store,
                                           const juce::String& deviceName,
                                           const juce::MidiMessage& message,
                                           TransportCommand& outCommand);

    void reportStatus(const juce::String& text) const;
    void sendScribbleStripText(int trackIndex, const juce::String& topLineText, const juce::String& bottomLineText);
    void sendFaderValue(int trackIndex, float gain);
    void sendMasterFaderValue(float gain);
    void sendPanValue(int trackIndex, float pan);
    void sendMuteValue(int trackIndex, bool muted);
    void sendSoloValue(int trackIndex, bool soloed);
    void sendTransportValue(int noteNumber, bool active);
    void timerCallback() override;

    juce::String activeDeviceName;
    juce::StringArray enabledDeviceIds;
    std::map<juce::String, juce::String> deviceNameById;
    ControlSurfaceMappingStore mappingStore;
    WorkstationAudioEngine* learnEngine = nullptr;
    juce::StringArray channelNames;
    juce::Array<int> pendingScribbleTracks;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    int bankOffset = 0;
    int pendingScribbleBankOffset = 0;
    int pendingScribbleIndex = 0;
    int totalTrackCount = 0;
    bool transportPlaying = false;
    bool transportRecording = false;

};
