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
    std::function<void(int, float)> onPanMoved;
    std::function<void(int, bool)> onMuteChanged;
    std::function<void(int, bool)> onSoloChanged;
    std::function<void(TransportCommand)> onTransportCommand;
    std::function<void(juce::String)> onStatusMessage;

    void attachToDeviceManager(juce::AudioDeviceManager& deviceManager);
    void detachFromDeviceManager(juce::AudioDeviceManager& deviceManager);

    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;

private:
    static bool matchesXTouchFamily(const juce::String& deviceName);
    static float midi14BitToUnitFloat(int value);
    static float midi7BitToPanFloat(int value);

    void reportStatus(const juce::String& text) const;

    juce::String activeDeviceName;
    juce::StringArray enabledDeviceIds;
};
