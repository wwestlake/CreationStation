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
    addAndMakeVisible(compactButton);
    addAndMakeVisible(comfortButton);
    addAndMakeVisible(tallButton);
    addAndMakeVisible(zoomOutButton);
    addAndMakeVisible(zoomInButton);
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
    canvas.onPlayheadPositionChanged = [this](double seconds)
    {
        if (onPlayheadPositionChanged)
            onPlayheadPositionChanged(seconds);
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

void TrackerPanel::setTrackLevel(int trackIndex, float level)
{
    canvas.setTrackLevel(trackIndex, level);
}

void TrackerPanel::setTrackGain(int trackIndex, float gain)
{
    canvas.setTrackGain(trackIndex, gain);
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
    selectionLabel.setBounds(header);

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
    trackMuted.resize((size_t) trackCount, false);
    trackSoloed.resize((size_t) trackCount, false);
    trackMonitored.resize((size_t) trackCount, false);
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
    auto labelWidth = juce::jmin(340, juce::jmax(260, getWidth() / 4));
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

    constexpr int beatsPerMeasure = 4;
    constexpr int rulerHeight = 56;

    auto labelWidth = juce::jmin(340, juce::jmax(260, bounds.getWidth() / 4));
    auto timelineWidth = juce::jmax(1, getWidth() - labelWidth - 20);
    auto tempoBpm = timelineModel != nullptr ? timelineModel->getTempoBpm() : 120.0;
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
    g.drawText(juce::String(tempoBpm, 1) + " BPM  |  4/4", 12, 0, labelWidth - 24, 22, juce::Justification::centredLeft, true);
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
            for (const auto& clip : timelineModel->getClips())
            {
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
                g.setColour(clip.recording ? juce::Colour(0xffff6b6b) : juce::Colour(0xff74e0ff));
                g.drawRoundedRectangle(clipBounds.toFloat(), 7.0f, 1.5f);

                auto waveformArea = clipBounds.reduced(8, 18);
                g.setColour(juce::Colour(0xffd7f8ff));

                if (! clip.peaks.empty())
                {
                    auto maxPeak = 0.01f;
                    for (auto peak : clip.peaks)
                        maxPeak = juce::jmax(maxPeak, peak);

                    for (int x = 0; x < waveformArea.getWidth(); ++x)
                    {
                        auto peakIndex = juce::jlimit(0, (int) clip.peaks.size() - 1, x * (int) clip.peaks.size() / juce::jmax(1, waveformArea.getWidth()));
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
                g.drawText(clip.recording ? "Recording…" : clip.file.getFileName(),
                           clipBounds.reduced(8, 4).removeFromTop(16),
                           juce::Justification::centredLeft,
                           true);
            }
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
    auto labelWidth = juce::jmin(340, juce::jmax(260, getWidth() / 4));

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
    auto labelWidth = juce::jmin(340, juce::jmax(260, getWidth() / 4));

    if (event.x >= labelWidth + 12 && timelineModel != nullptr)
    {
        draggingClipIndex = hitTestClip(event.position.toInt());
        if (draggingClipIndex >= 0)
        {
            const auto& clip = timelineModel->getClips()[(size_t) draggingClipIndex];
            draggingOriginalTrack = clip.trackIndex;
            draggingOriginalStartSeconds = clip.startSeconds;
            draggingClipMoved = false;

            selectedTrack = clip.trackIndex;
            if (onTrackSelected)
                onTrackSelected(clip.trackIndex);

            repaint();
            return;
        }

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
    auto labelWidth = juce::jmin(340, juce::jmax(260, getWidth() / 4));
    if (timelineModel == nullptr)
        return;

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
    auto labelWidth = juce::jmin(340, juce::jmax(260, getWidth() / 4));
    auto timelineOriginX = labelWidth + 12;
    auto pixelsPerSecond = timelineModel != nullptr ? timelineModel->getPixelsPerSecond() : 120.0;
    return juce::jmax(0.0, scrollSeconds + static_cast<double>(x - timelineOriginX) / juce::jmax(1.0, pixelsPerSecond));
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
    auto labelWidth = juce::jmin(340, juce::jmax(260, getWidth() / 4));
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

    auto setupToggle = [this](juce::TextButton& button, juce::Colour colour, const juce::String& tooltip)
    {
        button.setClickingTogglesState(true);
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

    stereoLabel.setText("ST", juce::dontSendNotification);
    stereoLabel.setJustificationType(juce::Justification::centred);
    stereoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));
    addAndMakeVisible(stereoLabel);

    inputSelector.setTooltip("Audio input source for this track");
    inputSelector.onChange = [this]
    {
        if (onInputChanged)
            onInputChanged(trackIndex, inputSelector.getSelectedId() - 1);
    };
    addAndMakeVisible(inputSelector);

    fxLabel.setText("FX  +", juce::dontSendNotification);
    fxLabel.setJustificationType(juce::Justification::centredLeft);
    fxLabel.setColour(juce::Label::textColourId, juce::Colour(0xff67e8a5));
    fxLabel.setTooltip("Track effects bay placeholder");
    addAndMakeVisible(fxLabel);

    dbLabel.setText("-inf dB", juce::dontSendNotification);
    dbLabel.setJustificationType(juce::Justification::centredRight);
    dbLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));
    addAndMakeVisible(dbLabel);

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

void TrackerPanel::TimelineCanvas::TrackHeader::setSelected(bool shouldSelect)
{
    selected = shouldSelect;
    repaint();
}

void TrackerPanel::TimelineCanvas::TrackHeader::setArmed(bool armed)
{
    armButton.setToggleState(armed, juce::dontSendNotification);
    armButton.setButtonText(armed ? "ARM" : "Arm");
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

void TrackerPanel::TimelineCanvas::TrackHeader::setLevel(float level)
{
    inputLevel = juce::jlimit(0.0f, 1.0f, level);
    auto db = inputLevel > 0.000001f ? juce::Decibels::gainToDecibels(inputLevel) : -100.0f;
    dbLabel.setText(db <= -99.0f ? "-inf dB" : juce::String(db, 1) + " dB", juce::dontSendNotification);
    repaint();
}

void TrackerPanel::TimelineCanvas::TrackHeader::setGain(float gain)
{
    gainSlider.setValue(juce::jlimit(0.0f, 1.0f, gain), juce::dontSendNotification);
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

    auto meter = getLocalBounds().removeFromRight(10).reduced(2, 12).toFloat();
    g.setColour(juce::Colour(0xff101820));
    g.fillRoundedRectangle(meter, 2.0f);
    g.setColour(inputLevel > 0.75f ? juce::Colour(0xffff6b6b) : juce::Colour(0xff67e8a5));
    auto meterFill = meter.withTrimmedTop(meter.getHeight() * (1.0f - inputLevel));
    g.fillRoundedRectangle(meterFill, 2.0f);
}

void TrackerPanel::TimelineCanvas::TrackHeader::paintOverChildren(juce::Graphics& g)
{
    auto horizontalMeter = getLocalBounds().reduced(52, 8).removeFromBottom(7).toFloat();
    horizontalMeter.removeFromRight(86.0f);
    g.setColour(juce::Colour(0xff0b1118));
    g.fillRoundedRectangle(horizontalMeter, 3.0f);
    g.setColour(juce::Colour(0xff33445a));
    g.drawRoundedRectangle(horizontalMeter, 3.0f, 1.0f);

    auto fillWidth = horizontalMeter.getWidth() * inputLevel;
    if (fillWidth > 1.0f)
    {
        auto fill = horizontalMeter.withWidth(fillWidth);
        g.setColour(inputLevel > 0.75f ? juce::Colour(0xffff6b6b)
                                       : inputLevel > 0.35f ? juce::Colour(0xffffd166)
                                                            : juce::Colour(0xff67e8a5));
        g.fillRoundedRectangle(fill, 3.0f);
    }
}

void TrackerPanel::TimelineCanvas::TrackHeader::resized()
{
    auto area = getLocalBounds().reduced(14, 8);
    area.removeFromRight(12);

    auto top = area.removeFromTop(26);
    stereoLabel.setBounds(top.removeFromRight(36));
    top.removeFromRight(6);
    nameEditor.setBounds(top);

    area.removeFromTop(8);
    auto buttons = area.removeFromTop(28);
    armButton.setBounds(buttons.removeFromLeft(44));
    buttons.removeFromLeft(6);
    muteButton.setBounds(buttons.removeFromLeft(34));
    buttons.removeFromLeft(6);
    soloButton.setBounds(buttons.removeFromLeft(34));
    buttons.removeFromLeft(6);
    monitorButton.setBounds(buttons.removeFromLeft(44));
    buttons.removeFromLeft(8);
    inputSelector.setBounds(buttons);

    area.removeFromTop(8);
    auto gainRow = area.removeFromTop(22);
    dbLabel.setBounds(gainRow.removeFromRight(70));
    gainRow.removeFromRight(8);
    gainSlider.setBounds(gainRow);

    area.removeFromTop(4);
    fxLabel.setBounds(area.removeFromTop(22));
}

void TrackerPanel::TimelineCanvas::TrackHeader::mouseDown(const juce::MouseEvent&)
{
    if (onSelected)
        onSelected(trackIndex);
}
