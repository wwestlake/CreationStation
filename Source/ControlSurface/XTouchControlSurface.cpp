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
constexpr int assignmentTrack = 0x28;
constexpr int assignmentSend = 0x29;
constexpr int assignmentPan = 0x2A;
constexpr int assignmentPlugin = 0x2B;
constexpr int assignmentEq = 0x2C;
constexpr int assignmentInstrument = 0x2D;
constexpr int bankLeftFull = 0x2E;
constexpr int bankRightFull = 0x2F;
constexpr int channelLeft = 0x30;
constexpr int channelRight = 0x31;
constexpr int faderFlip = 0x32;
constexpr int globalView = 0x33;
constexpr int displayNameValue = 0x34;
constexpr int displaySmpteBeats = 0x35;
constexpr int functionF1 = 0x36;
constexpr int functionF2 = 0x37;
constexpr int functionF3 = 0x38;
constexpr int functionF4 = 0x39;
constexpr int functionF5 = 0x3A;
constexpr int functionF6 = 0x3B;
constexpr int functionF7 = 0x3C;
constexpr int functionF8 = 0x3D;
constexpr int globalMidiTracks = 0x3E;
constexpr int globalInputs = 0x3F;
constexpr int globalAudioTracks = 0x40;
constexpr int globalAudioInstrument = 0x41;
constexpr int globalAux = 0x42;
constexpr int globalBusses = 0x43;
constexpr int globalOutputs = 0x44;
constexpr int globalUser = 0x45;
constexpr int modifierShift = 0x46;
constexpr int modifierOption = 0x47;
constexpr int modifierControl = 0x48;
constexpr int modifierAlt = 0x49;
constexpr int automationRead = 0x4A;
constexpr int automationWrite = 0x4B;
constexpr int automationTrim = 0x4C;
constexpr int automationTouch = 0x4D;
constexpr int automationLatch = 0x4E;
constexpr int automationGroup = 0x4F;
constexpr int utilitySave = 0x50;
constexpr int utilityUndo = 0x51;
constexpr int utilityCancel = 0x52;
constexpr int utilityEnter = 0x53;
constexpr int transportMarker = 0x54;
constexpr int transportNudge = 0x55;
constexpr int transportCycle = 0x56;
constexpr int transportDrop = 0x57;
constexpr int transportReplace = 0x58;
constexpr int transportClick = 0x59;
constexpr int transportSolo = 0x5A;
constexpr int scribbleStripWidth = 7;
constexpr int scribbleStripHeight = 2;
constexpr juce::uint8 scribbleDeviceId = 0x41;

juce::String formatScribbleLine(juce::String text)
{
    text = text.trim().toUpperCase();

    if (text.isEmpty())
        return juce::String().paddedRight(' ', scribbleStripWidth).substring(0, scribbleStripWidth);

    text = text.retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-./");
    text = text.replaceCharacter('_', ' ');
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

juce::String forceScribbleWidth(juce::String text)
{
    text = formatScribbleLine(std::move(text));
    if (text.length() > scribbleStripWidth)
        text = text.substring(0, scribbleStripWidth);

    while (text.length() < scribbleStripWidth)
        text << " ";

    return text;
}

juce::String makeTrackBankCode(int trackIndex, int bankOffset)
{
    auto trackNumber = juce::String(trackIndex + 1).paddedLeft('0', 3);
    auto bankNumber = juce::String((bankOffset / 8) + 1).paddedLeft('0', 3);
    return forceScribbleWidth(trackNumber + "-" + bankNumber);
}

std::array<juce::uint8, (size_t) scribbleStripWidth> makeScribbleBytes(const juce::String& text)
{
    auto fixed = forceScribbleWidth(text);
    std::array<juce::uint8, (size_t) scribbleStripWidth> bytes {};

    for (int characterIndex = 0; characterIndex < scribbleStripWidth; ++characterIndex)
    {
        auto character = fixed.getCharPointer()[characterIndex];
        bytes[(size_t) characterIndex] = (juce::uint8) (character == 0 ? ' ' : character);
    }

    return bytes;
}
}

bool XTouchControlSurface::matchesXTouchFamily(const juce::String& deviceName)
{
    auto lowered = deviceName.toLowerCase();
    return lowered.contains("x-touch")
        || lowered.contains("xtouch")
        || lowered.contains("x touch")
        || lowered.contains("mackie")
        || lowered.contains("bcr2000")
        || lowered.contains("b-control rotary")
        || lowered.contains("b-control");
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
        reportStatus("MIDI: no X-Touch or BCR2000 detected yet");

    refreshVisibleWindow();
}

void XTouchControlSurface::detachFromDeviceManager(juce::AudioDeviceManager& deviceManager)
{
    stopTimer();
    pendingScribbleTracks.clear();
    pendingScribbleIndex = 0;
    deviceManager.removeMidiInputDeviceCallback({}, this);
    midiOutput.reset();

    for (const auto& identifier : enabledDeviceIds)
        deviceManager.setMidiInputDeviceEnabled(identifier, false);

    enabledDeviceIds.clear();
    activeDeviceName.clear();
}

void XTouchControlSurface::setTrackCount(int newTrackCount)
{
    totalTrackCount = juce::jmax(0, newTrackCount);
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
    pendingScribbleTracks.clear();
    pendingScribbleIndex = 0;
    pendingScribbleBankOffset = bankOffset;

    for (int slot = 0; slot < 8; ++slot)
        pendingScribbleTracks.add(bankOffset + slot);

    if (! isTimerRunning())
        startTimer(35);
}

void XTouchControlSurface::timerCallback()
{
    if (midiOutput == nullptr || pendingScribbleIndex >= pendingScribbleTracks.size())
    {
        stopTimer();
        return;
    }

    auto trackIndex = pendingScribbleTracks[(int) pendingScribbleIndex++];
    if (juce::isPositiveAndBelow(trackIndex, totalTrackCount))
    {
        auto channelName = juce::isPositiveAndBelow(trackIndex, channelNames.size()) ? channelNames[(size_t) trackIndex]
                                                                                    : juce::String();
        sendScribbleStripText(trackIndex, channelName, makeTrackBankCode(trackIndex, pendingScribbleBankOffset));
    }
    else
    {
        sendScribbleStripText(trackIndex, {}, {});
    }

    if (pendingScribbleIndex >= pendingScribbleTracks.size())
        stopTimer();
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
    auto upperText = forceScribbleWidth(topLineText);
    auto lowerText = forceScribbleWidth(bottomLineText);
    juce::MemoryBlock data;
    juce::MemoryOutputStream stream(data, false);
    auto upperBytes = makeScribbleBytes(upperText);
    auto lowerBytes = makeScribbleBytes(lowerText);

    stream.writeByte((juce::uint8) 0xF0);
    stream.writeByte((juce::uint8) 0x00);
    stream.writeByte((juce::uint8) 0x20);
    stream.writeByte((juce::uint8) 0x32);
    stream.writeByte(scribbleDeviceId);
    stream.writeByte((juce::uint8) 0x4C);
    stream.writeByte((juce::uint8) stripIndex);
    stream.writeByte((juce::uint8) 0x00);

    for (auto character : upperBytes)
        stream.writeByte(character);

    for (auto character : lowerBytes)
        stream.writeByte(character);

    stream.writeByte((juce::uint8) 0xF7);

    midiOutput->sendMessageNow(juce::MidiMessage::createSysExMessage(data.getData(), (int) data.getSize()));
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
    else if (note == bankLeft && message.getVelocity() > 0 && onBankStep)
        onBankStep(-8);
    else if (note == bankRight && message.getVelocity() > 0 && onBankStep)
        onBankStep(8);
    else if (note == cursorLeftNote && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("cursor_left");
    else if (note == cursorRightNote && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("cursor_right");
    else if (note == cursorUpNote && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("cursor_up");
    else if (note == cursorDownNote && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("cursor_down");
    else if (note == zoomNote && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("zoom");
    else if (note == scrubNote && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("scrub");
    else if (note == assignmentTrack && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("assign_track");
    else if (note == assignmentSend && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("assign_send");
    else if (note == assignmentPan && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("assign_pan");
    else if (note == assignmentPlugin && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("assign_plugin");
    else if (note == assignmentEq && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("assign_eq");
    else if (note == assignmentInstrument && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("assign_instrument");
    else if (note == bankLeftFull && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("bank_left_full");
    else if (note == bankRightFull && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("bank_right_full");
    else if (note == channelLeft && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("channel_left");
    else if (note == channelRight && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("channel_right");
    else if (note == faderFlip && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("flip");
    else if (note == globalView && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("global_view");
    else if (note == displayNameValue && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("display_name_value");
    else if (note == displaySmpteBeats && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("display_smpte_beats");
    else if (note == functionF1 && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("f1");
    else if (note == functionF2 && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("f2");
    else if (note == functionF3 && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("f3");
    else if (note == functionF4 && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("f4");
    else if (note == functionF5 && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("f5");
    else if (note == functionF6 && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("f6");
    else if (note == functionF7 && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("f7");
    else if (note == functionF8 && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("f8");
    else if (note == globalMidiTracks && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("view_midi_tracks");
    else if (note == globalInputs && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("view_inputs");
    else if (note == globalAudioTracks && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("view_audio_tracks");
    else if (note == globalAudioInstrument && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("view_audio_instrument");
    else if (note == globalAux && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("view_aux");
    else if (note == globalBusses && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("view_busses");
    else if (note == globalOutputs && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("view_outputs");
    else if (note == globalUser && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("view_user");
    else if (note == modifierShift && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("modifier_shift");
    else if (note == modifierOption && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("modifier_option");
    else if (note == modifierControl && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("modifier_control");
    else if (note == modifierAlt && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("modifier_alt");
    else if (note == automationRead && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("automation_read");
    else if (note == automationWrite && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("automation_write");
    else if (note == automationTrim && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("automation_trim");
    else if (note == automationTouch && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("automation_touch");
    else if (note == automationLatch && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("automation_latch");
    else if (note == automationGroup && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("automation_group");
    else if (note == utilitySave && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("utility_save");
    else if (note == utilityUndo && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("utility_undo");
    else if (note == utilityCancel && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("utility_cancel");
    else if (note == utilityEnter && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("utility_enter");
    else if (note == transportMarker && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("transport_marker");
    else if (note == transportNudge && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("transport_nudge");
    else if (note == transportCycle && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("transport_cycle");
    else if (note == transportDrop && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("transport_drop");
    else if (note == transportReplace && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("transport_replace");
    else if (note == transportClick && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("transport_click");
    else if (note == transportSolo && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("transport_solo");
    else if (note == userANote && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("user_a");
    else if (note == userBNote && message.getVelocity() > 0 && onSpecialButtonPressed)
        onSpecialButtonPressed("user_b");

    reportStatus("MIDI: " + message.getDescription());
}
