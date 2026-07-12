#include "XTouchControlSurface.h"

#include <cmath>

namespace
{
constexpr int transportRewind = 91;
constexpr int transportFastForward = 92;
constexpr int transportStop = 93;
constexpr int transportPlay = 94;
constexpr int transportRecord = 95;
constexpr int bankLeft = 0x2e;
constexpr int bankRight = 0x2f;
constexpr int muteBaseNote = 0x10;
constexpr int soloBaseNote = 0x18;
constexpr int selectBaseNote = 0x20;
constexpr int cursorUpNote = 0x60;
constexpr int cursorDownNote = 0x61;
constexpr int cursorLeftNote = 0x62;
constexpr int cursorRightNote = 0x63;
constexpr int zoomNote = 0x64;
constexpr int scrubNote = 0x65;
constexpr int userANote = 0x66;
constexpr int userBNote = 0x67;
constexpr int scribbleStripWidth = 7;
constexpr int scribbleStripHeight = 2;

juce::String formatScribbleLine(juce::String text)
{
    text = text.trim().toUpperCase();

    if (text.isEmpty())
        return juce::String().paddedRight(' ', scribbleStripWidth).substring(0, scribbleStripWidth);

    text = text.retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-./");
    text = text.replaceCharacter('_', ' ');
    text = text.replaceCharacter('-', ' ');
    text = text.replaceCharacter('.', ' ');
    text = text.replaceCharacter('/', ' ');
    text = text.replaceCharacter(',', ' ');
    text = text.replaceCharacter(':', ' ');
    text = text.replaceCharacter(';', ' ');
    text = text.replaceCharacter('(', ' ');
    text = text.replaceCharacter(')', ' ');
    text = text.replaceCharacter('[', ' ');
    text = text.replaceCharacter(']', ' ');
    text = text.replaceCharacter('{', ' ');
    text = text.replaceCharacter('}', ' ');
    text = text.replaceCharacter('\t', ' ');
    text = text.retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ");

    while (text.contains("  "))
        text = text.replace("  ", " ");

    return text.substring(0, scribbleStripWidth).paddedRight(' ', scribbleStripWidth);
}
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

int XTouchControlSurface::unitFloatToMidi14Bit(float value)
{
    return juce::jlimit(0, 16383, static_cast<int>(std::round(juce::jlimit(0.0f, 1.0f, value) * 16383.0f)));
}

float XTouchControlSurface::midi7BitToPanFloat(int value)
{
    return juce::jlimit(-1.0f, 1.0f, (static_cast<float>(value) - 64.0f) / 63.0f);
}

int XTouchControlSurface::panFloatToMidi7Bit(float value)
{
    return juce::jlimit(0, 127, static_cast<int>(std::round((juce::jlimit(-1.0f, 1.0f, value) * 63.0f) + 64.0f)));
}

int XTouchControlSurface::gainToDisplayValue(float gain)
{
    return unitFloatToMidi14Bit(gain);
}

void XTouchControlSurface::reportStatus(const juce::String& text) const
{
    if (onStatusMessage)
        onStatusMessage(text);
}

void XTouchControlSurface::attachToDeviceManager(juce::AudioDeviceManager& deviceManager)
{
    enabledDeviceIds.clear();
    midiOutput.reset();

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

    for (const auto& device : juce::MidiOutput::getAvailableDevices())
    {
        if (matchesXTouchFamily(device.name))
        {
            midiOutput = juce::MidiOutput::openDevice(device.identifier);
            if (midiOutput != nullptr)
            {
                activeDeviceName = device.name;
                break;
            }
        }
    }

    deviceManager.addMidiInputDeviceCallback({}, this);

    if (activeDeviceName.isNotEmpty())
        reportStatus("MIDI: connected to " + activeDeviceName);
    else
        reportStatus("MIDI: no X-Touch detected yet");

    refreshVisibleWindow();
}

void XTouchControlSurface::detachFromDeviceManager(juce::AudioDeviceManager& deviceManager)
{
    deviceManager.removeMidiInputDeviceCallback({}, this);
    midiOutput.reset();

    for (const auto& identifier : enabledDeviceIds)
        deviceManager.setMidiInputDeviceEnabled(identifier, false);

    enabledDeviceIds.clear();
    activeDeviceName.clear();
}

void XTouchControlSurface::setTrackCount(int newTrackCount)
{
    totalTrackCount = juce::jmax(1, newTrackCount);
    while (channelNames.size() < totalTrackCount)
        channelNames.add({});

    while (channelNames.size() > totalTrackCount)
        channelNames.removeRange(totalTrackCount, channelNames.size() - totalTrackCount);

    setBankOffset(bankOffset);
}

void XTouchControlSurface::setBankOffset(int newBankOffset)
{
    bankOffset = juce::jlimit(0, juce::jmax(0, totalTrackCount - 8), newBankOffset);
}

void XTouchControlSurface::refreshVisibleWindow()
{
    for (int trackIndex = bankOffset; trackIndex < juce::jmin(totalTrackCount, bankOffset + 8); ++trackIndex)
        sendScribbleStripText(trackIndex, {}, {});

    for (int trackIndex = bankOffset; trackIndex < juce::jmin(totalTrackCount, bankOffset + 8); ++trackIndex)
    {
        auto channelName = juce::isPositiveAndBelow(trackIndex, channelNames.size()) ? channelNames[(size_t) trackIndex]
                                                                                    : juce::String();
        if (channelName.isEmpty())
            channelName = "Track " + juce::String(trackIndex + 1);

        sendScribbleStripText(trackIndex,
                              formatScribbleLine(channelName),
                              {});
    }
}

void XTouchControlSurface::sendScribbleStripText(int trackIndex,
                                                 const juce::String& topLineText,
                                                 const juce::String& bottomLineText)
{
    if (midiOutput == nullptr)
        return;

    if (trackIndex < bankOffset || trackIndex >= bankOffset + 8)
        return;

    auto stripIndex = trackIndex - bankOffset;
    auto upperText = formatScribbleLine(topLineText);
    auto lowerText = formatScribbleLine(bottomLineText);

    auto sendLine = [this](int lineOffset, const juce::String& lineText)
    {
        juce::MemoryBlock data;
        juce::MemoryOutputStream stream(data, false);

        stream.writeByte((juce::uint8) 0x00);
        stream.writeByte((juce::uint8) 0x00);
        stream.writeByte((juce::uint8) 0x66);
        stream.writeByte((juce::uint8) 0x14);
        stream.writeByte((juce::uint8) 0x12);
        stream.writeByte((juce::uint8) lineOffset);

        auto textUtf8 = lineText.toRawUTF8();
        for (int characterIndex = 0; characterIndex < scribbleStripWidth; ++characterIndex)
            stream.writeByte((juce::uint8) textUtf8[characterIndex]);

        midiOutput->sendMessageNow(juce::MidiMessage::createSysExMessage(data.getData(), (int) data.getSize()));
    };

    auto upperOffset = stripIndex * scribbleStripWidth;
    auto lowerOffset = 0x38 + (stripIndex * scribbleStripWidth);

    sendLine(upperOffset, upperText);
    sendLine(lowerOffset, lowerText);
}

void XTouchControlSurface::sendFaderValue(int trackIndex, float gain)
{
    if (midiOutput == nullptr)
        return;

    if (trackIndex < bankOffset || trackIndex >= bankOffset + 8)
        return;

    auto channel = juce::jlimit(1, 8, trackIndex - bankOffset + 1);
    midiOutput->sendMessageNow(juce::MidiMessage::pitchWheel(channel, gainToDisplayValue(gain)));
}

void XTouchControlSurface::sendMasterFaderValue(float gain)
{
    if (midiOutput == nullptr)
        return;

    midiOutput->sendMessageNow(juce::MidiMessage::pitchWheel(9, gainToDisplayValue(gain)));
}

void XTouchControlSurface::setChannelName(int trackIndex, const juce::String& name)
{
    if (juce::isPositiveAndBelow(trackIndex, totalTrackCount))
    {
        while (channelNames.size() <= trackIndex)
            channelNames.add({});

        channelNames.set(trackIndex, name);
    }

    sendScribbleStripText(trackIndex,
                          formatScribbleLine(name),
                          {});
}

void XTouchControlSurface::sendPanValue(int trackIndex, float pan)
{
    if (midiOutput == nullptr)
        return;

    if (trackIndex < bankOffset || trackIndex >= bankOffset + 8)
        return;

    auto channel = juce::jlimit(1, 8, trackIndex - bankOffset + 1);
    midiOutput->sendMessageNow(juce::MidiMessage::controllerEvent(channel, 10, panFloatToMidi7Bit(pan)));
}

void XTouchControlSurface::sendMuteValue(int trackIndex, bool muted)
{
    if (midiOutput == nullptr)
        return;

    if (trackIndex < bankOffset || trackIndex >= bankOffset + 8)
        return;

    auto channel = juce::jlimit(1, 8, trackIndex - bankOffset + 1);
    midiOutput->sendMessageNow(juce::MidiMessage::noteOn(channel, muteBaseNote + channel - 1, muted ? (juce::uint8) 127 : (juce::uint8) 0));
}

void XTouchControlSurface::sendSoloValue(int trackIndex, bool soloed)
{
    if (midiOutput == nullptr)
        return;

    if (trackIndex < bankOffset || trackIndex >= bankOffset + 8)
        return;

    auto channel = juce::jlimit(1, 8, trackIndex - bankOffset + 1);
    midiOutput->sendMessageNow(juce::MidiMessage::noteOn(channel, soloBaseNote + channel - 1, soloed ? (juce::uint8) 127 : (juce::uint8) 0));
}

void XTouchControlSurface::sendTransportValue(int noteNumber, bool active)
{
    if (midiOutput == nullptr)
        return;

    midiOutput->sendMessageNow(juce::MidiMessage::noteOn(1, noteNumber, active ? (juce::uint8) 127 : (juce::uint8) 0));
}

void XTouchControlSurface::setChannelGain(int trackIndex, float gain)
{
    sendFaderValue(trackIndex, gain);
}

void XTouchControlSurface::setMasterFaderValue(float gain)
{
    sendMasterFaderValue(gain);
}

void XTouchControlSurface::setChannelPan(int trackIndex, float pan)
{
    sendPanValue(trackIndex, pan);
}

void XTouchControlSurface::setChannelMuted(int trackIndex, bool muted)
{
    sendMuteValue(trackIndex, muted);
}

void XTouchControlSurface::setChannelSoloed(int trackIndex, bool soloed)
{
    sendSoloValue(trackIndex, soloed);
}

void XTouchControlSurface::setTransportState(bool playing, bool recording)
{
    transportPlaying = playing;
    transportRecording = recording;

    sendTransportValue(transportPlay, playing);
    sendTransportValue(transportStop, ! playing);
    sendTransportValue(transportRecord, recording);
}

void XTouchControlSurface::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (message.isPitchWheel())
    {
        auto channel = message.getChannel();
        if (channel == 9)
        {
            if (onMasterFaderMoved)
                onMasterFaderMoved(midi14BitToUnitFloat(message.getPitchWheelValue()));
            return;
        }

        auto trackIndex = bankOffset + channel - 1;

        if (juce::isPositiveAndBelow(trackIndex, totalTrackCount) && onFaderMoved)
            onFaderMoved(trackIndex, midi14BitToUnitFloat(message.getPitchWheelValue()));

        return;
    }

    if (message.isController())
    {
        auto channel = message.getChannel();
        auto trackIndex = bankOffset + channel - 1;

        if (juce::isPositiveAndBelow(trackIndex, totalTrackCount))
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
    else if (note >= muteBaseNote && note < muteBaseNote + 8 && onMuteChanged)
    {
        auto trackIndex = bankOffset + (note - muteBaseNote);
        if (juce::isPositiveAndBelow(trackIndex, totalTrackCount))
            onMuteChanged(trackIndex, message.getVelocity() > 0);
    }
    else if (note >= soloBaseNote && note < soloBaseNote + 8 && onSoloChanged)
    {
        auto trackIndex = bankOffset + (note - soloBaseNote);
        if (juce::isPositiveAndBelow(trackIndex, totalTrackCount))
            onSoloChanged(trackIndex, message.getVelocity() > 0);
    }
    else if (note >= selectBaseNote && note < selectBaseNote + 8 && onChannelSelected)
    {
        auto trackIndex = bankOffset + (note - selectBaseNote);
        if (juce::isPositiveAndBelow(trackIndex, totalTrackCount))
            onChannelSelected(trackIndex);
    }
    else if (note == bankLeft && onBankStep)
        onBankStep(-8);
    else if (note == bankRight && onBankStep)
        onBankStep(8);
    else if (note == cursorLeftNote && onSpecialButtonPressed)
        onSpecialButtonPressed("cursor_left");
    else if (note == cursorRightNote && onSpecialButtonPressed)
        onSpecialButtonPressed("cursor_right");
    else if (note == cursorUpNote && onSpecialButtonPressed)
        onSpecialButtonPressed("cursor_up");
    else if (note == cursorDownNote && onSpecialButtonPressed)
        onSpecialButtonPressed("cursor_down");
    else if (note == zoomNote && onSpecialButtonPressed)
        onSpecialButtonPressed("zoom");
    else if (note == scrubNote && onSpecialButtonPressed)
        onSpecialButtonPressed("scrub");
    else if (note == userANote && onSpecialButtonPressed)
        onSpecialButtonPressed("user_a");
    else if (note == userBNote && onSpecialButtonPressed)
        onSpecialButtonPressed("user_b");

    reportStatus("MIDI: " + message.getDescription());
}
