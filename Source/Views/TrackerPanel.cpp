#include "TrackerPanel.h"

#include <cmath>

namespace
{
juce::String makeDefaultTrackName(int trackIndex)
{
    return "Tk-" + juce::String(trackIndex + 1).paddedLeft('0', 4);
}

juce::String makeTrackLabel(int trackIndex, const juce::String& trackName)
{
    return trackName.isNotEmpty() ? trackName : makeDefaultTrackName(trackIndex);
}

float levelToDb(float level)
{
    return level > 0.000001f ? juce::Decibels::gainToDecibels(level) : -100.0f;
}

float dbToMeterAmount(float db)
{
    constexpr auto minimumDb = -60.0f;
    constexpr auto maximumDb = 3.0f;
    return juce::jlimit(0.0f, 1.0f, (db - minimumDb) / (maximumDb - minimumDb));
}

juce::Colour colourForMeterDb(float db)
{
    if (db >= -3.0f)
        return juce::Colour(0xffff5f6d);

    if (db >= -18.0f)
        return juce::Colour(0xffffd166);

    return juce::Colour(0xff67e8a5);
}

class TrackIconButtonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton,
                              bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        auto active = button.getToggleState();
        auto accent = button.findColour(juce::TextButton::buttonOnColourId);
        auto fill = active ? juce::Colour(0xff17202a) : backgroundColour;

        if (isButtonDown)
            fill = accent.withAlpha(0.20f).overlaidWith(fill);
        else if (isMouseOverButton)
            fill = accent.withAlpha(0.12f).overlaidWith(fill);

        if (active)
        {
            g.setColour(accent.withAlpha(0.25f));
            g.fillRoundedRectangle(bounds.expanded(2.0f), 8.0f);
        }

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(active ? accent.withAlpha(0.95f) : juce::Colour(0xff53667d));
        g.drawRoundedRectangle(bounds, 8.0f, active ? 1.8f : 1.0f);

        auto lamp = juce::Rectangle<float>(5.0f, 5.0f).withCentre({ bounds.getRight() - 5.5f, bounds.getY() + 5.5f });
        g.setColour(active ? accent : juce::Colour(0xff2f3d4d));
        g.fillEllipse(lamp);
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool,
                        bool) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(6.0f);
        g.setColour(button.getToggleState() ? button.findColour(juce::TextButton::buttonOnColourId).brighter(0.45f)
                                            : juce::Colour(0xffd7e4f5));
        drawTrackIcon(g, bounds, button.getButtonText());
    }

private:
    static void drawTrackIcon(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& iconName)
    {
        auto centre = bounds.getCentre();
        auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());

        if (iconName == "arm")
        {
            g.fillEllipse(juce::Rectangle<float>(size * 0.50f, size * 0.50f).withCentre(centre));
            return;
        }

        if (iconName == "mute")
        {
            juce::Path speaker;
            speaker.addRectangle(bounds.getX() + size * 0.10f, centre.y - size * 0.16f, size * 0.16f, size * 0.32f);
            speaker.addTriangle(bounds.getX() + size * 0.24f, centre.y - size * 0.22f,
                                bounds.getX() + size * 0.48f, centre.y - size * 0.38f,
                                bounds.getX() + size * 0.48f, centre.y + size * 0.38f);
            speaker.addTriangle(bounds.getX() + size * 0.24f, centre.y - size * 0.22f,
                                bounds.getX() + size * 0.48f, centre.y + size * 0.38f,
                                bounds.getX() + size * 0.24f, centre.y + size * 0.22f);
            g.fillPath(speaker);
            g.drawLine(bounds.getRight() - size * 0.36f, centre.y - size * 0.26f,
                       bounds.getRight() - size * 0.08f, centre.y + size * 0.26f, 2.0f);
            g.drawLine(bounds.getRight() - size * 0.08f, centre.y - size * 0.26f,
                       bounds.getRight() - size * 0.36f, centre.y + size * 0.26f, 2.0f);
            return;
        }

        if (iconName == "solo")
        {
            g.setFont(juce::Font(size * 0.70f, juce::Font::bold));
            g.drawText("S", bounds.toNearestInt(), juce::Justification::centred, true);
            return;
        }

        if (iconName == "monitor")
        {
            juce::Path headphones;
            headphones.addCentredArc(centre.x, centre.y + size * 0.06f, size * 0.34f, size * 0.34f, 0.0f,
                                     juce::MathConstants<float>::pi * 1.08f,
                                     juce::MathConstants<float>::pi * 1.92f,
                                     true);
            g.strokePath(headphones, juce::PathStrokeType(2.0f));
            g.fillRoundedRectangle(centre.x - size * 0.42f, centre.y, size * 0.16f, size * 0.34f, 2.0f);
            g.fillRoundedRectangle(centre.x + size * 0.26f, centre.y, size * 0.16f, size * 0.34f, 2.0f);
            return;
        }
    }
};

TrackIconButtonLookAndFeel& getTrackIconButtonLookAndFeel()
{
    static TrackIconButtonLookAndFeel lookAndFeel;
    return lookAndFeel;
}

void drawDbMeterTicks(juce::Graphics& g, juce::Rectangle<float> meter)
{
    g.setFont(juce::Font(8.5f, juce::Font::bold));

    for (auto tickDb : { -30.0f, -3.0f, 0.0f, 3.0f })
    {
        auto y = meter.getBottom() - meter.getHeight() * dbToMeterAmount(tickDb);
        g.setColour(tickDb >= 0.0f ? juce::Colour(0x99ff8a8a) : juce::Colour(0x88c7d2e0));
        g.drawHorizontalLine(juce::roundToInt(y), meter.getX(), meter.getRight());

        auto label = tickDb > 0.0f ? "+" + juce::String((int) tickDb)
                                   : juce::String((int) tickDb);
        g.drawText(label,
                   meter.withY(y - 5.0f).withHeight(10.0f).reduced(1.0f, 0.0f),
                   juce::Justification::centredRight,
                   false);
    }

    g.setColour(juce::Colour(0x99c7d2e0));
    g.drawText("dB", meter.removeFromTop(10.0f), juce::Justification::centred, false);
}
}

TrackerPanel::TrackerPanel()
{
    setName("Tracker");

    titleLabel.setText("Tracker", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("DAW timeline: add tracks, select lanes, and build synced audio, MIDI, automation, and folder structure.", juce::dontSendNotification);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(hintLabel);

    selectionLabel.setText("No tracks yet.", juce::dontSendNotification);
    selectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));
    addAndMakeVisible(selectionLabel);

    timingLabel.setText("Project timing", juce::dontSendNotification);
    timingLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(timingLabel);

    bpmEditor.setText("120.0", juce::dontSendNotification);
    bpmEditor.setInputRestrictions(6, "0123456789.");
    bpmEditor.setJustification(juce::Justification::centred);
    bpmEditor.onReturnKey = [this] { commitTimingEdits(); };
    bpmEditor.onFocusLost = [this] { commitTimingEdits(); };
    addAndMakeVisible(bpmEditor);

    timeSignatureEditor.setText("4/4", juce::dontSendNotification);
    timeSignatureEditor.setInputRestrictions(5, "0123456789/");
    timeSignatureEditor.setJustification(juce::Justification::centred);
    timeSignatureEditor.onReturnKey = [this] { commitTimingEdits(); };
    timeSignatureEditor.onFocusLost = [this] { commitTimingEdits(); };
    addAndMakeVisible(timeSignatureEditor);

    for (auto key : { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B",
                      "Cm", "C#m", "Dm", "Ebm", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "Bbm", "Bm" })
        keySelector.addItem(key, keySelector.getNumItems() + 1);
    keySelector.setSelectedId(1, juce::dontSendNotification);
    keySelector.onChange = [this]
    {
        if (onKeyChanged)
            onKeyChanged(keySelector.getText());
    };
    addAndMakeVisible(keySelector);

    addTrackButton.onClick = [this]
    {
        if (onAddTrackRequested)
            onAddTrackRequested();
    };
    addAndMakeVisible(addTrackButton);

    removeTrackButton.onClick = [this]
    {
        if (onRemoveTrackRequested && selectedTrack >= 0)
            onRemoveTrackRequested(selectedTrack);
    };
    addAndMakeVisible(removeTrackButton);

    compactButton.onClick = [this] { canvas.setLaneHeight(72); };
    comfortButton.onClick = [this] { canvas.setLaneHeight(100); };
    tallButton.onClick = [this] { canvas.setLaneHeight(132); };
    zoomOutButton.onClick = [this]
    {
        if (onZoomOutRequested)
            onZoomOutRequested();
    };
    zoomInButton.onClick = [this]
    {
        if (onZoomInRequested)
            onZoomInRequested();
    };
    addMarkerButton.onClick = [this]
    {
        if (onMarkerAddRequested)
            onMarkerAddRequested();
    };
    addAndMakeVisible(compactButton);
    addAndMakeVisible(comfortButton);
    addAndMakeVisible(tallButton);
    addAndMakeVisible(zoomOutButton);
    addAndMakeVisible(zoomInButton);
    addAndMakeVisible(addMarkerButton);
    timelineScrollBar.addListener(this);
    addAndMakeVisible(timelineScrollBar);

    canvas.onTrackSelected = [this](int trackIndex)
    {
        setSelectedTrack(trackIndex);
        if (onTrackSelected)
            onTrackSelected(trackIndex);
    };
    canvas.onTrackNameChanged = [this](int trackIndex, const juce::String& name)
    {
        if (onTrackNameChanged)
            onTrackNameChanged(trackIndex, name);
    };
    canvas.onTrackKindChanged = [this](int trackIndex, cs::TrackKind kind)
    {
        if (onTrackKindChanged)
            onTrackKindChanged(trackIndex, kind);
    };
    canvas.onTrackArmChanged = [this](int trackIndex, bool armed)
    {
        if (onTrackArmChanged)
            onTrackArmChanged(trackIndex, armed);
    };
    canvas.onTrackMuteChanged = [this](int trackIndex, bool muted)
    {
        if (onTrackMuteChanged)
            onTrackMuteChanged(trackIndex, muted);
    };
    canvas.onTrackSoloChanged = [this](int trackIndex, bool soloed)
    {
        if (onTrackSoloChanged)
            onTrackSoloChanged(trackIndex, soloed);
    };
    canvas.onTrackMonitorChanged = [this](int trackIndex, bool monitored)
    {
        if (onTrackMonitorChanged)
            onTrackMonitorChanged(trackIndex, monitored);
    };
    canvas.onTrackStereoChanged = [this](int trackIndex, bool stereo)
    {
        if (onTrackStereoChanged)
            onTrackStereoChanged(trackIndex, stereo);
    };
    canvas.onTrackGainChanged = [this](int trackIndex, float gain)
    {
        if (onTrackGainChanged)
            onTrackGainChanged(trackIndex, gain);
    };
    canvas.onTrackInputChanged = [this](int trackIndex, int inputChannel)
    {
        if (onTrackInputChanged)
            onTrackInputChanged(trackIndex, inputChannel);
    };
    canvas.onTrackFxRequested = [this](int trackIndex)
    {
        if (onTrackFxRequested)
            onTrackFxRequested(trackIndex);
    };
    canvas.onPlayheadPositionChanged = [this](double seconds)
    {
        if (onPlayheadPositionChanged)
            onPlayheadPositionChanged(seconds);
    };
    canvas.onLoopRegionChanged = [this](double startSeconds, double endSeconds)
    {
        if (onLoopRegionChanged)
            onLoopRegionChanged(startSeconds, endSeconds);
    };
    canvas.onMarkerClicked = [this](const juce::String& markerId)
    {
        if (onMarkerClicked)
            onMarkerClicked(markerId);
    };
    canvas.onClipMoved = [this](int clipIndex, int trackIndex, double startSeconds)
    {
        if (onClipMoved)
            onClipMoved(clipIndex, trackIndex, startSeconds);
    };
    canvas.onClipMoveCommitted = [this]
    {
        if (onClipMoveCommitted)
            onClipMoveCommitted();
    };
    canvas.onClipSelected = [this](int clipIndex)
    {
        selectedClipIndex = clipIndex;
        if (onClipSelected)
            onClipSelected(clipIndex);
    };
    canvas.onClipRenameRequested = [this](int clipIndex)
    {
        if (onClipRenameRequested)
            onClipRenameRequested(clipIndex);
    };
    canvas.onClipSplitRequested = [this](int clipIndex, double seconds)
    {
        if (onClipSplitRequested)
            onClipSplitRequested(clipIndex, seconds);
    };
    canvas.onClipDuplicateRequested = [this](int clipIndex)
    {
        if (onClipDuplicateRequested)
            onClipDuplicateRequested(clipIndex);
    };
    canvas.onClipDeleteRequested = [this](int clipIndex)
    {
        if (onClipDeleteRequested)
            onClipDeleteRequested(clipIndex);
    };
    addAndMakeVisible(canvas);

    refreshSelectionLabel();
}

void TrackerPanel::setTrackCount(int newTrackCount)
{
    canvas.setTrackCount(newTrackCount);
    if (newTrackCount <= 0)
        selectedTrack = -1;
    else
        selectedTrack = juce::jlimit(0, newTrackCount - 1, selectedTrack < 0 ? 0 : selectedTrack);

    canvas.setSelectedTrack(selectedTrack);
    refreshSelectionLabel();
}

void TrackerPanel::setTrackName(int trackIndex, const juce::String& name)
{
    canvas.setTrackName(trackIndex, name);
    refreshSelectionLabel();
}

void TrackerPanel::setTrackKind(int trackIndex, cs::TrackKind kind)
{
    canvas.setTrackKind(trackIndex, kind);
}

void TrackerPanel::setTrackArmed(int trackIndex, bool armed)
{
    canvas.setTrackArmed(trackIndex, armed);
}

void TrackerPanel::setTrackMuted(int trackIndex, bool muted)
{
    canvas.setTrackMuted(trackIndex, muted);
}

void TrackerPanel::setTrackSoloed(int trackIndex, bool soloed)
{
    canvas.setTrackSoloed(trackIndex, soloed);
}

void TrackerPanel::setTrackMonitored(int trackIndex, bool monitored)
{
    canvas.setTrackMonitored(trackIndex, monitored);
}

void TrackerPanel::setTrackStereo(int trackIndex, bool stereo)
{
    canvas.setTrackStereo(trackIndex, stereo);
}

void TrackerPanel::setTrackLevel(int trackIndex, float level)
{
    canvas.setTrackLevel(trackIndex, level);
}

void TrackerPanel::setTrackGain(int trackIndex, float gain)
{
    canvas.setTrackGain(trackIndex, gain);
}

void TrackerPanel::setTrackFxSummary(int trackIndex, int pluginCount)
{
    canvas.setTrackFxSummary(trackIndex, pluginCount);
}

void TrackerPanel::setInputSources(const juce::Array<juce::String>& sourceNames)
{
    canvas.setInputSources(sourceNames);
}

void TrackerPanel::setTrackInput(int trackIndex, int inputChannel)
{
    canvas.setTrackInput(trackIndex, inputChannel);
}

void TrackerPanel::setSelectedTrack(int trackIndex)
{
    selectedTrack = trackIndex;
    canvas.setSelectedTrack(trackIndex);
    refreshSelectionLabel();
}

void TrackerPanel::setSelectedClip(int clipIndex)
{
    selectedClipIndex = clipIndex;
    canvas.setSelectedClip(clipIndex);
}

void TrackerPanel::setTimingInfo(double bpm, int numerator, int denominator, const juce::String& key)
{
    bpmEditor.setText(juce::String(bpm, 1), juce::dontSendNotification);
    timeSignatureEditor.setText(juce::String(numerator) + "/" + juce::String(denominator), juce::dontSendNotification);
    auto keyId = 1;
    for (int index = 0; index < keySelector.getNumItems(); ++index)
        if (keySelector.getItemText(index) == key)
            keyId = index + 1;
    keySelector.setSelectedId(keyId > 0 ? keyId : 1, juce::dontSendNotification);
}

void TrackerPanel::setTimelineModel(const cs::TimelineModel* model)
{
    canvas.setTimelineModel(model);
    refreshTimelineScrollBar();
}

void TrackerPanel::refreshTimelineView()
{
    refreshTimelineScrollBar();
    canvas.repaint();
}

void TrackerPanel::centerTransportInView()
{
    auto visibleSeconds = canvas.getVisibleDurationSeconds();
    auto totalSeconds = canvas.getTotalDurationSeconds();
    auto desiredStart = canvas.getTransportSeconds() - (visibleSeconds * 0.5);
    auto maxStart = juce::jmax(0.0, totalSeconds - visibleSeconds);
    auto scrollStart = juce::jlimit(0.0, maxStart, desiredStart);

    timelineScrollBar.setCurrentRange(scrollStart, juce::jmax(0.1, visibleSeconds), juce::dontSendNotification);
    canvas.setScrollSeconds(scrollStart);
}

void TrackerPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff11161e));

    auto bounds = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(juce::Colour(0xff182131));
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0xff26364b));
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);
}

void TrackerPanel::resized()
{
    auto area = getLocalBounds().reduced(22, 18);
    auto header = area.removeFromTop(96);
    auto controls = header.removeFromRight(420);
    titleLabel.setBounds(header.removeFromTop(32));
    hintLabel.setBounds(header.removeFromTop(24));
    auto infoRow = header.removeFromTop(32);
    selectionLabel.setBounds(infoRow.removeFromLeft(180));
    timingLabel.setBounds(infoRow.removeFromLeft(92));
    bpmEditor.setBounds(infoRow.removeFromLeft(62));
    infoRow.removeFromLeft(6);
    timeSignatureEditor.setBounds(infoRow.removeFromLeft(52));
    infoRow.removeFromLeft(6);
    keySelector.setBounds(infoRow.removeFromLeft(72));

    auto trackButtons = controls.removeFromTop(32);
    addTrackButton.setBounds(trackButtons.removeFromLeft(154));
    trackButtons.removeFromLeft(8);
    removeTrackButton.setBounds(trackButtons.removeFromLeft(120));
    trackButtons.removeFromLeft(12);
    zoomOutButton.setBounds(trackButtons.removeFromLeft(78));
    trackButtons.removeFromLeft(6);
    zoomInButton.setBounds(trackButtons.removeFromLeft(78));

    controls.removeFromTop(8);
    auto heightButtons = controls.removeFromTop(30);
    compactButton.setBounds(heightButtons.removeFromLeft(96));
    heightButtons.removeFromLeft(6);
    comfortButton.setBounds(heightButtons.removeFromLeft(96));
    heightButtons.removeFromLeft(6);
    tallButton.setBounds(heightButtons.removeFromLeft(80));
    heightButtons.removeFromLeft(6);
    addMarkerButton.setBounds(heightButtons.removeFromLeft(90));

    area.removeFromTop(12);
    timelineScrollBar.setBounds(area.removeFromBottom(16));
    area.removeFromBottom(6);
    canvas.setBounds(area);
    refreshTimelineScrollBar();
}

void TrackerPanel::refreshSelectionLabel()
{
    auto trackCount = canvas.getNumChildComponents();
    juce::ignoreUnused(trackCount);
    removeTrackButton.setEnabled(selectedTrack >= 0);

    if (selectedTrack < 0)
    {
        selectionLabel.setText("No tracks yet. Use + Add Track to start the timeline.", juce::dontSendNotification);
        return;
    }

    selectionLabel.setText("Selected track " + juce::String(selectedTrack + 1), juce::dontSendNotification);
}

void TrackerPanel::refreshTimelineScrollBar()
{
    auto totalSeconds = canvas.getTotalDurationSeconds();
    auto visibleSeconds = canvas.getVisibleDurationSeconds();
    timelineScrollBar.setRangeLimits(0.0, juce::jmax(totalSeconds, visibleSeconds));
    timelineScrollBar.setCurrentRange(juce::jlimit(0.0, juce::jmax(0.0, totalSeconds - visibleSeconds), timelineScrollBar.getCurrentRangeStart()),
                                      juce::jmax(0.1, visibleSeconds));
}

void TrackerPanel::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved == &timelineScrollBar)
        canvas.setScrollSeconds(newRangeStart);
}

void TrackerPanel::TimelineCanvas::setTrackCount(int newTrackCount)
{
    trackCount = juce::jmax(0, newTrackCount);
    while (trackNames.size() < trackCount)
        trackNames.add({});
    while (trackNames.size() > trackCount)
        trackNames.remove(trackNames.size() - 1);
    trackArmed.resize((size_t) trackCount, false);
    trackKinds.resize((size_t) trackCount, cs::TrackKind::audio);
    trackMuted.resize((size_t) trackCount, false);
    trackSoloed.resize((size_t) trackCount, false);
    trackMonitored.resize((size_t) trackCount, false);
    trackStereo.resize((size_t) trackCount, false);
    trackLevels.resize((size_t) trackCount, 0.0f);
    trackGains.resize((size_t) trackCount, 0.75f);

    while (trackHeaders.size() < trackCount)
    {
        auto trackIndex = trackHeaders.size();
        auto* header = new TrackHeader(trackIndex);
        header->onSelected = [this](int index)
        {
            selectedTrack = index;
            if (onTrackSelected)
                onTrackSelected(index);
            repaint();
        };
        header->onNameChanged = [this](int index, const juce::String& name)
        {
            setTrackName(index, name);
            if (onTrackNameChanged)
                onTrackNameChanged(index, name);
        };
        header->onKindChanged = [this](int index, cs::TrackKind kind)
        {
            setTrackKind(index, kind);
            if (onTrackKindChanged)
                onTrackKindChanged(index, kind);
        };
        header->onArmChanged = [this](int index, bool armed)
        {
            setTrackArmed(index, armed);
            if (onTrackArmChanged)
                onTrackArmChanged(index, armed);
        };
        header->onMuteChanged = [this](int index, bool muted)
        {
            setTrackMuted(index, muted);
            if (onTrackMuteChanged)
                onTrackMuteChanged(index, muted);
        };
        header->onSoloChanged = [this](int index, bool soloed)
        {
            setTrackSoloed(index, soloed);
            if (onTrackSoloChanged)
                onTrackSoloChanged(index, soloed);
        };
        header->onMonitorChanged = [this](int index, bool monitored)
        {
            setTrackMonitored(index, monitored);
            if (onTrackMonitorChanged)
                onTrackMonitorChanged(index, monitored);
        };
        header->onStereoChanged = [this](int index, bool stereo)
        {
            setTrackStereo(index, stereo);
            if (onTrackStereoChanged)
                onTrackStereoChanged(index, stereo);
        };
        header->onGainChanged = [this](int index, float gain)
        {
            setTrackGain(index, gain);
            if (onTrackGainChanged)
                onTrackGainChanged(index, gain);
        };
        header->onInputChanged = [this](int index, int inputChannel)
        {
            setTrackInput(index, inputChannel);
            if (onTrackInputChanged)
                onTrackInputChanged(index, inputChannel);
        };
        header->onFxRequested = [this](int index)
        {
            if (onTrackFxRequested)
                onTrackFxRequested(index);
        };
        addAndMakeVisible(header);
        header->setInputSources(inputSourceNames);
        trackHeaders.add(header);
    }
    while (trackHeaders.size() > trackCount)
        trackHeaders.remove(trackHeaders.size() - 1);

    if (trackCount == 0)
        selectedTrack = -1;
    else
        selectedTrack = juce::jlimit(0, trackCount - 1, selectedTrack < 0 ? 0 : selectedTrack);

    repaint();
    resized();
}

void TrackerPanel::TimelineCanvas::setTrackName(int trackIndex, const juce::String& name)
{
    if (! juce::isPositiveAndBelow(trackIndex, trackNames.size()))
        return;

    trackNames.set(trackIndex, name);
    if (auto* header = trackHeaders[trackIndex])
        header->setTrackName(name);
    repaint();
}

void TrackerPanel::commitTimingEdits()
{
    auto bpm = bpmEditor.getText().getDoubleValue();
    auto signature = timeSignatureEditor.getText().trim();
    auto numerator = signature.upToFirstOccurrenceOf("/", false, false).getIntValue();
    auto denominator = signature.fromFirstOccurrenceOf("/", false, false).getIntValue();

    if (onTimeSignatureChanged)
        onTimeSignatureChanged(numerator > 0 ? numerator : 4, denominator > 0 ? denominator : 4);

    if (onTempoChanged)
        onTempoChanged(bpm);

    if (onKeyChanged)
        onKeyChanged(keySelector.getText());
}

void TrackerPanel::TimelineCanvas::setTrackKind(int trackIndex, cs::TrackKind kind)
{
    if (! juce::isPositiveAndBelow(trackIndex, (int) trackKinds.size()))
        return;

    trackKinds[(size_t) trackIndex] = kind;
    if (auto* header = trackHeaders[trackIndex])
        header->setTrackKind(kind);
    repaint();
}

void TrackerPanel::TimelineCanvas::setTrackArmed(int trackIndex, bool armed)
{
    if (! juce::isPositiveAndBelow(trackIndex, (int) trackArmed.size()))
        return;

    trackArmed[(size_t) trackIndex] = armed;
    if (auto* header = trackHeaders[trackIndex])
        header->setArmed(armed);
}

void TrackerPanel::TimelineCanvas::setTrackMuted(int trackIndex, bool muted)
{
    if (! juce::isPositiveAndBelow(trackIndex, (int) trackMuted.size()))
        return;

    trackMuted[(size_t) trackIndex] = muted;
    if (auto* header = trackHeaders[trackIndex])
        header->setMuted(muted);
}

void TrackerPanel::TimelineCanvas::setTrackSoloed(int trackIndex, bool soloed)
{
    if (! juce::isPositiveAndBelow(trackIndex, (int) trackSoloed.size()))
        return;

    trackSoloed[(size_t) trackIndex] = soloed;
    if (auto* header = trackHeaders[trackIndex])
        header->setSoloed(soloed);
}

void TrackerPanel::TimelineCanvas::setTrackMonitored(int trackIndex, bool monitored)
{
    if (! juce::isPositiveAndBelow(trackIndex, (int) trackMonitored.size()))
        return;

    trackMonitored[(size_t) trackIndex] = monitored;
    if (auto* header = trackHeaders[trackIndex])
        header->setMonitored(monitored);
}

void TrackerPanel::TimelineCanvas::setTrackStereo(int trackIndex, bool stereo)
{
    if (! juce::isPositiveAndBelow(trackIndex, (int) trackStereo.size()))
        return;

    trackStereo[(size_t) trackIndex] = stereo;
    if (auto* header = trackHeaders[trackIndex])
        header->setStereo(stereo);
}

void TrackerPanel::TimelineCanvas::setTrackLevel(int trackIndex, float level)
{
    if (! juce::isPositiveAndBelow(trackIndex, (int) trackLevels.size()))
        return;

    trackLevels[(size_t) trackIndex] = juce::jlimit(0.0f, 1.0f, level);
    if (auto* header = trackHeaders[trackIndex])
        header->setLevel(trackLevels[(size_t) trackIndex]);
}

void TrackerPanel::TimelineCanvas::setTrackGain(int trackIndex, float gain)
{
    if (! juce::isPositiveAndBelow(trackIndex, (int) trackGains.size()))
        return;

    trackGains[(size_t) trackIndex] = juce::jlimit(0.0f, 1.0f, gain);
    if (auto* header = trackHeaders[trackIndex])
        header->setGain(trackGains[(size_t) trackIndex]);
}

void TrackerPanel::TimelineCanvas::setTrackFxSummary(int trackIndex, int pluginCount)
{
    if (auto* header = trackHeaders[trackIndex])
        header->setFxSummary(pluginCount);
}

void TrackerPanel::TimelineCanvas::setInputSources(const juce::Array<juce::String>& sourceNames)
{
    inputSourceNames = sourceNames;
    for (auto* header : trackHeaders)
        if (header != nullptr)
            header->setInputSources(inputSourceNames);
}

void TrackerPanel::TimelineCanvas::setTrackInput(int trackIndex, int inputChannel)
{
    if (! juce::isPositiveAndBelow(trackIndex, trackHeaders.size()))
        return;

    if (auto* header = trackHeaders[trackIndex])
        header->setInputChannel(inputChannel);
}

void TrackerPanel::TimelineCanvas::setSelectedTrack(int trackIndex)
{
    selectedTrack = trackIndex;
    for (int index = 0; index < trackHeaders.size(); ++index)
        if (auto* header = trackHeaders[index])
            header->setSelected(index == selectedTrack);
    repaint();
}

void TrackerPanel::TimelineCanvas::setSelectedClip(int clipIndex)
{
    selectedClipIndex = clipIndex;
    repaint();
}

void TrackerPanel::TimelineCanvas::setLaneHeight(int newLaneHeight)
{
    laneHeight = juce::jlimit(64, 156, newLaneHeight);
    resized();
    repaint();
}

void TrackerPanel::TimelineCanvas::setTimelineModel(const cs::TimelineModel* model)
{
    timelineModel = model;
    repaint();
}

void TrackerPanel::TimelineCanvas::setScrollSeconds(double seconds)
{
    scrollSeconds = juce::jmax(0.0, seconds);
    repaint();
}

double TrackerPanel::TimelineCanvas::getTransportSeconds() const noexcept
{
    return timelineModel != nullptr ? timelineModel->getTransportSeconds() : 0.0;
}

double TrackerPanel::TimelineCanvas::getVisibleDurationSeconds() const noexcept
{
    auto labelWidth = juce::jmin(340, juce::jmax(290, getWidth() / 4));
    auto timelineWidth = juce::jmax(1, getWidth() - labelWidth - 20);
    auto pixelsPerSecond = timelineModel != nullptr ? timelineModel->getPixelsPerSecond() : 120.0;
    return (double) timelineWidth / pixelsPerSecond;
}

double TrackerPanel::TimelineCanvas::getTotalDurationSeconds() const noexcept
{
    return timelineModel != nullptr ? timelineModel->getTotalDurationSeconds() : 8.0;
}

void TrackerPanel::TimelineCanvas::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d1218));
    auto bounds = getLocalBounds();

    constexpr int rulerHeight = 56;

    auto labelWidth = juce::jmin(340, juce::jmax(290, bounds.getWidth() / 4));
    auto timelineWidth = juce::jmax(1, getWidth() - labelWidth - 20);
    auto tempoBpm = timelineModel != nullptr ? timelineModel->getTempoBpm() : 120.0;
    auto beatsPerMeasure = timelineModel != nullptr ? timelineModel->getTimeSignatureNumerator() : 4;
    auto beatDenominator = timelineModel != nullptr ? timelineModel->getTimeSignatureDenominator() : 4;
    auto keyText = timelineModel != nullptr ? timelineModel->getMusicalKey() : juce::String("C");
    auto visibleSeconds = timelineModel != nullptr ? (double) timelineWidth / timelineModel->getPixelsPerSecond() : 16.0;
    auto visibleBeats = juce::jmax(4, juce::roundToInt(visibleSeconds * tempoBpm / 60.0));
    auto secondsPerBeat = 60.0 / tempoBpm;
    auto beatWidth = timelineModel != nullptr ? timelineModel->getPixelsPerSecond() * secondsPerBeat
                                               : juce::jmax(42.0, (double) timelineWidth / visibleBeats);
    auto startBeat = juce::jmax(0, (int) std::floor(scrollSeconds / secondsPerBeat));
    auto endBeat = startBeat + visibleBeats + 2;
    auto ruler = bounds.removeFromTop(rulerHeight);

    g.setColour(juce::Colour(0xff202a39));
    g.fillRect(ruler);
    g.setColour(juce::Colour(0xff2d3b51));
    g.fillRect(ruler.removeFromLeft(labelWidth));
    g.setColour(juce::Colour(0xffaebbd0));
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText(juce::String(tempoBpm, 1) + " BPM  |  "
                   + juce::String(beatsPerMeasure) + "/" + juce::String(beatDenominator)
                   + "  |  Key " + keyText,
               12,
               0,
               labelWidth - 24,
               22,
               juce::Justification::centredLeft,
               true);
    g.setFont(juce::Font(12.0f));
    g.drawText("Measures / Beats / Time", 12, 24, labelWidth - 24, 24, juce::Justification::centredLeft, true);

    for (int beat = startBeat; beat <= endBeat; ++beat)
    {
        auto beatSeconds = (double) beat * secondsPerBeat;
        auto x = labelWidth + juce::roundToInt((beatSeconds - scrollSeconds) * (timelineModel != nullptr ? timelineModel->getPixelsPerSecond() : 120.0));
        if (x < labelWidth - 80 || x > getWidth() + 80)
            continue;

        auto isMeasure = beat % beatsPerMeasure == 0;
        g.setColour(isMeasure ? juce::Colour(0xff74caff) : juce::Colour(0xff3a465a));
        g.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));

        auto measure = beat / beatsPerMeasure + 1;
        auto beatInMeasure = beat % beatsPerMeasure + 1;
        auto seconds = secondsPerBeat * static_cast<double>(beat);
        auto millis = juce::roundToInt(seconds * 1000.0) % 1000;
        auto wholeSeconds = static_cast<int>(seconds);
        auto timeText = juce::String(wholeSeconds / 60) + ":"
                      + juce::String(wholeSeconds % 60).paddedLeft('0', 2) + "."
                      + juce::String(millis).paddedLeft('0', 3);

        g.setColour(isMeasure ? juce::Colour(0xfff0f5ff) : juce::Colour(0xff9fb0c8));
        g.setFont(juce::Font(isMeasure ? 13.0f : 11.0f).boldened());
        g.drawText(isMeasure ? ("M" + juce::String(measure)) : juce::String(beatInMeasure),
                   x + 4,
                   3,
                   juce::roundToInt(beatWidth) - 6,
                   16,
                   juce::Justification::centredLeft,
                   true);

        g.setFont(juce::Font(11.0f));
        g.drawText(juce::String(measure) + "." + juce::String(beatInMeasure),
                   x + 4,
                   20,
                   juce::roundToInt(beatWidth) - 6,
                   14,
                   juce::Justification::centredLeft,
                   true);
        g.drawText(timeText,
                   x + 4,
                   36,
                   juce::roundToInt(beatWidth) - 6,
                   14,
                   juce::Justification::centredLeft,
                   true);
    }

    if (trackCount == 0)
    {
        g.setColour(juce::Colour(0xff9fb0c8));
        g.setFont(juce::Font(18.0f).boldened());
        g.drawText("No tracks yet", bounds.reduced(24), juce::Justification::centred);
        g.setFont(juce::Font(14.0f));
        g.drawText("Add tracks here; audio, MIDI, automation, and folders come next.", bounds.reduced(24).translated(0, 26), juce::Justification::centred);
        return;
    }

    for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
    {
        auto lane = juce::Rectangle<int>(0, rulerHeight + trackIndex * laneHeight, getWidth(), laneHeight);
        if (lane.getY() > getHeight())
            break;

        auto selected = trackIndex == selectedTrack;
        g.setColour(selected ? juce::Colour(0xff223653) : (trackIndex % 2 == 0 ? juce::Colour(0xff151c28) : juce::Colour(0xff121824)));
        g.fillRect(lane);
        g.setColour(selected ? juce::Colour(0xff74caff) : juce::Colour(0xff26364b));
        g.drawRect(lane, selected ? 2 : 1);

        auto timelineLane = lane.withTrimmedLeft(labelWidth).reduced(12, 14);
        auto timelineOriginX = timelineLane.getX();
        auto centerY = timelineLane.getCentreY();

        g.setColour(juce::Colour(0xff233048));
        g.drawHorizontalLine(centerY, static_cast<float>(timelineLane.getX()), static_cast<float>(timelineLane.getRight()));
        g.setColour(juce::Colour(0x339fb0c8));
        auto hintArea = timelineLane.withWidth(180).withHeight(24);
        hintArea.setCentre(hintArea.getCentreX(), centerY);
        g.drawText("record or drop media here", hintArea, juce::Justification::centredLeft, true);

        g.setColour(juce::Colour(0xff243147));
        for (int beat = startBeat; beat <= endBeat; ++beat)
        {
            auto beatSeconds = (double) beat * secondsPerBeat;
            auto x = labelWidth + juce::roundToInt((beatSeconds - scrollSeconds) * (timelineModel != nullptr ? timelineModel->getPixelsPerSecond() : 120.0));
            if (x < labelWidth || x > getWidth())
                continue;

            g.drawVerticalLine(x, static_cast<float>(lane.getY() + 10), static_cast<float>(lane.getBottom() - 10));
        }

        if (timelineModel != nullptr)
        {
            const auto& clips = timelineModel->getClips();
            for (int clipIndex = 0; clipIndex < static_cast<int>(clips.size()); ++clipIndex)
            {
                const auto& clip = clips[(size_t) clipIndex];
                if (clip.trackIndex != trackIndex)
                    continue;

                auto pixelsPerSecond = timelineModel->getPixelsPerSecond();
                auto clipX = timelineOriginX + juce::roundToInt((clip.startSeconds - scrollSeconds) * pixelsPerSecond);
                auto clipWidth = juce::jmax(8, juce::roundToInt(clip.durationSeconds * pixelsPerSecond));
                auto clipBounds = juce::Rectangle<int>(clipX, timelineLane.getY(), clipWidth, timelineLane.getHeight()).reduced(2, 4);

                if (! clipBounds.intersects(timelineLane))
                    continue;

                g.setColour(clip.recording ? juce::Colour(0xff5d1f2a) : juce::Colour(0xff167c8f));
                g.fillRoundedRectangle(clipBounds.toFloat(), 7.0f);
                auto isSelectedClip = clipIndex == selectedClipIndex;
                g.setColour(isSelectedClip ? juce::Colour(0xffffd166)
                                           : (clip.recording ? juce::Colour(0xffff6b6b) : juce::Colour(0xff74e0ff)));
                g.drawRoundedRectangle(clipBounds.toFloat(), 7.0f, isSelectedClip ? 2.5f : 1.5f);
                if (isSelectedClip)
                {
                    g.setColour(juce::Colour(0x44ffd166));
                    g.drawRoundedRectangle(clipBounds.toFloat().expanded(3.0f), 9.0f, 2.0f);
                }

                auto waveformArea = clipBounds.reduced(8, 18);
                g.setColour(juce::Colour(0xffd7f8ff));

                if (! clip.peaks.empty())
                {
                    auto maxPeak = 0.01f;
                    for (auto peak : clip.peaks)
                        maxPeak = juce::jmax(maxPeak, peak);

                    auto sourceDurationSeconds = clip.sourceDurationSeconds > 0.0
                                                   ? clip.sourceDurationSeconds
                                                   : juce::jmax(clip.sourceStartSeconds + clip.durationSeconds, clip.durationSeconds);
                    auto sourceStartRatio = juce::jlimit(0.0, 1.0, clip.sourceStartSeconds / juce::jmax(0.001, sourceDurationSeconds));
                    auto sourceEndRatio = juce::jlimit(sourceStartRatio, 1.0, (clip.sourceStartSeconds + clip.durationSeconds)
                                                                       / juce::jmax(0.001, sourceDurationSeconds));
                    auto sourceStartPeak = juce::roundToInt(sourceStartRatio * static_cast<double>(clip.peaks.size() - 1));
                    auto sourceEndPeak = juce::roundToInt(sourceEndRatio * static_cast<double>(clip.peaks.size() - 1));
                    auto sourcePeakSpan = juce::jmax(1, sourceEndPeak - sourceStartPeak);

                    for (int x = 0; x < waveformArea.getWidth(); ++x)
                    {
                        auto peakIndex = juce::jlimit(0,
                                                      (int) clip.peaks.size() - 1,
                                                      sourceStartPeak + x * sourcePeakSpan / juce::jmax(1, waveformArea.getWidth()));
                        auto peak = juce::jlimit(0.0f, 1.0f, clip.peaks[(size_t) peakIndex] / maxPeak);
                        auto halfHeight = juce::roundToInt((float) waveformArea.getHeight() * 0.5f * peak);
                        g.drawVerticalLine(waveformArea.getX() + x,
                                           (float) waveformArea.getCentreY() - halfHeight,
                                           (float) waveformArea.getCentreY() + halfHeight);
                    }
                }
                else
                {
                    g.drawHorizontalLine(waveformArea.getCentreY(), (float) waveformArea.getX(), (float) waveformArea.getRight());
                }

                g.setFont(juce::Font(12.0f).boldened());
                g.setColour(juce::Colours::white);
                auto clipTitle = clip.displayName.isNotEmpty() ? clip.displayName : clip.file.getFileName();
                g.drawText(clip.recording ? "Recording..." : clipTitle,
                           clipBounds.reduced(8, 4).removeFromTop(16),
                           juce::Justification::centredLeft,
                           true);
            }
        }
    }

    if (timelineModel != nullptr)
    {
        auto loopStart = timelineModel->getLoopStartSeconds();
        auto loopEnd = timelineModel->getLoopEndSeconds();
        if (loopEnd > loopStart)
        {
            auto startX = xForTimelineSeconds(loopStart);
            auto endX = xForTimelineSeconds(loopEnd);
            auto bandLeft = juce::jmax(labelWidth, startX);
            auto bandRight = juce::jmin(getWidth(), endX);
            if (bandRight > bandLeft)
            {
                g.setColour(juce::Colour(timelineModel->isLoopEnabled() ? 0x4067e8a5 : 0x2067e8a5));
                g.fillRect(bandLeft, 0, bandRight - bandLeft, getHeight());
                g.setColour(juce::Colour(0xff67e8a5));
                g.drawVerticalLine(startX, 0.0f, static_cast<float>(rulerHeight));
                g.drawVerticalLine(endX, 0.0f, static_cast<float>(rulerHeight));
            }
        }

        if (draggingLoopRegion && loopRegionMoved)
        {
            auto previewStartX = xForTimelineSeconds(juce::jmin(loopDragStartSeconds, loopDragCurrentSeconds));
            auto previewEndX = xForTimelineSeconds(juce::jmax(loopDragStartSeconds, loopDragCurrentSeconds));
            g.setColour(juce::Colour(0x60ffd166));
            g.fillRect(juce::jmax(labelWidth, previewStartX), 0, juce::jmax(0, previewEndX - previewStartX), getHeight());
        }

        for (const auto& marker : timelineModel->getMarkers())
        {
            auto x = xForTimelineSeconds(marker.seconds);
            if (x < labelWidth || x > getWidth())
                continue;

            g.setColour(juce::Colour(0xffff9f6e));
            juce::Path flag;
            flag.addTriangle((float) x, 4.0f, (float) x, 20.0f, (float) x + 10.0f, 12.0f);
            g.fillPath(flag);
            g.drawVerticalLine(x, 20.0f, static_cast<float>(rulerHeight));

            g.setFont(juce::Font(10.0f));
            g.drawText(marker.name, x + 12, 4, 90, 16, juce::Justification::centredLeft, true);
        }
    }

    if (timelineModel != nullptr)
    {
        auto timelineOriginX = labelWidth + 12;
        auto playheadX = timelineOriginX + juce::roundToInt((timelineModel->getTransportSeconds() - scrollSeconds)
                                                            * timelineModel->getPixelsPerSecond());
        if (playheadX >= timelineOriginX && playheadX <= getWidth())
        {
            g.setColour(juce::Colour(0xffffd166));
            g.drawVerticalLine(playheadX, 0.0f, static_cast<float>(getHeight()));
            g.fillEllipse(static_cast<float>(playheadX - 5), 4.0f, 10.0f, 10.0f);
        }
    }
}

void TrackerPanel::TimelineCanvas::resized()
{
    constexpr int rulerHeight = 56;
    auto labelWidth = juce::jmin(340, juce::jmax(290, getWidth() / 4));

    for (int trackIndex = 0; trackIndex < trackHeaders.size(); ++trackIndex)
    {
        if (auto* header = trackHeaders[trackIndex])
            header->setBounds(0, rulerHeight + trackIndex * laneHeight, labelWidth, laneHeight);
    }
}

void TrackerPanel::TimelineCanvas::mouseDown(const juce::MouseEvent& event)
{
    if (trackCount <= 0)
        return;

    auto laneStart = 56;
    auto labelWidth = juce::jmin(340, juce::jmax(290, getWidth() / 4));

    if (event.x >= labelWidth + 12 && timelineModel != nullptr && event.y < laneStart)
    {
        auto markerId = hitTestMarker(event.x);
        if (markerId.isNotEmpty())
        {
            if (onMarkerClicked)
                onMarkerClicked(markerId);
            return;
        }

        draggingLoopRegion = true;
        loopRegionMoved = false;
        loopDragStartSeconds = xToTimelineSeconds(event.x);
        loopDragCurrentSeconds = loopDragStartSeconds;
        repaint();
        return;
    }

    if (event.x >= labelWidth + 12 && timelineModel != nullptr)
    {
        draggingClipIndex = hitTestClip(event.position.toInt());
        if (draggingClipIndex >= 0)
        {
            const auto& clip = timelineModel->getClips()[(size_t) draggingClipIndex];
            selectedClipIndex = draggingClipIndex;
            draggingOriginalTrack = clip.trackIndex;
            draggingOriginalStartSeconds = clip.startSeconds;
            draggingClipMoved = false;

            selectedTrack = clip.trackIndex;
            if (onTrackSelected)
                onTrackSelected(clip.trackIndex);
            if (onClipSelected)
                onClipSelected(draggingClipIndex);

            if (event.mods.isPopupMenu())
            {
                juce::PopupMenu menu;
                menu.addItem(1, "Rename clip");
                menu.addSeparator();
                menu.addItem(2, "Split at playhead");
                menu.addSeparator();
                menu.addItem(3, "Duplicate clip");
                menu.addItem(4, "Delete clip");
                auto clickArea = juce::Rectangle<int>(event.x, event.y, 1, 1);
                menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                                                            .withTargetScreenArea(localAreaToGlobal(clickArea)),
                                    [safe = juce::Component::SafePointer<TimelineCanvas>(this),
                                     clipIndex = draggingClipIndex](int result)
                                    {
                                        if (safe == nullptr)
                                            return;

                                        if (result == 1 && safe->onClipRenameRequested)
                                            safe->onClipRenameRequested(clipIndex);
                                        else if (result == 2 && safe->onClipSplitRequested)
                                            safe->onClipSplitRequested(clipIndex, safe->getTransportSeconds());
                                        else if (result == 3 && safe->onClipDuplicateRequested)
                                            safe->onClipDuplicateRequested(clipIndex);
                                        else if (result == 4 && safe->onClipDeleteRequested)
                                            safe->onClipDeleteRequested(clipIndex);
                                    });
                draggingClipIndex = -1;
                repaint();
                return;
            }

            repaint();
            return;
        }

        selectedClipIndex = -1;
        if (onClipSelected)
            onClipSelected(-1);

        auto seconds = xToTimelineSeconds(event.x);
        if (onPlayheadPositionChanged)
            onPlayheadPositionChanged(seconds);
        repaint();
    }

    if (event.y < laneStart)
        return;

    auto trackIndex = (event.y - laneStart) / laneHeight;
    if (! juce::isPositiveAndBelow(trackIndex, trackCount))
        return;

    selectedTrack = trackIndex;
    repaint();

    if (onTrackSelected)
        onTrackSelected(trackIndex);
}

void TrackerPanel::TimelineCanvas::mouseDrag(const juce::MouseEvent& event)
{
    auto labelWidth = juce::jmin(340, juce::jmax(290, getWidth() / 4));
    if (timelineModel == nullptr)
        return;

    if (draggingLoopRegion)
    {
        loopDragCurrentSeconds = xToTimelineSeconds(event.x);
        loopRegionMoved = true;
        repaint();
        return;
    }

    if (draggingClipIndex >= 0)
    {
        const auto pixelsPerSecond = juce::jmax(1.0, timelineModel->getPixelsPerSecond());
        const auto deltaSeconds = static_cast<double>(event.getDistanceFromDragStartX()) / pixelsPerSecond;
        const auto newStartSeconds = juce::jmax(0.0, draggingOriginalStartSeconds + deltaSeconds);
        const auto newTrackIndex = yToTrackIndex(event.y);

        if (juce::isPositiveAndBelow(newTrackIndex, trackCount))
        {
            selectedTrack = newTrackIndex;
            draggingClipMoved = true;

            if (onTrackSelected)
                onTrackSelected(newTrackIndex);
            if (onClipMoved)
                onClipMoved(draggingClipIndex, newTrackIndex, newStartSeconds);
        }

        repaint();
        return;
    }

    if (event.x < labelWidth + 12)
        return;

    auto seconds = xToTimelineSeconds(event.x);
    if (onPlayheadPositionChanged)
        onPlayheadPositionChanged(seconds);
    repaint();
}

void TrackerPanel::TimelineCanvas::mouseUp(const juce::MouseEvent&)
{
    if (draggingLoopRegion)
    {
        if (loopRegionMoved && std::abs(loopDragCurrentSeconds - loopDragStartSeconds) > 0.05)
        {
            if (onLoopRegionChanged)
                onLoopRegionChanged(loopDragStartSeconds, loopDragCurrentSeconds);
        }
        else if (onPlayheadPositionChanged)
        {
            onPlayheadPositionChanged(loopDragStartSeconds);
        }

        draggingLoopRegion = false;
        loopRegionMoved = false;
        repaint();
        return;
    }

    if (draggingClipIndex >= 0)
    {
        if (draggingClipMoved && onClipMoveCommitted)
            onClipMoveCommitted();

        draggingClipIndex = -1;
        draggingOriginalTrack = -1;
        draggingOriginalStartSeconds = 0.0;
        draggingClipMoved = false;
        repaint();
    }
}

double TrackerPanel::TimelineCanvas::xToTimelineSeconds(int x) const noexcept
{
    auto labelWidth = juce::jmin(340, juce::jmax(290, getWidth() / 4));
    auto timelineOriginX = labelWidth + 12;
    auto pixelsPerSecond = timelineModel != nullptr ? timelineModel->getPixelsPerSecond() : 120.0;
    return juce::jmax(0.0, scrollSeconds + static_cast<double>(x - timelineOriginX) / juce::jmax(1.0, pixelsPerSecond));
}

int TrackerPanel::TimelineCanvas::xForTimelineSeconds(double seconds) const noexcept
{
    auto labelWidth = juce::jmin(340, juce::jmax(290, getWidth() / 4));
    auto timelineOriginX = labelWidth + 12;
    auto pixelsPerSecond = timelineModel != nullptr ? timelineModel->getPixelsPerSecond() : 120.0;
    return timelineOriginX + juce::roundToInt((seconds - scrollSeconds) * pixelsPerSecond);
}

int TrackerPanel::TimelineCanvas::yToTrackIndex(int y) const noexcept
{
    constexpr int laneStart = 56;
    return (y - laneStart) / juce::jmax(1, laneHeight);
}

int TrackerPanel::TimelineCanvas::hitTestClip(juce::Point<int> position) const
{
    if (timelineModel == nullptr)
        return -1;

    constexpr int rulerHeight = 56;
    auto labelWidth = juce::jmin(340, juce::jmax(290, getWidth() / 4));
    auto timelineOriginX = labelWidth + 12;
    auto pixelsPerSecond = timelineModel->getPixelsPerSecond();
    const auto& clips = timelineModel->getClips();

    for (auto clipIndex = static_cast<int>(clips.size()) - 1; clipIndex >= 0; --clipIndex)
    {
        const auto& clip = clips[(size_t) clipIndex];
        if (clip.recording || ! juce::isPositiveAndBelow(clip.trackIndex, trackCount))
            continue;

        auto lane = juce::Rectangle<int>(labelWidth,
                                         rulerHeight + clip.trackIndex * laneHeight,
                                         getWidth() - labelWidth - 4,
                                         laneHeight).reduced(0, 6);
        auto timelineLane = lane.withTrimmedLeft(12);
        auto clipX = timelineOriginX + juce::roundToInt((clip.startSeconds - scrollSeconds) * pixelsPerSecond);
        auto clipWidth = juce::jmax(8, juce::roundToInt(clip.durationSeconds * pixelsPerSecond));
        auto clipBounds = juce::Rectangle<int>(clipX, timelineLane.getY(), clipWidth, timelineLane.getHeight()).reduced(2, 4);

        if (clipBounds.contains(position))
            return clipIndex;
    }

    return -1;
}

juce::String TrackerPanel::TimelineCanvas::hitTestMarker(int x) const
{
    if (timelineModel == nullptr)
        return {};

    constexpr int hitRadius = 8;
    for (const auto& marker : timelineModel->getMarkers())
    {
        auto markerX = xForTimelineSeconds(marker.seconds);
        if (std::abs(x - markerX) <= hitRadius)
            return marker.id;
    }

    return {};
}

TrackerPanel::TimelineCanvas::TrackHeader::TrackHeader(int newTrackIndex)
    : trackIndex(newTrackIndex)
{
    nameEditor.setText(makeDefaultTrackName(trackIndex), juce::dontSendNotification);
    nameEditor.setSelectAllWhenFocused(true);
    nameEditor.onReturnKey = [this]
    {
        if (onNameChanged)
            onNameChanged(trackIndex, nameEditor.getText().trim());
    };
    nameEditor.onFocusLost = [this]
    {
        if (onNameChanged)
            onNameChanged(trackIndex, nameEditor.getText().trim());
    };
    addAndMakeVisible(nameEditor);

    kindButton.setButtonText(cs::toDisplayName(trackKind));
    kindButton.setTooltip("Choose what kind of material this track contains");
    kindButton.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Audio", true, trackKind == cs::TrackKind::audio);
        menu.addItem(2, "MIDI", true, trackKind == cs::TrackKind::midi);
        menu.addItem(3, "Automation", true, trackKind == cs::TrackKind::automation);
        menu.addItem(4, "Signal", true, trackKind == cs::TrackKind::signal);
        menu.addItem(5, "Foley", true, trackKind == cs::TrackKind::foley);
        menu.addItem(6, "Folder", true, trackKind == cs::TrackKind::folder);
        menu.addItem(7, "Marker", true, trackKind == cs::TrackKind::marker);

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&kindButton),
                           [this](int result)
                           {
                               if (result <= 0)
                                   return;

                               auto newKind = cs::TrackKind::audio;
                               switch (result)
                               {
                                   case 2: newKind = cs::TrackKind::midi; break;
                                   case 3: newKind = cs::TrackKind::automation; break;
                                   case 4: newKind = cs::TrackKind::signal; break;
                                   case 5: newKind = cs::TrackKind::foley; break;
                                   case 6: newKind = cs::TrackKind::folder; break;
                                   case 7: newKind = cs::TrackKind::marker; break;
                                   default: break;
                               }

                               setTrackKind(newKind);
                               if (onKindChanged)
                                   onKindChanged(trackIndex, newKind);
                           });
    };
    addAndMakeVisible(kindButton);

    auto setupToggle = [this](juce::TextButton& button, juce::Colour colour, const juce::String& tooltip)
    {
        button.setClickingTogglesState(true);
        button.setLookAndFeel(&getTrackIconButtonLookAndFeel());
        button.setTooltip(tooltip);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263341));
        button.setColour(juce::TextButton::buttonOnColourId, colour);
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd7e4f5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        addAndMakeVisible(button);
    };

    setupToggle(armButton, juce::Colour(0xffff4f5f), "Arm this track for recording");
    setupToggle(muteButton, juce::Colour(0xffffc857), "Mute this track");
    setupToggle(soloButton, juce::Colour(0xff67e8a5), "Solo this track");
    setupToggle(monitorButton, juce::Colour(0xff5da5ff), "Input monitor: hear this track while recording");
    armButton.setButtonText("arm");
    muteButton.setButtonText("mute");
    soloButton.setButtonText("solo");
    monitorButton.setButtonText("monitor");

    armButton.onClick = [this]
    {
        if (onArmChanged)
            onArmChanged(trackIndex, armButton.getToggleState());
    };
    muteButton.onClick = [this]
    {
        if (onMuteChanged)
            onMuteChanged(trackIndex, muteButton.getToggleState());
    };
    soloButton.onClick = [this]
    {
        if (onSoloChanged)
            onSoloChanged(trackIndex, soloButton.getToggleState());
    };
    monitorButton.onClick = [this]
    {
        if (onMonitorChanged)
            onMonitorChanged(trackIndex, monitorButton.getToggleState());
    };

    stereoButton.setClickingTogglesState(true);
    stereoButton.setTooltip("Track channel mode: Mono records one input, ST records this input plus the next input as stereo.");
    stereoButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263341));
    stereoButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff74caff));
    stereoButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd7e4f5));
    stereoButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    stereoButton.onClick = [this]
    {
        setStereo(stereoButton.getToggleState());
        if (onStereoChanged)
            onStereoChanged(trackIndex, stereoButton.getToggleState());
    };
    addAndMakeVisible(stereoButton);

    inputSelector.setTooltip("Audio input source for this track");
    inputSelector.onChange = [this]
    {
        if (onInputChanged)
            onInputChanged(trackIndex, inputSelector.getSelectedId() - 1);
    };
    addAndMakeVisible(inputSelector);

    fxButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263341));
    fxButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff8fa0b8));
    fxButton.setTooltip("No plugins loaded - click to open this track's effects chain");
    fxButton.onClick = [this]
    {
        if (onFxRequested)
            onFxRequested(trackIndex);
    };
    addAndMakeVisible(fxButton);

    dbLabel.setText("-inf dB", juce::dontSendNotification);
    dbLabel.setJustificationType(juce::Justification::centredRight);
    dbLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));

    gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainSlider.setRange(0.0, 1.0, 0.001);
    gainSlider.setValue(0.75, juce::dontSendNotification);
    gainSlider.onValueChange = [this]
    {
        if (onGainChanged)
            onGainChanged(trackIndex, static_cast<float>(gainSlider.getValue()));
    };
    addAndMakeVisible(gainSlider);
}

void TrackerPanel::TimelineCanvas::TrackHeader::setTrackIndex(int newTrackIndex)
{
    trackIndex = newTrackIndex;
}

void TrackerPanel::TimelineCanvas::TrackHeader::setTrackName(const juce::String& name)
{
    nameEditor.setText(makeTrackLabel(trackIndex, name), juce::dontSendNotification);
}

void TrackerPanel::TimelineCanvas::TrackHeader::setTrackKind(cs::TrackKind kind)
{
    trackKind = kind;
    kindButton.setButtonText(cs::toDisplayName(kind));
    auto colour = juce::Colour(0xff74caff);

    switch (kind)
    {
        case cs::TrackKind::audio: colour = juce::Colour(0xff74caff); break;
        case cs::TrackKind::midi: colour = juce::Colour(0xffb185ff); break;
        case cs::TrackKind::automation: colour = juce::Colour(0xffffd166); break;
        case cs::TrackKind::signal: colour = juce::Colour(0xff67e8a5); break;
        case cs::TrackKind::foley: colour = juce::Colour(0xffff9f6e); break;
        case cs::TrackKind::folder: colour = juce::Colour(0xff9fb0c8); break;
        case cs::TrackKind::marker: colour = juce::Colour(0xffff6b6b); break;
    }

    kindButton.setColour(juce::TextButton::buttonColourId, colour.withAlpha(0.85f));
    kindButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff11161e));
}

void TrackerPanel::TimelineCanvas::TrackHeader::setSelected(bool shouldSelect)
{
    selected = shouldSelect;
    repaint();
}

void TrackerPanel::TimelineCanvas::TrackHeader::setArmed(bool armed)
{
    armButton.setToggleState(armed, juce::dontSendNotification);
    armButton.setButtonText("arm");
    repaint();
}

void TrackerPanel::TimelineCanvas::TrackHeader::setMuted(bool muted)
{
    muteButton.setToggleState(muted, juce::dontSendNotification);
}

void TrackerPanel::TimelineCanvas::TrackHeader::setSoloed(bool soloed)
{
    soloButton.setToggleState(soloed, juce::dontSendNotification);
}

void TrackerPanel::TimelineCanvas::TrackHeader::setMonitored(bool monitored)
{
    monitorButton.setToggleState(monitored, juce::dontSendNotification);
    monitorButton.setTooltip(monitored ? "Input monitor is on: live input is routed to the speakers"
                                       : "Input monitor is off: this track can record silently");
}

void TrackerPanel::TimelineCanvas::TrackHeader::setStereo(bool stereo)
{
    stereoButton.setToggleState(stereo, juce::dontSendNotification);
    stereoButton.setButtonText(stereo ? "ST" : "Mono");
}

void TrackerPanel::TimelineCanvas::TrackHeader::setLevel(float level)
{
    inputLevel = juce::jlimit(0.0f, 1.0f, level);
    auto db = levelToDb(inputLevel);
    dbLabel.setText(db <= -60.0f ? "-inf dB" : juce::String(db, 1) + " dB", juce::dontSendNotification);
    repaint();
}

void TrackerPanel::TimelineCanvas::TrackHeader::setGain(float gain)
{
    gainSlider.setValue(juce::jlimit(0.0f, 1.0f, gain), juce::dontSendNotification);
}

void TrackerPanel::TimelineCanvas::TrackHeader::setFxSummary(int pluginCount)
{
    const bool hasPlugins = pluginCount > 0;
    fxButton.setColour(juce::TextButton::buttonColourId,
                       hasPlugins ? juce::Colour(0xff1e3629) : juce::Colour(0xff263341));
    fxButton.setColour(juce::TextButton::textColourOffId,
                       hasPlugins ? juce::Colour(0xff67e8a5) : juce::Colour(0xff8fa0b8));
    fxButton.setTooltip(hasPlugins
                             ? (juce::String(pluginCount) + (pluginCount == 1 ? " plugin loaded - click to edit"
                                                                              : " plugins loaded - click to edit"))
                             : "No plugins loaded - click to open this track's effects chain");
}

void TrackerPanel::TimelineCanvas::TrackHeader::setInputSources(const juce::Array<juce::String>& sourceNames)
{
    auto selectedId = inputSelector.getSelectedId();
    inputSelector.clear(juce::dontSendNotification);

    if (sourceNames.isEmpty())
    {
        inputSelector.addItem("No input", 1);
        inputSelector.setSelectedId(1, juce::dontSendNotification);
        return;
    }

    for (int index = 0; index < sourceNames.size(); ++index)
        inputSelector.addItem(sourceNames[index], index + 1);

    inputSelector.setSelectedId(juce::jlimit(1, sourceNames.size(), selectedId <= 0 ? 1 : selectedId), juce::dontSendNotification);
}

void TrackerPanel::TimelineCanvas::TrackHeader::setInputChannel(int inputChannel)
{
    inputSelector.setSelectedId(inputChannel + 1, juce::dontSendNotification);
}

void TrackerPanel::TimelineCanvas::TrackHeader::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(selected ? juce::Colour(0xff223653) : juce::Colour(0xff17202c));
    g.fillRect(bounds);
    g.setColour(selected ? juce::Colour(0xff74caff) : juce::Colour(0xff2d3b51));
    g.drawRect(getLocalBounds(), selected ? 2 : 1);

    auto badge = getLocalBounds().reduced(8, 8).removeFromLeft(6).toFloat();
    g.setColour(selected ? juce::Colour(0xff74caff) : juce::Colour(0xff33445a));
    g.fillRoundedRectangle(badge, 3.0f);

    auto meter = getLocalBounds().removeFromRight(34).reduced(3, 12).toFloat();
    g.setColour(juce::Colour(0xff101820));
    g.fillRoundedRectangle(meter, 2.0f);
    auto db = levelToDb(inputLevel);
    auto meterAmount = dbToMeterAmount(db);
    g.setColour(colourForMeterDb(db));
    auto meterFill = meter.withTrimmedTop(meter.getHeight() * (1.0f - meterAmount));
    g.fillRoundedRectangle(meterFill, 2.0f);
    drawDbMeterTicks(g, meter);
}

void TrackerPanel::TimelineCanvas::TrackHeader::paintOverChildren(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void TrackerPanel::TimelineCanvas::TrackHeader::resized()
{
    auto area = getLocalBounds().reduced(14, 8);
    area.removeFromRight(36);

    auto top = area.removeFromTop(26);
    stereoButton.setBounds(top.removeFromRight(48));
    top.removeFromRight(6);
    kindButton.setBounds(top.removeFromRight(96));
    top.removeFromRight(6);
    nameEditor.setBounds(top);

    area.removeFromTop(8);
    auto buttons = area.removeFromTop(28);
    armButton.setBounds(buttons.removeFromLeft(27));
    buttons.removeFromLeft(4);
    muteButton.setBounds(buttons.removeFromLeft(27));
    buttons.removeFromLeft(4);
    soloButton.setBounds(buttons.removeFromLeft(27));
    buttons.removeFromLeft(4);
    monitorButton.setBounds(buttons.removeFromLeft(27));
    buttons.removeFromLeft(4);
    fxButton.setBounds(buttons.removeFromLeft(30));
    buttons.removeFromLeft(8);
    inputSelector.setBounds(buttons);

    // The gain slider is the lowest-priority row: show it only when there's genuinely
    // legible room left (Comfort/Tall), rather than letting removeFromTop silently hand
    // it a zero-height sliver in Compact.
    area.removeFromTop(8);
    constexpr int minimumLegibleGainHeight = 14;
    if (area.getHeight() >= minimumLegibleGainHeight)
    {
        gainSlider.setVisible(true);
        gainSlider.setBounds(area.removeFromTop(juce::jmin(area.getHeight(), 22)));
    }
    else
    {
        gainSlider.setVisible(false);
    }
}

void TrackerPanel::TimelineCanvas::TrackHeader::mouseDown(const juce::MouseEvent&)
{
    if (onSelected)
        onSelected(trackIndex);
}
