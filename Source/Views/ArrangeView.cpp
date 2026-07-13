#include "ArrangeView.h"

namespace
{
juce::Colour panelColour() { return juce::Colour(0xff11151c); }
juce::Colour laneColour() { return juce::Colour(0xff1a2030); }
juce::Colour laneAltColour() { return juce::Colour(0xff181d2a); }
juce::Colour accentColour() { return juce::Colour(0xff5f93ff); }
juce::Colour clipColourForIndex(int index)
{
    static const juce::Colour colours[] =
    {
        juce::Colour(0xff5f93ff),
        juce::Colour(0xff7dd36f),
        juce::Colour(0xffd48a5f),
        juce::Colour(0xffb37df0),
        juce::Colour(0xff63d8d0)
    };

    return colours[(size_t) (index % (int) (sizeof(colours) / sizeof(colours[0])))];
}

constexpr int laneNameWidth = 180;
constexpr int laneBeatWidth = 90;

juce::Rectangle<int> getLaneClipArea(const juce::Component& lane)
{
    return lane.getLocalBounds().withTrimmedLeft(laneNameWidth).reduced(10, 16);
}

juce::Rectangle<int> getClipBoundsForState(const juce::Component& lane, int startBeat, int lengthBeats)
{
    auto clipArea = getLaneClipArea(lane);
    return juce::Rectangle<int>(clipArea.getX() + startBeat * laneBeatWidth + 4,
                                clipArea.getY() + 12,
                                juce::jmax(1, lengthBeats) * laneBeatWidth - 8,
                                clipArea.getHeight() - 24);
}
}

ArrangeView::Lane::Lane(int newTrackIndex, const juce::String& newTrackName)
    : trackIndex(newTrackIndex), trackName(newTrackName)
{
    setRepaintsOnMouseActivity(true);
}

void ArrangeView::Lane::ClipBlock::setClipName(const juce::String& name)
{
    clipName = name;
    repaint();
}

void ArrangeView::Lane::ClipBlock::setLaneIndex(int newLaneIndex)
{
    laneIndex = newLaneIndex;
}

void ArrangeView::Lane::ClipBlock::setStartBeat(int newStartBeat)
{
    startBeat = juce::jmax(0, newStartBeat);
    updateBoundsFromState();
}

void ArrangeView::Lane::ClipBlock::setLengthBeats(int newLengthBeats)
{
    lengthBeats = juce::jmax(1, newLengthBeats);
    updateBoundsFromState();
}

void ArrangeView::Lane::ClipBlock::setColour(juce::Colour newColour)
{
    colour = newColour;
    repaint();
}

void ArrangeView::Lane::ClipBlock::updateBoundsFromState()
{
    if (auto* parentLane = dynamic_cast<Lane*>(getParentComponent()))
        setBounds(getClipBoundsForState(*parentLane, startBeat, lengthBeats));
}

void ArrangeView::Lane::ClipBlock::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(colour.withAlpha(0.88f));
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(12.0f).boldened());
    g.drawText(clipName, getLocalBounds().reduced(10), juce::Justification::centredLeft, true);

    g.setColour(juce::Colours::white.withAlpha(0.12f));
    auto handle = getLocalBounds().removeFromRight(8).reduced(2, 8);
    g.fillRoundedRectangle(handle.toFloat(), 2.0f);
}

void ArrangeView::Lane::ClipBlock::mouseDown(const juce::MouseEvent& event)
{
    dragStart = event.getPosition();
    startBeatOnDrag = startBeat;
    toFront(true);
}

void ArrangeView::Lane::ClipBlock::mouseDrag(const juce::MouseEvent& event)
{
    auto* parentLane = dynamic_cast<Lane*>(getParentComponent());
    if (parentLane == nullptr)
        return;

    auto clipArea = getLaneClipArea(*parentLane);
    auto deltaBeats = juce::roundToInt((float) event.getDistanceFromDragStartX() / (float) laneBeatWidth);
    startBeat = juce::jmax(0, startBeatOnDrag + deltaBeats);

    auto newBounds = getClipBoundsForState(*parentLane, startBeat, lengthBeats);
    auto maxX = clipArea.getRight() - newBounds.getWidth();
    newBounds.setX(juce::jlimit(clipArea.getX(), maxX, newBounds.getX()));
    setBounds(newBounds);
}

void ArrangeView::Lane::ClipBlock::mouseUp(const juce::MouseEvent&)
{
    updateBoundsFromState();
    if (auto* parentLane = dynamic_cast<Lane*>(getParentComponent()))
        if (auto* parentCanvas = dynamic_cast<Canvas*>(parentLane->getParentComponent()))
            if (parentCanvas->onTrackSelected)
                parentCanvas->onTrackSelected(laneIndex);
}

void ArrangeView::Lane::setTrackIndex(int newTrackIndex)
{
    trackIndex = newTrackIndex;
    repaint();
}

void ArrangeView::Lane::setTrackName(const juce::String& newTrackName)
{
    trackName = newTrackName;
    repaint();
}

void ArrangeView::Lane::setClipNames(const juce::StringArray& newClipNames)
{
    clipNames = newClipNames;
    rebuildClipBlocks();
}

void ArrangeView::Lane::setSelected(bool shouldSelect)
{
    selected = shouldSelect;
    repaint();
}

void ArrangeView::Lane::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(trackIndex % 2 == 0 ? laneColour() : laneAltColour());
    g.fillRect(bounds);

    auto nameArea = getLocalBounds().removeFromLeft(180);
    g.setColour(juce::Colour(0xff222a3a));
    g.fillRect(nameArea);

    g.setColour(juce::Colour(0xffd7deea));
    g.setFont(juce::Font(15.0f).boldened());
    g.drawText(juce::String(trackIndex + 1) + ". " + trackName, nameArea.reduced(14, 0), juce::Justification::centredLeft, true);

    auto clipArea = getLaneClipArea(*this);
    g.setColour(juce::Colour(0xff283144));
    for (int beat = 0; beat < 24; ++beat)
    {
        auto x = clipArea.getX() + beat * laneBeatWidth;
        g.drawLine(static_cast<float>(x), static_cast<float>(clipArea.getY()), static_cast<float>(x), static_cast<float>(clipArea.getBottom()), 1.0f);
    }

    if (selected)
    {
        g.setColour(accentColour().withAlpha(0.8f));
        g.drawRect(getLocalBounds(), 2);
    }
}

void ArrangeView::Lane::mouseDown(const juce::MouseEvent&)
{
}

void ArrangeView::Lane::mouseUp(const juce::MouseEvent&)
{
    if (auto* parent = dynamic_cast<Canvas*>(getParentComponent()))
        if (parent->onTrackSelected)
            parent->onTrackSelected(trackIndex);
}

void ArrangeView::Lane::rebuildClipBlocks()
{
    clipBlocks.clear(true);

    juce::StringArray namesToUse = clipNames;
    if (namesToUse.isEmpty())
    {
        namesToUse.add("Intro");
        namesToUse.add("Build");
        namesToUse.add("Drop");
        namesToUse.add("Outro");
    }

    for (int clipIndex = 0; clipIndex < namesToUse.size(); ++clipIndex)
    {
        auto* block = new ClipBlock();
        block->setClipName(namesToUse[clipIndex]);
        block->setLaneIndex(trackIndex);
        block->setStartBeat(1 + clipIndex * 4);
        block->setLengthBeats(3 + (clipIndex % 3));
        block->setColour(clipColourForIndex(clipIndex));
        clipBlocks.add(block);
        addAndMakeVisible(block);
    }

    layoutClipBlocks();
}

void ArrangeView::Lane::layoutClipBlocks()
{
    for (auto* block : clipBlocks)
        block->updateBoundsFromState();
}

void ArrangeView::Canvas::setTrackCount(int newTrackCount, const juce::StringArray& newTrackNames)
{
    trackCount = juce::jmax(0, newTrackCount);
    trackNames = newTrackNames;

    while (lanes.size() < trackCount)
        lanes.add(new Lane(lanes.size(), "Track " + juce::String(lanes.size() + 1)));

    while (lanes.size() > trackCount)
        lanes.removeLast();

    for (int index = 0; index < lanes.size(); ++index)
    {
        lanes[index]->setTrackIndex(index);
        if (juce::isPositiveAndBelow(index, trackNames.size()) && trackNames[index].isNotEmpty())
            lanes[index]->setTrackName(trackNames[index]);
        addAndMakeVisible(lanes[index]);
    }

    setSize(2400, 100 + trackCount * 88);
    rebuildLaneClips();
    resized();
}

void ArrangeView::Canvas::setTrackName(int trackIndex, const juce::String& name)
{
    if (juce::isPositiveAndBelow(trackIndex, lanes.size()))
        lanes[trackIndex]->setTrackName(name);
}

void ArrangeView::Canvas::setRecordedClips(const juce::StringArray& clipNames)
{
    recordedClips = clipNames;
    rebuildLaneClips();
}

void ArrangeView::Canvas::setSelectedTrack(int trackIndex)
{
    selectedTrack = trackIndex;

    for (int index = 0; index < lanes.size(); ++index)
        lanes[index]->setSelected(index == trackIndex);
}

void ArrangeView::Canvas::rebuildLaneClips()
{
    if (lanes.isEmpty())
        return;

    juce::Array<juce::StringArray> perLaneClips;
    perLaneClips.resize(lanes.size());

    for (int clipIndex = 0; clipIndex < recordedClips.size(); ++clipIndex)
    {
        auto laneIndex = clipIndex % lanes.size();
        perLaneClips[(size_t) laneIndex].add(recordedClips[clipIndex]);
    }

    for (int laneIndex = 0; laneIndex < lanes.size(); ++laneIndex)
        lanes[laneIndex]->setClipNames(perLaneClips[(size_t) laneIndex]);
}

void ArrangeView::Canvas::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());

    auto header = getLocalBounds().removeFromTop(44);
    g.setColour(juce::Colour(0xff1e2533));
    g.fillRect(header);

    auto ruler = header.withTrimmedLeft(180);
    g.setColour(juce::Colour(0xff8ea0b7));
    g.setFont(juce::Font(13.0f));

    auto beatWidth = 90;
    for (int beat = 0; beat < 24; ++beat)
    {
        auto x = ruler.getX() + beat * beatWidth;
        g.drawLine(static_cast<float>(x), static_cast<float>(ruler.getY()), static_cast<float>(x), static_cast<float>(getHeight()), 1.0f);
        g.drawText(juce::String(beat + 1), x + 4, ruler.getY() + 4, 40, 18, juce::Justification::centredLeft);
    }
}

void ArrangeView::Canvas::resized()
{
    auto y = 44;
    for (auto* lane : lanes)
    {
        lane->setBounds(0, y, getWidth(), 88);
        lane->layoutClipBlocks();
        y += 88;
    }
}

ArrangeView::ArrangeView()
{
    setName("DAW");

    titleLabel.setText("Arrange View", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Timeline, lanes, clips, and track selection.", juce::dontSendNotification);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(hintLabel);

    addTrackButton.onClick = [this]
    {
        if (visibleTrackCount < totalTrackCount)
        {
            ++visibleTrackCount;
            updateCanvasTrackCount();
        }

        if (onAddTrackRequested)
            onAddTrackRequested();
    };
    addAndMakeVisible(addTrackButton);

    viewport.setViewedComponent(&canvas, false);
    viewport.setScrollBarsShown(true, true);
    addAndMakeVisible(viewport);

    canvas.onTrackSelected = [this](int trackIndex)
    {
        setSelectedTrack(trackIndex);
        if (onTrackSelected)
            onTrackSelected(trackIndex);
    };

    updateCanvasTrackCount();
}

void ArrangeView::setTotalTrackCount(int newTrackCount)
{
    totalTrackCount = juce::jlimit(1, 128, newTrackCount);
    visibleTrackCount = juce::jmin(visibleTrackCount, totalTrackCount);
    juce::StringArray resizedNames;
    for (int index = 0; index < totalTrackCount; ++index)
    {
        if (juce::isPositiveAndBelow(index, trackNames.size()))
            resizedNames.add(trackNames[index]);
        else
            resizedNames.add({});
    }

    trackNames = resizedNames;
    updateCanvasTrackCount();
}

void ArrangeView::setVisibleTrackCount(int newVisibleTrackCount)
{
    visibleTrackCount = juce::jlimit(1, totalTrackCount, newVisibleTrackCount);
    updateCanvasTrackCount();
}

void ArrangeView::setTrackName(int trackIndex, const juce::String& name)
{
    if (juce::isPositiveAndBelow(trackIndex, trackNames.size()))
        trackNames.set(trackIndex, name);
    canvas.setTrackName(trackIndex, name);
}

void ArrangeView::setRecordedClips(const juce::StringArray& clipNames)
{
    canvas.setRecordedClips(clipNames);
}

void ArrangeView::setSelectedTrack(int trackIndex)
{
    canvas.setSelectedTrack(trackIndex);
}

void ArrangeView::updateCanvasTrackCount()
{
    canvas.setTrackCount(visibleTrackCount, trackNames);
    viewport.setViewPosition(0, 0);
}

void ArrangeView::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());
}

void ArrangeView::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(32));
    hintLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(8);
    addTrackButton.setBounds(area.removeFromTop(34).removeFromRight(160));
    area.removeFromTop(8);
    viewport.setBounds(area);
    canvas.setSize(2400, 44 + visibleTrackCount * 88);
}
