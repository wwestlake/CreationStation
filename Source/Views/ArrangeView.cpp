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

juce::String formatPercent(double value)
{
    return juce::String(juce::roundToInt(value * 100.0)) + "%";
}

juce::String formatDecibels(float value)
{
    return juce::String(value, 1) + " dB";
}
}

ArrangeView::WaveformPanel::WaveformPanel()
    : thumbnail(512, formatManager, thumbnailCache)
{
    formatManager.registerBasicFormats();
}

void ArrangeView::WaveformPanel::setAudioFile(const juce::File& file)
{
    thumbnail.clear();

    if (file.existsAsFile())
    {
        thumbnail.setSource(new juce::FileInputSource(file));
        emptyText = file.getFileName();
    }
    else
    {
        emptyText = "Select a project sound to shape it.";
    }

    repaint();
}

void ArrangeView::WaveformPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff171d27));
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(juce::Colour(0xff2d384a));
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);

    if (thumbnail.getTotalLength() > 0.0)
    {
        g.setColour(juce::Colour(0xff6fa8ff));
        thumbnail.drawChannels(g, getLocalBounds().reduced(10), 0.0, thumbnail.getTotalLength(), 1.0f);
    }
    else
    {
        g.setColour(juce::Colour(0xff8ea0b7));
        g.drawFittedText(emptyText, getLocalBounds().reduced(12), juce::Justification::centred, 2);
    }
}

ArrangeView::Lane::Lane(int newTrackIndex, const juce::String& newTrackName)
    : trackIndex(newTrackIndex), trackName(newTrackName)
{
    setRepaintsOnMouseActivity(true);
}

void ArrangeView::Lane::ClipBlock::setClipData(const ClipPlacement& placementData, int newModelIndex, juce::Colour newColour)
{
    placement = placementData;
    modelIndex = newModelIndex;
    colour = newColour;
    repaint();
}

void ArrangeView::Lane::ClipBlock::setSelected(bool shouldSelect)
{
    selected = shouldSelect;
    repaint();
}

void ArrangeView::Lane::ClipBlock::updateBoundsFromState()
{
    if (auto* parentLane = dynamic_cast<Lane*>(getParentComponent()))
        setBounds(getClipBoundsForState(*parentLane, placement.startBeat, placement.lengthBeats));
}

void ArrangeView::Lane::ClipBlock::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(colour.withAlpha(0.88f));
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);

    if (selected)
    {
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.drawRoundedRectangle(bounds.reduced(2.0f), 8.0f, 2.0f);
    }

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(12.0f).boldened());
    g.drawText(placement.displayName, getLocalBounds().reduced(10), juce::Justification::centredLeft, true);

    g.setColour(juce::Colours::white.withAlpha(0.12f));
    auto handle = getLocalBounds().removeFromRight(8).reduced(2, 8);
    g.fillRoundedRectangle(handle.toFloat(), 2.0f);
}

void ArrangeView::Lane::ClipBlock::mouseDown(const juce::MouseEvent& event)
{
    dragStart = event.getPosition();
    startBeatOnDrag = placement.startBeat;
    toFront(true);

    if (onSelected)
        onSelected(modelIndex);
}

void ArrangeView::Lane::ClipBlock::mouseDrag(const juce::MouseEvent& event)
{
    auto* parentLane = dynamic_cast<Lane*>(getParentComponent());
    if (parentLane == nullptr)
        return;

    auto clipArea = getLaneClipArea(*parentLane);
    auto deltaBeats = juce::roundToInt((float) event.getDistanceFromDragStartX() / (float) laneBeatWidth);
    placement.startBeat = juce::jmax(0, startBeatOnDrag + deltaBeats);

    auto newBounds = getClipBoundsForState(*parentLane, placement.startBeat, placement.lengthBeats);
    auto maxX = clipArea.getRight() - newBounds.getWidth();
    newBounds.setX(juce::jlimit(clipArea.getX(), maxX, newBounds.getX()));
    setBounds(newBounds);
}

void ArrangeView::Lane::ClipBlock::mouseUp(const juce::MouseEvent&)
{
    updateBoundsFromState();

    if (onMoved)
        onMoved(modelIndex, placement.startBeat);

    if (auto* parentLane = dynamic_cast<Lane*>(getParentComponent()))
        if (auto* parentCanvas = dynamic_cast<Canvas*>(parentLane->getParentComponent()))
            if (parentCanvas->onTrackSelected)
                parentCanvas->onTrackSelected(placement.trackIndex);
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

void ArrangeView::Lane::setClips(const juce::Array<LaneClipView>& newClips, int selectedClipIndex)
{
    clips = newClips;
    this->selectedClipIndex = selectedClipIndex;
    for (int index = 0; index < clips.size(); ++index)
        clips.getReference(index).colour = clipColourForIndex(index + trackIndex * 3);
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
        g.drawLine((float) x, (float) clipArea.getY(), (float) x, (float) clipArea.getBottom(), 1.0f);
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

    for (int clipIndex = 0; clipIndex < clips.size(); ++clipIndex)
    {
        auto* block = new ClipBlock();
        block->setClipData(clips.getReference(clipIndex).placement,
                           clips.getReference(clipIndex).modelIndex,
                           clips.getReference(clipIndex).colour);
        block->onMoved = onClipMoved;
        block->onSelected = onClipSelected;
        block->setSelected(clips.getReference(clipIndex).modelIndex == selectedClipIndex);
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
        lanes[index]->onClipMoved = onClipMoved;
        lanes[index]->onClipSelected = onClipSelected;
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

void ArrangeView::Canvas::setSeedClips(const juce::StringArray& clipNames)
{
    seedClips = clipNames;
    repaint();
}

void ArrangeView::Canvas::setPlacedClips(const juce::Array<ClipPlacement>& clipPlacements, int newSelectedClipIndex)
{
    placedClips = clipPlacements;
    this->selectedClipIndex = newSelectedClipIndex;
    rebuildLaneClips();
}

void ArrangeView::Canvas::setSelectedTrack(int trackIndex)
{
    for (int index = 0; index < lanes.size(); ++index)
        lanes[index]->setSelected(index == trackIndex);
}

void ArrangeView::Canvas::rebuildLaneClips()
{
    if (lanes.isEmpty())
        return;

    juce::Array<juce::Array<Lane::LaneClipView>> clipsByLane;
    clipsByLane.resize(lanes.size());

    for (int clipIndex = 0; clipIndex < placedClips.size(); ++clipIndex)
    {
        const auto& placement = placedClips.getReference(clipIndex);
        if (! juce::isPositiveAndBelow(placement.trackIndex, lanes.size()))
            continue;

        Lane::LaneClipView view;
        view.placement = placement;
        view.modelIndex = clipIndex;
        view.colour = clipColourForIndex(clipIndex);
        clipsByLane.getReference(placement.trackIndex).add(view);
    }

    for (int laneIndex = 0; laneIndex < lanes.size(); ++laneIndex)
        lanes[laneIndex]->setClips(clipsByLane[(size_t) laneIndex], selectedClipIndex);
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

    for (int beat = 0; beat < 24; ++beat)
    {
        auto x = ruler.getX() + beat * laneBeatWidth;
        g.drawLine((float) x, (float) ruler.getY(), (float) x, (float) getHeight(), 1.0f);
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
    setName("Foley");

    titleLabel.setText("Foley Stage", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    hintLabel.setText("Stage source sounds, trim the useful gesture, and drop it into a layer.", juce::dontSendNotification);
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(hintLabel);

    assetLabel.setText("Project Sounds", juce::dontSendNotification);
    assetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(assetLabel);

    trimLabel.setText("Slice Window", juce::dontSendNotification);
    trimLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(trimLabel);

    clipInspectorLabel.setText("Clip Inspector", juce::dontSendNotification);
    clipInspectorLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(clipInspectorLabel);

    clipNameLabel.setText("Name", juce::dontSendNotification);
    clipNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(clipNameLabel);

    clipNameEditor.onTextChange = [this]
    {
        if (suppressInspectorCallbacks)
            return;

        applyEditorValuesToSelectedClip();
    };
    addAndMakeVisible(clipNameEditor);

    clipTrackLabel.setText("Layer", juce::dontSendNotification);
    clipTrackLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(clipTrackLabel);

    clipTrackSelector.onChange = [this]
    {
        if (suppressInspectorCallbacks)
            return;

        applyEditorValuesToSelectedClip();
    };
    addAndMakeVisible(clipTrackSelector);

    clipStartBeatLabel.setText("Start Beat", juce::dontSendNotification);
    clipStartBeatLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(clipStartBeatLabel);

    clipStartBeatSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    clipStartBeatSlider.setRange(0.0, 64.0, 1.0);
    clipStartBeatSlider.onValueChange = [this]
    {
        if (suppressInspectorCallbacks)
            return;

        applyEditorValuesToSelectedClip();
    };
    addAndMakeVisible(clipStartBeatSlider);

    clipLengthLabel.setText("Length", juce::dontSendNotification);
    clipLengthLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(clipLengthLabel);

    clipLengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    clipLengthSlider.setRange(1.0, 16.0, 1.0);
    clipLengthSlider.onValueChange = [this]
    {
        if (suppressInspectorCallbacks)
            return;

        applyEditorValuesToSelectedClip();
    };
    addAndMakeVisible(clipLengthSlider);

    clipSelectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(clipSelectionLabel);

    actionLabel.setText("Slice Actions", juce::dontSendNotification);
    actionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(actionLabel);

    trimInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(trimInfoLabel);

    trimStartSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    trimStartSlider.setRange(0.0, 0.99, 0.001);
    trimStartSlider.onValueChange = [this]
    {
        trimStart = trimStartSlider.getValue();
        if (trimStart > trimEnd - 0.01)
            trimStart = trimEnd - 0.01;
        trimStartSlider.setValue(trimStart, juce::dontSendNotification);
        applyEditorValuesToSelectedClip();
        refreshTrimUi();
    };
    addAndMakeVisible(trimStartSlider);

    trimEndSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    trimEndSlider.setRange(0.01, 1.0, 0.001);
    trimEndSlider.onValueChange = [this]
    {
        trimEnd = trimEndSlider.getValue();
        if (trimEnd < trimStart + 0.01)
            trimEnd = trimStart + 0.01;
        trimEndSlider.setValue(trimEnd, juce::dontSendNotification);
        applyEditorValuesToSelectedClip();
        refreshTrimUi();
    };
    addAndMakeVisible(trimEndSlider);

    gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gainSlider.setRange(-18.0, 18.0, 0.1);
    gainSlider.onValueChange = [this]
    {
        gainDecibels = (float) gainSlider.getValue();
        applyEditorValuesToSelectedClip();
        refreshTrimUi();
    };
    addAndMakeVisible(gainSlider);

    fadeInSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    fadeInSlider.setRange(0.0, 0.5, 0.001);
    fadeInSlider.onValueChange = [this]
    {
        fadeInNormalized = (float) fadeInSlider.getValue();
        applyEditorValuesToSelectedClip();
        refreshTrimUi();
    };
    addAndMakeVisible(fadeInSlider);

    fadeOutSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    fadeOutSlider.setRange(0.0, 0.5, 0.001);
    fadeOutSlider.onValueChange = [this]
    {
        fadeOutNormalized = (float) fadeOutSlider.getValue();
        applyEditorValuesToSelectedClip();
        refreshTrimUi();
    };
    addAndMakeVisible(fadeOutSlider);

    reverseButton.onClick = [this]
    {
        applyEditorValuesToSelectedClip();
        refreshTrimUi();
    };
    addAndMakeVisible(reverseButton);

    normalizeButton.onClick = [this]
    {
        applyEditorValuesToSelectedClip();
        refreshTrimUi();
    };
    addAndMakeVisible(normalizeButton);

    importAssetButton.onClick = [this]
    {
        if (onImportAssetRequested)
            onImportAssetRequested();
    };
    addAndMakeVisible(importAssetButton);

    previewSliceButton.onClick = [this]
    {
        if (juce::isPositiveAndBelow(selectedAssetIndex, assetFiles.size()) && onAssetPreviewRequested)
            onAssetPreviewRequested(assetFiles[(size_t) selectedAssetIndex]);
    };
    addAndMakeVisible(previewSliceButton);

    placeAssetButton.onClick = [this]
    {
        if (! juce::isPositiveAndBelow(selectedAssetIndex, assetFiles.size()))
            return;

        ClipPlacement placement;
        placement.displayName = makePlacedClipLabel();
        placement.assetFileName = assetFiles[(size_t) selectedAssetIndex].getFileName();
        placement.trackIndex = selectedTrack;
        placement.trimStart = trimStart;
        placement.trimEnd = trimEnd;
        placement.gainDecibels = gainDecibels;
        placement.fadeInNormalized = fadeInNormalized;
        placement.fadeOutNormalized = fadeOutNormalized;
        placement.reverse = reverseButton.getToggleState();
        placement.normalize = normalizeButton.getToggleState();
        placement.lengthBeats = juce::jlimit(1, 8, juce::roundToInt((placement.trimEnd - placement.trimStart) * 12.0));

        auto nextBeat = 1;
        for (const auto& existing : placedClips)
            if (existing.trackIndex == selectedTrack)
                nextBeat = juce::jmax(nextBeat, existing.startBeat + existing.lengthBeats + 1);

        placement.startBeat = nextBeat;
        placedClips.add(placement);
        selectedClipIndex = placedClips.size() - 1;
        canvas.setPlacedClips(placedClips, selectedClipIndex);
        refreshClipActionButtons();
        refreshClipInspector();
        notifyArrangementChanged();
    };
    addAndMakeVisible(placeAssetButton);

    duplicateClipButton.onClick = [this]
    {
        if (! juce::isPositiveAndBelow(selectedClipIndex, placedClips.size()))
            return;

        auto duplicate = placedClips.getReference(selectedClipIndex);
        duplicate.startBeat += juce::jmax(1, duplicate.lengthBeats);
        duplicate.displayName += " copy";
        placedClips.insert(selectedClipIndex + 1, duplicate);
        ++selectedClipIndex;
        canvas.setPlacedClips(placedClips, selectedClipIndex);
        refreshClipActionButtons();
        refreshClipInspector();
        notifyArrangementChanged();
    };
    addAndMakeVisible(duplicateClipButton);

    deleteClipButton.onClick = [this]
    {
        if (! juce::isPositiveAndBelow(selectedClipIndex, placedClips.size()))
            return;

        placedClips.remove(selectedClipIndex);
        selectedClipIndex = placedClips.isEmpty() ? -1 : juce::jlimit(0, placedClips.size() - 1, selectedClipIndex);
        canvas.setPlacedClips(placedClips, selectedClipIndex);
        refreshClipActionButtons();
        refreshClipInspector();
        notifyArrangementChanged();
    };
    addAndMakeVisible(deleteClipButton);

    addTrackButton.setTooltip("Add Track");
    addTrackButton.onClick = [this]
    {
        if (onAddTrackRequested)
            onAddTrackRequested();
    };
    addAndMakeVisible(addTrackButton);

    removeTrackButton.setTooltip("Remove Selected Track");
    removeTrackButton.onClick = [this]
    {
        if (onRemoveTrackRequested)
            onRemoveTrackRequested(selectedTrack);
    };
    addAndMakeVisible(removeTrackButton);

    assetSelector.onChange = [this]
    {
        selectAssetIndex(assetSelector.getSelectedItemIndex());
    };
    addAndMakeVisible(assetSelector);

    addAndMakeVisible(waveformPanel);

    viewport.setViewedComponent(&canvas, false);
    viewport.setScrollBarsShown(true, true);
    addAndMakeVisible(viewport);

    canvas.onTrackSelected = [this](int trackIndex)
    {
        setSelectedTrack(trackIndex);
        if (onTrackSelected)
            onTrackSelected(trackIndex);
    };

    canvas.onClipMoved = [this](int modelIndex, int newStartBeat)
    {
        if (juce::isPositiveAndBelow(modelIndex, placedClips.size()))
        {
            placedClips.getReference(modelIndex).startBeat = newStartBeat;
            selectedClipIndex = modelIndex;
            canvas.setPlacedClips(placedClips, selectedClipIndex);
            refreshClipActionButtons();
            refreshClipInspector();
            notifyArrangementChanged();
        }
    };

    canvas.onClipSelected = [this](int modelIndex)
    {
        selectedClipIndex = modelIndex;
        canvas.setPlacedClips(placedClips, selectedClipIndex);
        refreshClipActionButtons();
        refreshClipInspector();
    };

    trimStartSlider.setValue(0.0, juce::dontSendNotification);
    trimEndSlider.setValue(1.0, juce::dontSendNotification);
    gainSlider.setValue(0.0, juce::dontSendNotification);
    fadeInSlider.setValue(0.0, juce::dontSendNotification);
    fadeOutSlider.setValue(0.0, juce::dontSendNotification);
    refreshTrimUi();
    updateCanvasTrackCount();
    refreshAssetSelector();
    refreshClipActionButtons();
    refreshClipInspector();
}

void ArrangeView::setTotalTrackCount(int newTrackCount)
{
    totalTrackCount = juce::jmax(0, newTrackCount);
    visibleTrackCount = juce::jmin(visibleTrackCount, totalTrackCount);
    if (totalTrackCount == 0)
    {
        selectedTrack = 0;
        placedClips.clear();
        selectedClipIndex = -1;
    }
    else
    {
        selectedTrack = juce::jlimit(0, totalTrackCount - 1, selectedTrack);
        for (int clipIndex = placedClips.size(); --clipIndex >= 0;)
        {
            if (placedClips.getReference(clipIndex).trackIndex >= totalTrackCount)
            {
                placedClips.remove(clipIndex);
                if (selectedClipIndex == clipIndex)
                    selectedClipIndex = -1;
                else if (selectedClipIndex > clipIndex)
                    --selectedClipIndex;
            }
        }
    }

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
    canvas.setPlacedClips(placedClips, selectedClipIndex);
    refreshClipActionButtons();
    refreshClipInspector();
}

void ArrangeView::setVisibleTrackCount(int newVisibleTrackCount)
{
    visibleTrackCount = totalTrackCount == 0 ? 0 : juce::jlimit(1, totalTrackCount, newVisibleTrackCount);
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
    canvas.setSeedClips(clipNames);
}

void ArrangeView::setAssetFiles(const juce::Array<juce::File>& files)
{
    assetFiles = files;
    refreshAssetSelector();
}

void ArrangeView::setSelectedTrack(int trackIndex)
{
    selectedTrack = totalTrackCount == 0 ? 0 : juce::jlimit(0, juce::jmax(0, visibleTrackCount - 1), trackIndex);
    canvas.setSelectedTrack(selectedTrack);
}

void ArrangeView::addAssetClipToSelectedTrack(const juce::String& clipName)
{
    auto targetIndex = -1;
    for (int index = 0; index < assetFiles.size(); ++index)
    {
        auto& file = assetFiles.getReference(index);
        if (file.getFileNameWithoutExtension() == clipName || file.getFileName() == clipName)
        {
            targetIndex = index;
            break;
        }
    }

    if (! juce::isPositiveAndBelow(targetIndex, assetFiles.size()))
        return;

    selectedAssetIndex = targetIndex;
    assetSelector.setSelectedItemIndex(targetIndex, juce::dontSendNotification);
    waveformPanel.setAudioFile(assetFiles[(size_t) targetIndex]);
    placeAssetButton.setEnabled(true);

    ClipPlacement placement;
    placement.displayName = makePlacedClipLabel();
    placement.assetFileName = assetFiles[(size_t) targetIndex].getFileName();
    placement.trackIndex = selectedTrack;
    placement.trimStart = trimStart;
    placement.trimEnd = trimEnd;
    placement.gainDecibels = gainDecibels;
    placement.fadeInNormalized = fadeInNormalized;
    placement.fadeOutNormalized = fadeOutNormalized;
    placement.reverse = reverseButton.getToggleState();
    placement.normalize = normalizeButton.getToggleState();
    placement.lengthBeats = juce::jlimit(1, 8, juce::roundToInt((placement.trimEnd - placement.trimStart) * 12.0));

    auto nextBeat = 1;
    for (const auto& existing : placedClips)
        if (existing.trackIndex == selectedTrack)
            nextBeat = juce::jmax(nextBeat, existing.startBeat + existing.lengthBeats + 1);

    placement.startBeat = nextBeat;
    placedClips.add(placement);
    selectedClipIndex = placedClips.size() - 1;
    canvas.setPlacedClips(placedClips, selectedClipIndex);
    refreshClipActionButtons();
    refreshClipInspector();
    notifyArrangementChanged();
}

juce::ValueTree ArrangeView::createState() const
{
    juce::ValueTree state("ArrangeView");
    state.setProperty("selectedTrack", selectedTrack, nullptr);
    state.setProperty("selectedAssetName", juce::isPositiveAndBelow(selectedAssetIndex, assetFiles.size())
                                             ? assetFiles[(size_t) selectedAssetIndex].getFileName()
                                             : juce::String(),
                      nullptr);
    state.setProperty("trimStart", trimStart, nullptr);
    state.setProperty("trimEnd", trimEnd, nullptr);
    state.setProperty("gainDecibels", gainDecibels, nullptr);
    state.setProperty("fadeInNormalized", fadeInNormalized, nullptr);
    state.setProperty("fadeOutNormalized", fadeOutNormalized, nullptr);
    state.setProperty("reverse", reverseButton.getToggleState(), nullptr);
    state.setProperty("normalize", normalizeButton.getToggleState(), nullptr);

    for (int clipIndex = 0; clipIndex < placedClips.size(); ++clipIndex)
    {
        const auto& clip = placedClips.getReference(clipIndex);
        juce::ValueTree clipState("Clip");
        clipState.setProperty("displayName", clip.displayName, nullptr);
        clipState.setProperty("assetFileName", clip.assetFileName, nullptr);
        clipState.setProperty("trackIndex", clip.trackIndex, nullptr);
        clipState.setProperty("startBeat", clip.startBeat, nullptr);
        clipState.setProperty("lengthBeats", clip.lengthBeats, nullptr);
        clipState.setProperty("trimStart", clip.trimStart, nullptr);
        clipState.setProperty("trimEnd", clip.trimEnd, nullptr);
        clipState.setProperty("gainDecibels", clip.gainDecibels, nullptr);
        clipState.setProperty("fadeInNormalized", clip.fadeInNormalized, nullptr);
        clipState.setProperty("fadeOutNormalized", clip.fadeOutNormalized, nullptr);
        clipState.setProperty("reverse", clip.reverse, nullptr);
        clipState.setProperty("normalize", clip.normalize, nullptr);
        state.addChild(clipState, -1, nullptr);
    }

    return state;
}

void ArrangeView::restoreState(const juce::ValueTree& state)
{
    placedClips.clear();

    trimStart = (double) state.getProperty("trimStart", 0.0);
    trimEnd = (double) state.getProperty("trimEnd", 1.0);
    gainDecibels = (float) state.getProperty("gainDecibels", 0.0f);
    fadeInNormalized = (float) state.getProperty("fadeInNormalized", 0.0f);
    fadeOutNormalized = (float) state.getProperty("fadeOutNormalized", 0.0f);
    trimStartSlider.setValue(trimStart, juce::dontSendNotification);
    trimEndSlider.setValue(trimEnd, juce::dontSendNotification);
    gainSlider.setValue(gainDecibels, juce::dontSendNotification);
    fadeInSlider.setValue(fadeInNormalized, juce::dontSendNotification);
    fadeOutSlider.setValue(fadeOutNormalized, juce::dontSendNotification);
    reverseButton.setToggleState((bool) state.getProperty("reverse", false), juce::dontSendNotification);
    normalizeButton.setToggleState((bool) state.getProperty("normalize", false), juce::dontSendNotification);
    refreshTrimUi();

    for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex)
    {
        auto clipState = state.getChild(childIndex);
        if (! clipState.hasType("Clip"))
            continue;

        ClipPlacement clip;
        clip.displayName = clipState.getProperty("displayName").toString();
        clip.assetFileName = clipState.getProperty("assetFileName").toString();
        clip.trackIndex = (int) clipState.getProperty("trackIndex", 0);
        clip.startBeat = (int) clipState.getProperty("startBeat", 0);
        clip.lengthBeats = (int) clipState.getProperty("lengthBeats", 4);
        clip.trimStart = (double) clipState.getProperty("trimStart", 0.0);
        clip.trimEnd = (double) clipState.getProperty("trimEnd", 1.0);
        clip.gainDecibels = (float) clipState.getProperty("gainDecibels", 0.0f);
        clip.fadeInNormalized = (float) clipState.getProperty("fadeInNormalized", 0.0f);
        clip.fadeOutNormalized = (float) clipState.getProperty("fadeOutNormalized", 0.0f);
        clip.reverse = (bool) clipState.getProperty("reverse", false);
        clip.normalize = (bool) clipState.getProperty("normalize", false);
        placedClips.add(clip);
    }

    selectedClipIndex = placedClips.isEmpty() ? -1 : 0;
    canvas.setPlacedClips(placedClips, selectedClipIndex);
    refreshClipActionButtons();
    refreshClipInspector();
    setSelectedTrack((int) state.getProperty("selectedTrack", 0));

    auto selectedAssetName = state.getProperty("selectedAssetName").toString();
    if (selectedAssetName.isNotEmpty())
    {
        for (int index = 0; index < assetFiles.size(); ++index)
        {
            if (assetFiles[(size_t) index].getFileName() == selectedAssetName)
            {
                assetSelector.setSelectedItemIndex(index, juce::dontSendNotification);
                selectAssetIndex(index);
                break;
            }
        }
    }
}

void ArrangeView::updateCanvasTrackCount()
{
    canvas.setTrackCount(visibleTrackCount, trackNames);
    canvas.setPlacedClips(placedClips, selectedClipIndex);
    clipTrackSelector.clear(juce::dontSendNotification);
    for (int index = 0; index < visibleTrackCount; ++index)
    {
        auto label = juce::isPositiveAndBelow(index, trackNames.size()) && trackNames[index].isNotEmpty()
                   ? trackNames[index]
                   : "Layer " + juce::String(index + 1);
        clipTrackSelector.addItem(label, index + 1);
    }
    refreshClipInspector();
    viewport.setViewPosition(0, 0);
    setSelectedTrack(selectedTrack);
}

void ArrangeView::refreshAssetSelector()
{
    assetSelector.clear(juce::dontSendNotification);

    for (int index = 0; index < assetFiles.size(); ++index)
        assetSelector.addItem(assetFiles[(size_t) index].getFileName(), index + 1);

    if (assetFiles.isEmpty())
    {
        selectedAssetIndex = -1;
        assetSelector.setTextWhenNoChoicesAvailable("Import a sound to begin");
        assetSelector.setText({}, juce::dontSendNotification);
        waveformPanel.setAudioFile({});
        previewSliceButton.setEnabled(false);
        placeAssetButton.setEnabled(false);
    }
    else
    {
        assetSelector.setSelectedItemIndex(0, juce::dontSendNotification);
        selectAssetIndex(0);
    }
}

void ArrangeView::refreshTrimUi()
{
    auto text = "Slice: " + formatPercent(trimStart) + " -> " + formatPercent(trimEnd)
              + "  |  Gain " + formatDecibels(gainDecibels)
              + "  |  Fade In " + formatPercent(fadeInNormalized)
              + "  |  Fade Out " + formatPercent(fadeOutNormalized);

    if (reverseButton.getToggleState())
        text += "  |  Reverse";

    if (normalizeButton.getToggleState())
        text += "  |  Normalize";

    trimInfoLabel.setText(text, juce::dontSendNotification);
}

void ArrangeView::selectAssetIndex(int index)
{
    if (! juce::isPositiveAndBelow(index, assetFiles.size()))
    {
        selectedAssetIndex = -1;
        waveformPanel.setAudioFile({});
        previewSliceButton.setEnabled(false);
        placeAssetButton.setEnabled(false);
        return;
    }

    selectedAssetIndex = index;
    waveformPanel.setAudioFile(assetFiles[(size_t) index]);
    previewSliceButton.setEnabled(true);
    placeAssetButton.setEnabled(true);
}

juce::String ArrangeView::makePlacedClipLabel() const
{
    if (! juce::isPositiveAndBelow(selectedAssetIndex, assetFiles.size()))
        return {};

    return assetFiles[(size_t) selectedAssetIndex].getFileNameWithoutExtension()
         + " ["
         + formatPercent(trimStart)
         + "-"
         + formatPercent(trimEnd)
         + ", " + formatDecibels(gainDecibels)
         + (reverseButton.getToggleState() ? ", rev" : "")
         + (normalizeButton.getToggleState() ? ", norm" : "")
         + "]";
}

void ArrangeView::notifyArrangementChanged()
{
    if (onArrangementChanged)
        onArrangementChanged();
}

void ArrangeView::refreshClipActionButtons()
{
    auto hasSelection = juce::isPositiveAndBelow(selectedClipIndex, placedClips.size());
    duplicateClipButton.setEnabled(hasSelection);
    deleteClipButton.setEnabled(hasSelection);
}

void ArrangeView::refreshClipInspector()
{
    suppressInspectorCallbacks = true;

    auto hasSelection = juce::isPositiveAndBelow(selectedClipIndex, placedClips.size());
    clipNameEditor.setEnabled(hasSelection);
    clipTrackSelector.setEnabled(hasSelection);
    clipStartBeatSlider.setEnabled(hasSelection);
    clipLengthSlider.setEnabled(hasSelection);

    if (hasSelection)
    {
        const auto& clip = placedClips.getReference(selectedClipIndex);
        clipSelectionLabel.setText("Editing placed clip: " + clip.displayName, juce::dontSendNotification);
        clipNameEditor.setText(clip.displayName, juce::dontSendNotification);
        clipTrackSelector.setSelectedItemIndex(clip.trackIndex, juce::dontSendNotification);
        clipStartBeatSlider.setValue((double) clip.startBeat, juce::dontSendNotification);
        clipLengthSlider.setValue((double) clip.lengthBeats, juce::dontSendNotification);

        trimStart = clip.trimStart;
        trimEnd = clip.trimEnd;
        gainDecibels = clip.gainDecibels;
        fadeInNormalized = clip.fadeInNormalized;
        fadeOutNormalized = clip.fadeOutNormalized;
        trimStartSlider.setValue(trimStart, juce::dontSendNotification);
        trimEndSlider.setValue(trimEnd, juce::dontSendNotification);
        gainSlider.setValue(gainDecibels, juce::dontSendNotification);
        fadeInSlider.setValue(fadeInNormalized, juce::dontSendNotification);
        fadeOutSlider.setValue(fadeOutNormalized, juce::dontSendNotification);
        reverseButton.setToggleState(clip.reverse, juce::dontSendNotification);
        normalizeButton.setToggleState(clip.normalize, juce::dontSendNotification);
    }
    else
    {
        clipSelectionLabel.setText("No placed clip selected. Slice controls below affect the next placement.", juce::dontSendNotification);
        clipNameEditor.setText({}, juce::dontSendNotification);
        clipTrackSelector.setSelectedItemIndex(-1, juce::dontSendNotification);
        clipStartBeatSlider.setValue(0.0, juce::dontSendNotification);
        clipLengthSlider.setValue(1.0, juce::dontSendNotification);
    }

    suppressInspectorCallbacks = false;
    refreshTrimUi();
}

void ArrangeView::applyEditorValuesToSelectedClip()
{
    if (suppressInspectorCallbacks)
        return;

    if (! juce::isPositiveAndBelow(selectedClipIndex, placedClips.size()))
        return;

    auto& clip = placedClips.getReference(selectedClipIndex);
    auto trimmedName = clipNameEditor.getText().trim();
    if (trimmedName.isNotEmpty())
        clip.displayName = trimmedName;

    clip.trackIndex = juce::jlimit(0, juce::jmax(0, visibleTrackCount - 1), clipTrackSelector.getSelectedItemIndex());
    clip.startBeat = juce::jmax(0, juce::roundToInt(clipStartBeatSlider.getValue()));
    clip.lengthBeats = juce::jlimit(1, 16, juce::roundToInt(clipLengthSlider.getValue()));
    clip.trimStart = trimStart;
    clip.trimEnd = trimEnd;
    clip.gainDecibels = gainDecibels;
    clip.fadeInNormalized = fadeInNormalized;
    clip.fadeOutNormalized = fadeOutNormalized;
    clip.reverse = reverseButton.getToggleState();
    clip.normalize = normalizeButton.getToggleState();

    canvas.setPlacedClips(placedClips, selectedClipIndex);
    notifyArrangementChanged();
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

    auto toolbar = area.removeFromTop(34);
    assetLabel.setBounds(toolbar.removeFromLeft(110));
    assetSelector.setBounds(toolbar.removeFromLeft(280));
    toolbar.removeFromLeft(10);
    importAssetButton.setBounds(toolbar.removeFromLeft(130));
    toolbar.removeFromLeft(10);
    previewSliceButton.setBounds(toolbar.removeFromLeft(120));
    toolbar.removeFromLeft(10);
    placeAssetButton.setBounds(toolbar.removeFromLeft(110));
    toolbar.removeFromLeft(10);
    duplicateClipButton.setBounds(toolbar.removeFromLeft(120));
    toolbar.removeFromLeft(10);
    deleteClipButton.setBounds(toolbar.removeFromLeft(100));
    removeTrackButton.setBounds(toolbar.removeFromRight(72));
    toolbar.removeFromRight(8);
    addTrackButton.setBounds(toolbar.removeFromRight(72));

    area.removeFromTop(10);

    auto editorArea = area.removeFromTop(320);
    auto inspectorArea = editorArea.removeFromLeft(area.getWidth() / 2).reduced(0, 0);
    inspectorArea.removeFromRight(8);
    auto sliceArea = editorArea.reduced(0, 0);
    sliceArea.removeFromLeft(8);

    clipInspectorLabel.setBounds(inspectorArea.removeFromTop(22));
    clipSelectionLabel.setBounds(inspectorArea.removeFromTop(22));
    inspectorArea.removeFromTop(6);
    clipNameLabel.setBounds(inspectorArea.removeFromTop(18));
    clipNameEditor.setBounds(inspectorArea.removeFromTop(26));
    inspectorArea.removeFromTop(6);
    clipTrackLabel.setBounds(inspectorArea.removeFromTop(18));
    clipTrackSelector.setBounds(inspectorArea.removeFromTop(26));
    inspectorArea.removeFromTop(6);
    clipStartBeatLabel.setBounds(inspectorArea.removeFromTop(18));
    clipStartBeatSlider.setBounds(inspectorArea.removeFromTop(24));
    inspectorArea.removeFromTop(6);
    clipLengthLabel.setBounds(inspectorArea.removeFromTop(18));
    clipLengthSlider.setBounds(inspectorArea.removeFromTop(24));

    trimLabel.setBounds(sliceArea.removeFromTop(22));
    waveformPanel.setBounds(sliceArea.removeFromTop(98));
    sliceArea.removeFromTop(6);
    trimStartSlider.setBounds(sliceArea.removeFromTop(24));
    sliceArea.removeFromTop(4);
    trimEndSlider.setBounds(sliceArea.removeFromTop(24));
    sliceArea.removeFromTop(8);
    actionLabel.setBounds(sliceArea.removeFromTop(22));
    gainSlider.setBounds(sliceArea.removeFromTop(24));
    sliceArea.removeFromTop(4);
    fadeInSlider.setBounds(sliceArea.removeFromTop(24));
    sliceArea.removeFromTop(4);
    fadeOutSlider.setBounds(sliceArea.removeFromTop(24));
    sliceArea.removeFromTop(6);
    auto toggles = sliceArea.removeFromTop(24);
    reverseButton.setBounds(toggles.removeFromLeft(120));
    normalizeButton.setBounds(toggles.removeFromLeft(120));
    sliceArea.removeFromTop(4);
    trimInfoLabel.setBounds(sliceArea.removeFromTop(20));

    area.removeFromTop(10);
    viewport.setBounds(area);
    canvas.setSize(2400, 44 + visibleTrackCount * 88);
}
