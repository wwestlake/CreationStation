#include "XTouchControlSurface.h"

namespace
{
constexpr int transportRewind = 91;
constexpr int transportFastForward = 92;
constexpr int transportStop = 93;
constexpr int transportPlay = 94;
constexpr int transportRecord = 95;
}

bool XTouchControlSurface::matchesXTouchFamily(const juce::String& deviceName)
{
    auto lowered = deviceName.toLowerCase();
    return lowered.contains("x-touch")
        || lowered.contains("xtouch")
        || lowered.contains("x touch")
        || lowered.contains("mackie");
}

float XTouchControlSurface::midi14BitToUnitFloat(int value)
{
    return juce::jlimit(0.0f, 1.0f, static_cast<float>(value) / 16383.0f);
}

float XTouchControlSurface::midi7BitToPanFloat(int value)
{
    return juce::jlimit(-1.0f, 1.0f, (static_cast<float>(value) - 64.0f) / 63.0f);
}

void XTouchControlSurface::reportStatus(const juce::String& text) const
{
    if (onStatusMessage)
        onStatusMessage(text);
}

void XTouchControlSurface::attachToDeviceManager(juce::AudioDeviceManager& deviceManager)
{
    enabledDeviceIds.clear();

    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (matchesXTouchFamily(device.name))
        {
            deviceManager.setMidiInputDeviceEnabled(device.identifier, true);
            enabledDeviceIds.add(device.identifier);

            if (activeDeviceName.isEmpty())
                activeDeviceName = device.name;
        }
    }

    deviceManager.addMidiInputDeviceCallback({}, this);

    if (activeDeviceName.isNotEmpty())
        reportStatus("MIDI: connected to " + activeDeviceName);
    else
        reportStatus("MIDI: no X-Touch detected yet");
}

void XTouchControlSurface::detachFromDeviceManager(juce::AudioDeviceManager& deviceManager)
{
    deviceManager.removeMidiInputDeviceCallback({}, this);

    for (const auto& identifier : enabledDeviceIds)
        deviceManager.setMidiInputDeviceEnabled(identifier, false);

    enabledDeviceIds.clear();
    activeDeviceName.clear();
}

void XTouchControlSurface::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (message.isPitchWheel())
    {
        auto channel = message.getChannel();
        auto trackIndex = channel - 1;

        if (trackIndex >= 0 && trackIndex < 8 && onFaderMoved)
            onFaderMoved(trackIndex, midi14BitToUnitFloat(message.getPitchWheelValue()));

        return;
    }

    if (message.isController())
    {
        auto channel = message.getChannel();
        auto trackIndex = channel - 1;

        if (trackIndex >= 0 && trackIndex < 8)
        {
            if (message.getControllerNumber() == 10 && onPanMoved)
            {
                onPanMoved(trackIndex, midi7BitToPanFloat(message.getControllerValue()));
                return;
            }

            if (message.getControllerNumber() == 0x10 && onMuteChanged)
            {
                onMuteChanged(trackIndex, message.getControllerValue() > 0);
                return;
            }

            if (message.getControllerNumber() == 0x18 && onSoloChanged)
            {
                onSoloChanged(trackIndex, message.getControllerValue() > 0);
                return;
            }
        }
    }

    if (! message.isNoteOn(true))
        return;

    auto note = message.getNoteNumber();

    if (note == transportPlay && onTransportCommand)
        onTransportCommand(TransportCommand::play);
    else if (note == transportStop && onTransportCommand)
        onTransportCommand(TransportCommand::stop);
    else if (note == transportRecord && onTransportCommand)
        onTransportCommand(TransportCommand::record);
    else if (note == transportRewind && onTransportCommand)
        onTransportCommand(TransportCommand::rewind);
    else if (note == transportFastForward && onTransportCommand)
        onTransportCommand(TransportCommand::fastForward);

    reportStatus("MIDI: " + message.getDescription());
}
