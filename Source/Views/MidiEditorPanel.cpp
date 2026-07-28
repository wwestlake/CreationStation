#include "MidiEditorPanel.h"

namespace
{
juce::Colour panelColour() { return juce::Colour(0xff11151c); }
juce::Colour gridBackground() { return juce::Colour(0xff0d1219); }
juce::Colour blackKeyRowColour() { return juce::Colour(0xff11161f); }
juce::Colour whiteKeyRowColour() { return juce::Colour(0xff151b26); }
juce::Colour beatLineColour() { return juce::Colour(0xff2a3445); }
juce::Colour subdivisionLineColour() { return juce::Colour(0xff1c2432); }
juce::Colour noteColour() { return juce::Colour(0xff56f4ff); }
juce::Colour noteSelectedColour() { return juce::Colour(0xffffb347); }
juce::Colour dimText() { return juce::Colour(0xff8ea0b7); }

class MidiTransportButtonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton,
                              bool isButtonDown) override
    {
        juce::ignoreUnused(backgroundColour);

        auto bounds = button.getLocalBounds().toFloat().reduced(1.5f);
        auto isToggle = button.getToggleState();
        auto accent = juce::Colour(0xff59dfff);
        auto fill = juce::Colour(0xff17222c);

        if (isToggle)
            fill = accent.withAlpha(0.25f).overlaidWith(juce::Colour(0xff13202b));
        else if (isButtonDown)
            fill = accent.withAlpha(0.20f).overlaidWith(fill);
        else if (isMouseOverButton)
            fill = accent.withAlpha(0.12f).overlaidWith(fill);

        g.setColour(accent.withAlpha(isToggle ? 0.35f : isMouseOverButton ? 0.35f : 0.14f));
        g.fillRoundedRectangle(bounds.expanded(2.0f), 13.0f);
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 11.0f);

        g.setColour(accent.withAlpha(isToggle ? 1.0f : 0.62f));
        g.drawRoundedRectangle(bounds, 11.0f, isToggle ? 2.0f : 1.3f);

        auto ring = bounds.reduced(7.0f, 5.0f);
        if (ring.getWidth() > 18.0f && ring.getHeight() > 18.0f)
        {
            auto diameter = juce::jmin(ring.getWidth(), ring.getHeight());
            auto circle = juce::Rectangle<float>(diameter, diameter).withCentre(ring.getCentre());
            g.setColour(accent.withAlpha(isToggle ? 0.96f : 0.36f));
            g.drawEllipse(circle, isToggle ? 2.4f : 2.0f);
        }
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool isMouseOverButton,
                        bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(8.0f, 7.0f);

        g.setColour(button.getToggleState() ? juce::Colours::white
                                            : (isButtonDown ? juce::Colour(0xffeaf6ff)
                                                            : isMouseOverButton ? juce::Colour(0xffdcecff)
                                                                                : juce::Colour(0xffb8c4d5)));
        drawTransportIcon(g, bounds, button.getButtonText());
    }

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool isMouseOverButton,
                          bool isButtonDown) override
    {
        drawButtonBackground(g,
                             button,
                             button.findColour(juce::TextButton::buttonColourId),
                             isMouseOverButton,
                             isButtonDown);

        auto bounds = button.getLocalBounds().toFloat().reduced(8.0f, 7.0f);
        g.setColour(button.getToggleState() ? juce::Colour(0xff5ce8ff) : juce::Colour(0xffb8c4d5));
        drawTransportIcon(g, bounds, button.getButtonText());
    }

private:
    static void drawTransportIcon(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& iconName)
    {
        auto centre = bounds.getCentre();
        auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());

        if (iconName == "play")
        {
            juce::Path path;
            path.addTriangle(centre.x - size * 0.22f, centre.y - size * 0.32f,
                             centre.x - size * 0.22f, centre.y + size * 0.32f,
                             centre.x + size * 0.32f, centre.y);
            g.fillPath(path);
            return;
        }

        if (iconName == "stop")
        {
            auto square = juce::Rectangle<float>(size * 0.55f, size * 0.55f).withCentre(centre);
            g.fillRoundedRectangle(square, 2.0f);
            return;
        }

        if (iconName == "loop")
        {
            auto arc = bounds.reduced(size * 0.12f);
            g.drawEllipse(arc, 2.0f);
            juce::Path arrow;
            arrow.addTriangle(arc.getRight() - size * 0.02f, arc.getCentreY() - size * 0.20f,
                              arc.getRight() + size * 0.16f, arc.getCentreY() - size * 0.06f,
                              arc.getRight() - size * 0.02f, arc.getCentreY() + size * 0.08f);
            g.fillPath(arrow);
            return;
        }
    }
};

MidiTransportButtonLookAndFeel& getMidiTransportButtonLookAndFeel()
{
    static MidiTransportButtonLookAndFeel lookAndFeel;
    return lookAndFeel;
}

constexpr float keyGutterWidth = 52.0f;
constexpr float rulerHeight = 44.0f;
constexpr float velocityLaneHeight = 84.0f;
constexpr double freePlacementMinimumBeats = 1.0 / 128.0;
constexpr float noteResizeHandleWidth = 6.0f;

bool isBlackKey(int pitch)
{
    switch (pitch % 12)
    {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

juce::String pitchName(int pitch)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    auto octave = (pitch / 12) - 1;
    return juce::String(names[pitch % 12]) + juce::String(octave);
}

double gridBeatsForComboId(int itemId)
{
    switch (itemId)
    {
        case 1: return 1.0;          // 1/4
        case 2: return 0.5;          // 1/8
        case 3: return 0.25;         // 1/16
        case 4: return 1.0 / 3.0;    // 1/8 triplet
        case 5: return 4.0;          // 1/1 (bar in 4/4)
        default: return 0.25;
    }
}

// -1 means "show the velocity lane" rather than a CC lane
int controllerForLaneComboId(int itemId)
{
    switch (itemId)
    {
        case 1: return -1;   // Velocity
        case 2: return 1;    // Mod Wheel
        case 3: return 10;   // Pan
        case 4: return 11;   // Expression
        case 5: return 64;   // Sustain
        case 6: return 91;   // Reverb Send
        default: return -1;
    }
}

juce::String controllerLaneName(int controller)
{
    switch (controller)
    {
        case 1: return "CC1 - Mod Wheel";
        case 10: return "CC10 - Pan";
        case 11: return "CC11 - Expression";
        case 64: return "CC64 - Sustain";
        case 91: return "CC91 - Reverb Send";
        default: return "CC" + juce::String(controller);
    }
}

juce::String formatBeatPosition(double beat, int beatsPerBar)
{
    auto safeBeat = juce::jmax(0.0, beat);
    auto wholeBeats = (int) std::floor(safeBeat);
    auto measure = wholeBeats / juce::jmax(1, beatsPerBar);
    auto beatInMeasure = wholeBeats % juce::jmax(1, beatsPerBar);
    auto fraction = safeBeat - (double) wholeBeats;
    auto ticks = (int) std::round(fraction * 1000.0);
    return juce::String(measure + 1) + "." + juce::String(beatInMeasure + 1) + "." + juce::String(ticks).paddedLeft('0', 3);
}

juce::String formatSecondsCompact(double seconds)
{
    auto totalMs = juce::jmax(0, (int) std::round(seconds * 1000.0));
    auto minutes = totalMs / 60000;
    auto secs = (totalMs / 1000) % 60;
    auto millis = totalMs % 1000;
    return juce::String(minutes) + ":" + juce::String(secs).paddedLeft('0', 2) + "." + juce::String(millis).paddedLeft('0', 3);
}
}

void MidiEditorPanel::PianoLane::paint(juce::Graphics& g)
{
    if (panel == nullptr)
        return;

    g.fillAll(panelColour());

    auto scrollY = (float) panel->gridViewport.getViewPositionY();
    auto rowHeight = panel->pixelsPerPitch();

    for (int pitch = 0; pitch <= 127; ++pitch)
    {
        auto y = panel->pitchToY(pitch) - scrollY;
        if (y + rowHeight < 0.0f || y > (float) getHeight())
            continue;

        auto rowBounds = juce::Rectangle<float>(0.0f, y, (float) getWidth(), rowHeight);
        g.setColour(isBlackKey(pitch) ? juce::Colour(0xff161c25) : juce::Colour(0xffe5e9ef));
        g.fillRect(rowBounds);

        g.setColour(isBlackKey(pitch) ? juce::Colour(0xff2a3445) : juce::Colour(0xffb8c2d1));
        g.drawRect(rowBounds.toNearestInt());

        auto pc = pitch % 12;
        auto shouldLabel = pc == 0 || ! isBlackKey(pitch);
        if (shouldLabel)
        {
            g.setColour(isBlackKey(pitch) ? juce::Colours::white : juce::Colour(0xff1b2430));
            g.setFont(juce::Font(pc == 0 ? 10.5f : 9.5f).boldened());
            g.drawText(pitchName(pitch),
                       rowBounds.reduced(6.0f, 0.0f),
                       juce::Justification::centredLeft,
                       false);
        }
    }

    g.setColour(juce::Colour(0xff27364a));
    g.drawLine((float) getWidth() - 1.0f, 0.0f, (float) getWidth() - 1.0f, (float) getHeight(), 1.0f);
}

void MidiEditorPanel::PianoLane::mouseDown(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handlePianoLaneMouseDown(event);
}

void MidiEditorPanel::PianoLane::mouseDrag(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handlePianoLaneMouseDrag(event);
}

void MidiEditorPanel::PianoLane::mouseUp(const juce::MouseEvent&)
{
    if (panel != nullptr)
        panel->handlePianoLaneMouseUp();
}

void MidiEditorPanel::GridView::paint(juce::Graphics& g)
{
    if (panel == nullptr)
        return;

    g.fillAll(gridBackground());

    auto totalBeats = panel->totalBeatsVisible();
    auto width = getWidth();
    auto height = getHeight();
    const auto transportBeat = panel->timelineModel != nullptr ? panel->timelineModel->secondsToBeat(panel->displayedTransportSeconds)
                                                               : 0.0;
    const auto loopStartBeat = panel->timelineModel != nullptr ? panel->timelineModel->secondsToBeat(panel->localLoopStartSeconds)
                                                               : 0.0;
    const auto loopEndBeat = panel->timelineModel != nullptr ? panel->timelineModel->secondsToBeat(panel->localLoopEndSeconds)
                                                             : 0.0;
    const bool showLoop = panel->localLoopEnabled && panel->localLoopEndSeconds > panel->localLoopStartSeconds;

    // Pitch rows
    for (int pitch = 0; pitch <= 127; ++pitch)
    {
        auto y = panel->pitchToY(pitch);
        auto rowBounds = juce::Rectangle<float>(0.0f, y, (float) width, panel->pixelsPerPitch());
        g.setColour(isBlackKey(pitch) ? blackKeyRowColour() : whiteKeyRowColour());
        g.fillRect(rowBounds);
    }

    if (showLoop)
    {
        auto loopX = panel->beatToX(loopStartBeat);
        auto loopW = juce::jmax(2.0f, panel->beatToX(loopEndBeat) - loopX);
        g.setColour(juce::Colour(0x2067e8a5));
        g.fillRect(juce::Rectangle<float>(loopX, 0.0f, loopW, (float) height));
        g.setColour(juce::Colour(0xff67e8a5).withAlpha(0.85f));
        g.drawVerticalLine((int) std::round(loopX), 0.0f, (float) height);
        g.drawVerticalLine((int) std::round(loopX + loopW), 0.0f, (float) height);
    }

    // Beat grid lines
    auto beatsPerBar = 4.0;
    for (double beat = 0.0; beat <= totalBeats; beat += 0.25)
    {
        auto x = panel->beatToX(beat);
        bool isBar = std::fmod(beat, beatsPerBar) < 0.001;
        bool isBeat = std::fmod(beat, 1.0) < 0.001;
        g.setColour(isBar ? beatLineColour() : (isBeat ? beatLineColour().withAlpha(0.6f) : subdivisionLineColour()));
        g.drawVerticalLine((int) x, 0.0f, (float) height);
    }

    // Notes
    if (panel->timelineModel != nullptr)
    {
        if (auto* notes = panel->timelineModel->getMidiNotes(panel->editingClipIndex))
        {
            for (size_t i = 0; i < notes->size() && i < (size_t) panel->noteBounds.size(); ++i)
            {
                const auto& note = (*notes)[i];
                auto bounds = panel->noteBounds[(int) i];
                bool selected = panel->selectedNoteIds.contains(note.id);

                g.setColour(selected ? noteSelectedColour() : noteColour().withAlpha(juce::jmap((float) note.velocity, 1.0f, 127.0f, 0.35f, 1.0f)));
                g.fillRoundedRectangle(bounds, 3.0f);
                g.setColour(juce::Colours::black.withAlpha(0.4f));
                g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
            }
        }
    }

    if (! panel->marqueeRect.isEmpty())
    {
        g.setColour(noteSelectedColour().withAlpha(0.15f));
        g.fillRect(panel->marqueeRect);
        g.setColour(noteSelectedColour().withAlpha(0.6f));
        g.drawRect(panel->marqueeRect, 1.0f);
    }

    auto playheadX = panel->beatToX(transportBeat);
    g.setColour(panel->isPlaying ? juce::Colour(0xffff6b6b) : noteSelectedColour());
    g.drawVerticalLine((int) std::round(playheadX), 0.0f, (float) height);
}

void MidiEditorPanel::GridView::mouseDown(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handleGridMouseDown(event);
}

void MidiEditorPanel::GridView::mouseDrag(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handleGridMouseDrag(event);
}

void MidiEditorPanel::GridView::mouseUp(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handleGridMouseUp(event);
}

void MidiEditorPanel::GridView::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handleGridDoubleClick(event);
}

void MidiEditorPanel::GridView::mouseMove(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handleGridMouseMove(event);
}

void MidiEditorPanel::GridView::mouseExit(const juce::MouseEvent&)
{
    if (panel != nullptr)
        panel->handleGridMouseExit();
}

void MidiEditorPanel::GridView::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (panel != nullptr)
        panel->handleMouseWheel(event, wheel, true);
}

void MidiEditorPanel::TimeRuler::paint(juce::Graphics& g)
{
    if (panel == nullptr)
        return;

    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff1a2330));

    auto scrollX = (float) panel->gridViewport.getViewPositionX();
    auto totalBeats = panel->totalBeatsVisible();
    auto beatsPerBar = panel->timelineModel != nullptr ? panel->timelineModel->getTimeSignatureNumerator() : 4;
    const auto transportBeat = panel->timelineModel != nullptr ? panel->timelineModel->secondsToBeat(panel->displayedTransportSeconds)
                                                               : 0.0;
    auto loopStartBeat = panel->timelineModel != nullptr ? panel->timelineModel->secondsToBeat(panel->localLoopStartSeconds)
                                                         : 0.0;
    auto loopEndBeat = panel->timelineModel != nullptr ? panel->timelineModel->secondsToBeat(panel->localLoopEndSeconds)
                                                       : 0.0;
    bool showLoop = panel->localLoopEnabled && panel->localLoopEndSeconds > panel->localLoopStartSeconds;
    if (panel->rulerDraggingLoop && panel->rulerLoopSelectionChanged)
    {
        loopStartBeat = juce::jmin(panel->rulerDragStartBeat, panel->rulerDragCurrentBeat);
        loopEndBeat = juce::jmax(panel->rulerDragStartBeat, panel->rulerDragCurrentBeat);
        showLoop = loopEndBeat > loopStartBeat;
    }

    if (showLoop)
    {
        auto loopX = panel->beatToX(loopStartBeat) - scrollX;
        auto loopW = juce::jmax(2.0f, panel->beatToX(loopEndBeat) - panel->beatToX(loopStartBeat));
        g.setColour(juce::Colour(0x3067e8a5));
        g.fillRect(juce::Rectangle<float>(loopX, 0.0f, loopW, bounds.getHeight()));
        g.setColour(juce::Colour(0xff67e8a5));
        g.drawRoundedRectangle(juce::Rectangle<float>(loopX, 1.0f, loopW, bounds.getHeight() - 2.0f), 4.0f, 1.0f);
    }

    auto hoverBeat = panel->hoveredBeat >= 0.0 ? std::floor(panel->hoveredBeat) : -1.0;
    if (hoverBeat >= 0.0)
    {
        auto x = panel->beatToX(hoverBeat) - scrollX;
        auto w = (float) panel->pixelsPerBeat();
        g.setColour(juce::Colour(0x22ffd166));
        g.fillRect(juce::Rectangle<float>(x, 0.0f, w, bounds.getHeight()));
    }

    for (double beat = 0.0; beat <= totalBeats; beat += 0.25)
    {
        auto x = panel->beatToX(beat) - scrollX;
        if (x < -1.0f || x > bounds.getWidth() + 1.0f)
            continue;

        bool isBar = std::fmod(beat, (double) beatsPerBar) < 0.001;
        bool isBeat = std::fmod(beat, 1.0) < 0.001;
        g.setColour(isBar ? juce::Colour(0xff5fa8ff)
                          : (isBeat ? beatLineColour().brighter(0.3f) : subdivisionLineColour()));
        g.drawVerticalLine((int) std::round(x), 0.0f, bounds.getBottom());

        if (isBar)
        {
            auto label = "M" + juce::String((int) std::floor(beat / beatsPerBar) + 1);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(12.0f).boldened());
            g.drawText(label, juce::Rectangle<float>(x + 6.0f, 4.0f, 48.0f, 14.0f), juce::Justification::centredLeft, false);
        }

        if (isBeat)
        {
            auto label = formatBeatPosition(beat, beatsPerBar);
            auto timeLabel = panel->timelineModel != nullptr ? formatSecondsCompact(panel->timelineModel->beatToSeconds(beat))
                                                             : formatSecondsCompact(beat * 0.5);
            g.setColour(dimText().brighter(isBar ? 0.35f : 0.0f));
            g.setFont(juce::Font(10.0f));
            g.drawText(label, juce::Rectangle<float>(x + 6.0f, 18.0f, 72.0f, 11.0f), juce::Justification::centredLeft, false);
            g.drawText(timeLabel, juce::Rectangle<float>(x + 6.0f, 29.0f, 72.0f, 11.0f), juce::Justification::centredLeft, false);
        }
    }

    if (panel->hoveredBeat >= 0.0)
    {
        auto hoverX = panel->beatToX(panel->hoveredBeat) - scrollX;
        g.setColour(noteSelectedColour().withAlpha(0.9f));
        g.drawVerticalLine((int) std::round(hoverX), 0.0f, bounds.getBottom());

        auto hoverBeatLabel = formatBeatPosition(panel->hoveredBeat, beatsPerBar);
        auto hoverTimeLabel = panel->timelineModel != nullptr ? formatSecondsCompact(panel->timelineModel->beatToSeconds(panel->hoveredBeat))
                                                              : formatSecondsCompact(0.0);
        auto bubble = juce::Rectangle<float>(juce::jlimit(0.0f, bounds.getWidth() - 118.0f, hoverX + 8.0f), 3.0f, 114.0f, 16.0f);
        g.setColour(juce::Colour(0xff263346).withAlpha(0.95f));
        g.fillRoundedRectangle(bubble, 4.0f);
        g.setColour(noteSelectedColour());
        g.drawRoundedRectangle(bubble, 4.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(10.5f).boldened());
        g.drawText(hoverBeatLabel + "  |  " + hoverTimeLabel, bubble.reduced(6.0f, 0.0f), juce::Justification::centredLeft, false);
    }

    auto playheadX = panel->beatToX(transportBeat) - scrollX;
    g.setColour(panel->isPlaying ? juce::Colour(0xffff6b6b) : noteSelectedColour());
    g.drawVerticalLine((int) std::round(playheadX), 0.0f, bounds.getBottom());
    g.fillEllipse(playheadX - 4.0f, 2.0f, 8.0f, 8.0f);

    g.setColour(juce::Colour(0xff27364a));
    g.drawLine(0.0f, bounds.getBottom() - 1.0f, bounds.getRight(), bounds.getBottom() - 1.0f, 1.0f);
}

void MidiEditorPanel::TimeRuler::mouseDown(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handleTimeRulerMouseDown(event);
}

void MidiEditorPanel::TimeRuler::mouseDrag(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handleTimeRulerMouseDrag(event);
}

void MidiEditorPanel::TimeRuler::mouseUp(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handleTimeRulerMouseUp(event);
}

void MidiEditorPanel::TimeRuler::mouseMove(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handleTimeRulerMouseMove(event);
}

void MidiEditorPanel::TimeRuler::mouseExit(const juce::MouseEvent&)
{
    if (panel != nullptr)
        panel->handleGridMouseExit();
}

void MidiEditorPanel::TimeRuler::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (panel != nullptr)
        panel->handleMouseWheel(event, wheel, false);
}

void MidiEditorPanel::VelocityLane::paint(juce::Graphics& g)
{
    if (panel == nullptr)
        return;

    g.fillAll(juce::Colour(0xff0a0e14));
    g.setColour(dimText());
    g.setFont(juce::Font(10.0f));
    g.drawText("Velocity", juce::Rectangle<float>(4.0f, 2.0f, keyGutterWidth - 6.0f, 14.0f), juce::Justification::centredLeft, false);

    if (panel->timelineModel == nullptr)
        return;

    auto* notes = panel->timelineModel->getMidiNotes(panel->editingClipIndex);
    if (notes == nullptr)
        return;

    auto scrollX = panel->gridViewport.getViewPositionX();
    auto height = getHeight();

    for (const auto& note : *notes)
    {
        auto x = panel->beatToX(note.startBeats) - scrollX;
        if (x < -8.0f || x > getWidth())
            continue;

        bool selected = panel->selectedNoteIds.contains(note.id);
        auto barHeight = juce::jmap((float) note.velocity, 0.0f, 127.0f, 4.0f, (float) height - 4.0f);
        auto barBounds = juce::Rectangle<float>(x - 2.0f, (float) height - barHeight, 4.0f, barHeight);
        g.setColour(selected ? noteSelectedColour() : noteColour());
        g.fillRoundedRectangle(barBounds, 1.5f);
    }
}

void MidiEditorPanel::VelocityLane::mouseDown(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handleVelocityMouseDown(event);
}

void MidiEditorPanel::VelocityLane::mouseDrag(const juce::MouseEvent& event)
{
    if (panel != nullptr)
        panel->handleVelocityMouseDrag(event);
}

void MidiEditorPanel::CCLane::paint(juce::Graphics& g)
{
    if (panel == nullptr)
        return;

    g.fillAll(juce::Colour(0xff0a0e14));
    g.setColour(dimText());
    g.setFont(juce::Font(10.0f));
    g.drawText(controllerLaneName(controller), juce::Rectangle<float>(4.0f, 2.0f, keyGutterWidth + 100.0f, 14.0f), juce::Justification::centredLeft, false);

    if (panel->timelineModel == nullptr)
        return;

    auto* ccEvents = panel->timelineModel->getMidiCC(panel->editingClipIndex);
    if (ccEvents == nullptr)
        return;

    auto scrollX = panel->gridViewport.getViewPositionX();
    auto height = (float) getHeight();

    juce::Path line;
    bool started = false;
    juce::Array<juce::Point<float>> points;

    for (const auto& point : *ccEvents)
    {
        if (point.controller != controller)
            continue;

        auto x = panel->beatToX(point.beats) - scrollX;
        auto y = juce::jmap((float) point.value, 0.0f, 127.0f, height - 4.0f, 4.0f);
        points.add({ x, y });
    }

    for (const auto& p : points)
    {
        if (p.x < -8.0f || p.x > getWidth())
            continue;

        if (! started)
        {
            line.startNewSubPath(p);
            started = true;
        }
        else
        {
            line.lineTo(p);
        }
    }

    if (started)
    {
        g.setColour(noteColour());
        g.strokePath(line, juce::PathStrokeType(1.5f));
    }

    g.setColour(noteColour());
    for (const auto& p : points)
    {
        if (p.x < -8.0f || p.x > getWidth())
            continue;

        g.fillEllipse(p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);
    }
}

void MidiEditorPanel::CCLane::mouseDown(const juce::MouseEvent& event)
{
    drawPointAt(event);
}

void MidiEditorPanel::CCLane::mouseDrag(const juce::MouseEvent& event)
{
    drawPointAt(event);
}

void MidiEditorPanel::CCLane::drawPointAt(const juce::MouseEvent& event)
{
    if (panel == nullptr || panel->timelineModel == nullptr)
        return;

    auto scrollX = panel->gridViewport.getViewPositionX();
    auto beat = panel->xToBeat(event.position.x + scrollX);
    auto height = (float) getHeight();
    auto value = juce::jlimit(0, 127, (int) juce::jmap(event.position.y, height - 4.0f, 4.0f, 0.0f, 127.0f));

    panel->timelineModel->addOrUpdateMidiCCPoint(panel->editingClipIndex, controller, beat, value);
    repaint();

    if (panel->onNotesChanged)
        panel->onNotesChanged();
}

MidiEditorPanel::MidiEditorPanel()
{
    titleLabel.setText("Midi Editor", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(28.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    clipLabel.setColour(juce::Label::textColourId, dimText());
    clipLabel.setFont(juce::Font(12.5f));
    clipLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(clipLabel);

    auto styleHeaderButton = [](juce::Button& button, const juce::String& tooltip)
    {
        button.setTooltip(tooltip);
        button.setMouseClickGrabsKeyboardFocus(false);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff26303b));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff1f5f86));
        button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    };

    closeButton.setButtonText("Close");
    closeButton.onClick = [this] { if (onCloseRequested) onCloseRequested(); };
    closeButton.setTooltip("Close the MIDI editor");
    styleHeaderButton(closeButton, "Close the MIDI editor");
    addAndMakeVisible(closeButton);

    playButton.setLookAndFeel(&getMidiTransportButtonLookAndFeel());
    playButton.setButtonText("play");
    styleHeaderButton(playButton, "Play from the MIDI editor playhead");
    playButton.onClick = [this] { if (onPlayRequested) onPlayRequested(); };
    addAndMakeVisible(playButton);

    stopButton.setLookAndFeel(&getMidiTransportButtonLookAndFeel());
    stopButton.setButtonText("stop");
    styleHeaderButton(stopButton, "Stop playback");
    stopButton.onClick = [this] { if (onStopRequested) onStopRequested(); };
    addAndMakeVisible(stopButton);

    loopToggle.setLookAndFeel(&getMidiTransportButtonLookAndFeel());
    loopToggle.setButtonText("loop");
    styleHeaderButton(loopToggle, "Enable or disable loop playback for the selected loop region");
    loopToggle.onClick = [this]
    {
        localLoopEnabled = loopToggle.getToggleState();
        if (onLoopEnabledChanged)
            onLoopEnabledChanged(loopToggle.getToggleState());
    };
    addAndMakeVisible(loopToggle);

    gridSizeCombo.addItem("1/4", 1);
    gridSizeCombo.addItem("1/8", 2);
    gridSizeCombo.addItem("1/16", 3);
    gridSizeCombo.addItem("1/8 Triplet", 4);
    gridSizeCombo.addItem("1 Bar", 5);
    gridSizeCombo.setSelectedId(2, juce::dontSendNotification);
    addAndMakeVisible(gridSizeCombo);

    snapToggle.setButtonText("Snap");
    snapToggle.setToggleState(true, juce::dontSendNotification);
    snapToggle.setTooltip("When on, note start and length follow the selected grid. Turn it off for free placement.");
    addAndMakeVisible(snapToggle);

    quantizeStrengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    quantizeStrengthSlider.setRange(0.0, 1.0, 0.01);
    quantizeStrengthSlider.setValue(1.0, juce::dontSendNotification);
    quantizeStrengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
    addAndMakeVisible(quantizeStrengthSlider);

    quantizeButton.onClick = [this] { quantizeSelectedNotes(); };
    quantizeButton.setTooltip("Snap the selected notes to the grid");
    addAndMakeVisible(quantizeButton);

    swingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    swingSlider.setRange(-1.0, 1.0, 0.01);
    swingSlider.setValue(0.3, juce::dontSendNotification);
    swingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
    addAndMakeVisible(swingSlider);

    swingButton.onClick = [this] { applySwingToSelection(); };
    swingButton.setTooltip("Apply swing timing to the selected notes");
    addAndMakeVisible(swingButton);

    zoomInButton.onClick = [this]
    {
        applyHorizontalZoom(1.8, (float) gridViewport.getWidth() * 0.5f);
    };
    styleHeaderButton(zoomInButton, "Zoom in");
    addAndMakeVisible(zoomInButton);

    zoomOutButton.onClick = [this]
    {
        applyHorizontalZoom(1.0 / 1.8, (float) gridViewport.getWidth() * 0.5f);
    };
    styleHeaderButton(zoomOutButton, "Zoom out");
    addAndMakeVisible(zoomOutButton);

    humanizeButton.onClick = [this] { humanizeSelection(); };
    humanizeButton.setTooltip("Add subtle random variation to the selected notes");
    addAndMakeVisible(humanizeButton);

    humanizeTimingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    humanizeTimingSlider.setRange(0.0, 0.25, 0.005);
    humanizeTimingSlider.setValue(0.03, juce::dontSendNotification);
    humanizeTimingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
    humanizeTimingSlider.setTooltip("Timing randomization amount (beats)");
    addAndMakeVisible(humanizeTimingSlider);

    humanizeVelocitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    humanizeVelocitySlider.setRange(0.0, 40.0, 1.0);
    humanizeVelocitySlider.setValue(10.0, juce::dontSendNotification);
    humanizeVelocitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
    humanizeVelocitySlider.setTooltip("Velocity randomization amount");
    addAndMakeVisible(humanizeVelocitySlider);

    humanizeLengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    humanizeLengthSlider.setRange(0.0, 0.5, 0.01);
    humanizeLengthSlider.setValue(0.05, juce::dontSendNotification);
    humanizeLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
    humanizeLengthSlider.setTooltip("Length randomization amount (fraction)");
    addAndMakeVisible(humanizeLengthSlider);

    laneSelectorCombo.addItem("Velocity", 1);
    laneSelectorCombo.addItem("CC1 - Mod Wheel", 2);
    laneSelectorCombo.addItem("CC10 - Pan", 3);
    laneSelectorCombo.addItem("CC11 - Expression", 4);
    laneSelectorCombo.addItem("CC64 - Sustain", 5);
    laneSelectorCombo.addItem("CC91 - Reverb Send", 6);
    laneSelectorCombo.setSelectedId(1, juce::dontSendNotification);
    laneSelectorCombo.onChange = [this] { updateLaneVisibility(); };
    addAndMakeVisible(laneSelectorCombo);

    generateTestBeatButton.setTooltip("Clears this clip and fills it with a basic kick/snare/hi-hat pattern for one minute, for testing playback");
    generateTestBeatButton.onClick = [this] { generateTestBeat(); };
    addAndMakeVisible(generateTestBeatButton);

    hintLabel.setText("Double-click to add a note. Drag to move or resize. Turn Snap off for free timing.", juce::dontSendNotification);
    hintLabel.setColour(juce::Label::textColourId, dimText());
    hintLabel.setFont(juce::Font(11.0f));
    addAndMakeVisible(hintLabel);

    pianoLane.setPanel(this);
    addAndMakeVisible(pianoLane);

    timeRuler.setPanel(this);
    addAndMakeVisible(timeRuler);

    gridView.setPanel(this);
    gridViewport.setViewedComponent(&gridView, false);
    gridViewport.setScrollBarsShown(true, true);
    addAndMakeVisible(gridViewport);

    velocityLane.setPanel(this);
    addAndMakeVisible(velocityLane);

    ccLane.setPanel(this);
    addChildComponent(ccLane);

    setWantsKeyboardFocus(true);
}

void MidiEditorPanel::setClip(cs::TimelineModel* model, int clipIndex)
{
    timelineModel = model;
    editingClipIndex = clipIndex;
    clearSelection();
    rebuildNoteBounds();

    if (timelineModel != nullptr && juce::isPositiveAndBelow(clipIndex, (int) timelineModel->getClips().size()))
        clipLabel.setText(timelineModel->getClips()[(size_t) clipIndex].displayName, juce::dontSendNotification);

    updateLaneVisibility();
    resized();

    // Land the viewport on a musically sane octave instead of the top of the MIDI range -
    // otherwise the first note anyone adds ends up around C9, inaudible on most instruments.
    int targetPitch = 60;
    if (timelineModel != nullptr)
    {
        if (auto* notes = timelineModel->getMidiNotes(editingClipIndex); notes != nullptr && ! notes->empty())
        {
            double sum = 0.0;
            for (const auto& note : *notes)
                sum += note.pitch;
            targetPitch = (int) std::round(sum / (double) notes->size());
        }
    }
    auto viewportHeight = gridViewport.getHeight();
    auto targetY = (int) pitchToY(targetPitch) - viewportHeight / 2;
    gridViewport.setViewPosition(0, juce::jmax(0, targetY));
    localTransportSeconds = 0.0;
    displayedTransportSeconds = 0.0;
    transportDrivenByMain = false;
    localLoopEnabled = false;
    localLoopStartSeconds = 0.0;
    localLoopEndSeconds = 0.0;
    loopToggle.setToggleState(false, juce::dontSendNotification);

    grabKeyboardFocus();
    startTimerHz(30);
    repaint();
}

void MidiEditorPanel::closeClip()
{
    timelineModel = nullptr;
    editingClipIndex = -1;
    clearSelection();
    stopTimer();
    repaint();
}

void MidiEditorPanel::setPlaybackState(bool playing, bool recording)
{
    isPlaying = playing;
    isRecording = recording;
    playButton.setEnabled(! recording);
    loopToggle.setToggleState(localLoopEnabled, juce::dontSendNotification);
    timeRuler.repaint();
    gridView.repaint();
}

void MidiEditorPanel::setDisplayedTransportSeconds(double seconds, bool fromMainTransport)
{
    displayedTransportSeconds = juce::jmax(0.0, seconds);
    transportDrivenByMain = fromMainTransport;
    if (! fromMainTransport)
        localTransportSeconds = displayedTransportSeconds;
    if (isPlaying)
        keepPlayheadVisible();
    timeRuler.repaint();
    gridView.repaint();
}

void MidiEditorPanel::keepPlayheadVisible()
{
    if (timelineModel == nullptr || gridViewport.getWidth() <= 0)
        return;

    auto playheadBeat = timelineModel->secondsToBeat(displayedTransportSeconds);
    auto playheadX = beatToX(playheadBeat);
    auto currentX = (float) gridViewport.getViewPositionX();
    auto viewWidth = (float) gridViewport.getWidth();
    auto leftGuard = currentX + 48.0f;
    auto rightGuard = currentX + viewWidth - 92.0f;

    if (playheadX < leftGuard || playheadX > rightGuard)
    {
        auto targetX = juce::roundToInt(playheadX - viewWidth * 0.35f);
        gridViewport.setViewPosition(juce::jmax(0, targetX), gridViewport.getViewPositionY());
    }
}

int MidiEditorPanel::totalBeatsVisible() const
{
    if (timelineModel == nullptr || ! juce::isPositiveAndBelow(editingClipIndex, (int) timelineModel->getClips().size()))
        return 32;

    const auto& clip = timelineModel->getClips()[(size_t) editingClipIndex];
    auto clipBeats = timelineModel->secondsToBeat(clip.durationSeconds);
    return juce::jmax(32, (int) std::ceil(clipBeats) + 8);
}

double MidiEditorPanel::xToBeat(float x) const
{
    return juce::jmax(0.0, (double) x / pixelsPerBeat());
}

int MidiEditorPanel::yToPitch(float y) const
{
    auto row = (int) (y / pixelsPerPitch());
    return juce::jlimit(0, 127, 127 - row);
}

float MidiEditorPanel::beatToX(double beat) const
{
    return (float) (beat * pixelsPerBeat());
}

float MidiEditorPanel::pitchToY(int pitch) const
{
    return (float) (127 - pitch) * pixelsPerPitch();
}

double MidiEditorPanel::snapBeat(double beat) const
{
    if (! snapToggle.getToggleState())
        return beat;

    auto gridBeats = gridBeatsForComboId(gridSizeCombo.getSelectedId());
    if (gridBeats <= 0.0)
        return beat;
    return std::round(beat / gridBeats) * gridBeats;
}

void MidiEditorPanel::rebuildNoteBounds()
{
    noteBounds.clear();

    if (timelineModel == nullptr)
        return;

    auto* notes = timelineModel->getMidiNotes(editingClipIndex);
    if (notes == nullptr)
        return;

    for (const auto& note : *notes)
    {
        auto x = beatToX(note.startBeats);
        auto y = pitchToY(note.pitch);
        auto w = (float) (note.lengthBeats * pixelsPerBeat());
        noteBounds.add(juce::Rectangle<float>(x, y + 1.0f, juce::jmax(4.0f, w), pixelsPerPitch() - 2.0f));
    }

    auto contentWidth = (int) (totalBeatsVisible() * pixelsPerBeat());
    auto contentHeight = (int) (128 * pixelsPerPitch());
    gridView.setSize(contentWidth, contentHeight);
}

void MidiEditorPanel::handleGridMouseDown(const juce::MouseEvent& event)
{
    if (timelineModel == nullptr)
        return;

    auto* notes = timelineModel->getMidiNotes(editingClipIndex);
    if (notes == nullptr)
        return;

    // Hit-test notes topmost-first
    for (int i = noteBounds.size() - 1; i >= 0; --i)
    {
        if (! noteBounds[i].contains(event.position))
            continue;

        const auto& note = (*notes)[(size_t) i];
        bool addToSelection = event.mods.isShiftDown() || event.mods.isCommandDown() || event.mods.isCtrlDown();

        if (! selectedNoteIds.contains(note.id))
            selectNote(note.id, addToSelection);

        const auto resizeZoneStart = noteBounds[i].getRight() - noteResizeHandleWidth;
        const auto resizeZoneEnd = noteBounds[i].getX() + noteResizeHandleWidth;
        dragState.resizeEdge = DragState::ResizeEdge::none;
        if (event.position.x >= resizeZoneStart)
            dragState.resizeEdge = DragState::ResizeEdge::right;
        else if (event.position.x <= resizeZoneEnd)
            dragState.resizeEdge = DragState::ResizeEdge::left;

        dragState.mode = dragState.resizeEdge != DragState::ResizeEdge::none ? DragMode::resizeNotes
                                                                              : DragMode::moveNotes;
        dragState.startPos = event.position;
        dragState.primaryNoteId = note.id;
        sustainNoteClickAudition = true;
        dragState.noteSnapshots.clear();
        for (const auto& selectedNoteId : selectedNoteIds)
        {
            auto it = std::find_if(notes->begin(), notes->end(), [&](const cs::MidiNoteEvent& n) { return n.id == selectedNoteId; });
            if (it != notes->end())
                dragState.noteSnapshots.add({ it->id, it->startBeats, it->lengthBeats, it->pitch });
        }

        auditioningPitch = note.pitch;
        if (onAuditionNote)
            onAuditionNote(note.pitch, note.velocity, true);

        return;
    }

    // Empty space: start marquee (unless modifier already clears)
    if (! event.mods.isShiftDown() && ! event.mods.isCommandDown() && ! event.mods.isCtrlDown())
        clearSelection();

    dragState.mode = DragMode::marquee;
    dragState.resizeEdge = DragState::ResizeEdge::none;
    dragState.startPos = event.position;
    sustainNoteClickAudition = false;
    marqueeRect = {};
    repaint();
}

void MidiEditorPanel::handleGridMouseDrag(const juce::MouseEvent& event)
{
    if (timelineModel == nullptr)
        return;

    if (dragState.mode == DragMode::marquee)
    {
        marqueeRect = juce::Rectangle<float>(dragState.startPos, event.position);
        sustainNoteClickAudition = false;
        gridView.repaint();
        return;
    }

    auto* notes = timelineModel->getMidiNotes(editingClipIndex);
    if (notes == nullptr)
        return;

    auto deltaBeats = snapBeat(xToBeat(event.position.x) - xToBeat(dragState.startPos.x));
    auto deltaPitch = yToPitch(event.position.y) - yToPitch(dragState.startPos.y);
    if (std::abs(deltaBeats) > 0.0 || deltaPitch != 0)
        sustainNoteClickAudition = false;

    if (dragState.mode == DragMode::moveNotes)
    {
        for (const auto& snapshot : dragState.noteSnapshots)
        {
            auto newBeat = juce::jmax(0.0, snapshot.startBeats + deltaBeats);
            auto newPitch = juce::jlimit(0, 127, snapshot.pitch + deltaPitch);
            timelineModel->updateMidiNote(editingClipIndex, snapshot.id, newPitch, newBeat, snapshot.lengthBeats);
        }

        extendClipToFitNotes();
        rebuildNoteBounds();
        gridView.repaint();
        pianoLane.repaint();
        velocityLane.repaint();
        if (onNotesChanged)
            onNotesChanged();
    }
    else if (dragState.mode == DragMode::resizeNotes)
    {
        auto it = std::find_if(notes->begin(), notes->end(), [&](const cs::MidiNoteEvent& n) { return n.id == dragState.primaryNoteId; });
        if (it != notes->end())
        {
            auto original = std::find_if(dragState.noteSnapshots.begin(), dragState.noteSnapshots.end(),
                                         [&](const DragState::NoteSnapshot& snapshot) { return snapshot.id == dragState.primaryNoteId; });
            auto baseLength = original != dragState.noteSnapshots.end() ? original->lengthBeats : it->lengthBeats;
            auto baseStart = original != dragState.noteSnapshots.end() ? original->startBeats : it->startBeats;
            auto minimumLength = snapToggle.getToggleState() ? gridBeatsForComboId(gridSizeCombo.getSelectedId())
                                                             : freePlacementMinimumBeats;

            if (dragState.resizeEdge == DragState::ResizeEdge::left)
            {
                auto baseEnd = baseStart + baseLength;
                auto candidateStart = juce::jmax(0.0, baseStart + deltaBeats);
                auto maxStart = baseEnd - minimumLength;
                auto newStart = juce::jmin(candidateStart, maxStart);
                auto newLength = juce::jmax(minimumLength, baseEnd - newStart);
                timelineModel->updateMidiNote(editingClipIndex, it->id, it->pitch, newStart, newLength);
            }
            else
            {
                auto newLength = juce::jmax(minimumLength, baseLength + deltaBeats);
                timelineModel->updateMidiNote(editingClipIndex, it->id, it->pitch, it->startBeats, newLength);
            }

            extendClipToFitNotes();
            rebuildNoteBounds();
            gridView.repaint();
            pianoLane.repaint();
            if (onNotesChanged)
                onNotesChanged();
        }
    }
}

void MidiEditorPanel::handleGridMouseUp(const juce::MouseEvent&)
{
    if (dragState.mode == DragMode::marquee && ! marqueeRect.isEmpty() && timelineModel != nullptr)
    {
        auto* notes = timelineModel->getMidiNotes(editingClipIndex);
        if (notes != nullptr)
        {
            for (int i = 0; i < noteBounds.size() && i < (int) notes->size(); ++i)
            {
                if (marqueeRect.intersects(noteBounds[i]))
                    selectNote((*notes)[(size_t) i].id, true);
            }
        }
    }

    dragState.mode = DragMode::none;
    dragState.resizeEdge = DragState::ResizeEdge::none;
    dragState.noteSnapshots.clear();
    marqueeRect = {};
    gridView.repaint();
    pianoLane.repaint();

    if (auditioningPitch >= 0)
    {
        if (sustainNoteClickAudition)
        {
            auto pitch = auditioningPitch;
            juce::Component::SafePointer<MidiEditorPanel> safeThis(this);
            juce::Timer::callAfterDelay(140, [safeThis, pitch]
            {
                if (safeThis != nullptr && safeThis->onAuditionNote)
                    safeThis->onAuditionNote(pitch, 0, false);
            });
        }
        else if (onAuditionNote)
        {
            onAuditionNote(auditioningPitch, 0, false);
        }
        auditioningPitch = -1;
    }
    sustainNoteClickAudition = false;
}

void MidiEditorPanel::handleGridDoubleClick(const juce::MouseEvent& event)
{
    if (timelineModel == nullptr)
        return;

    auto beat = snapBeat(xToBeat(event.position.x));
    auto pitch = yToPitch(event.position.y);
    auto gridBeats = snapToggle.getToggleState() ? gridBeatsForComboId(gridSizeCombo.getSelectedId())
                                                 : 0.5;

    auto newId = timelineModel->addMidiNote(editingClipIndex, pitch, beat, gridBeats, 100);
    if (newId.isNotEmpty())
    {
        extendClipToFitNotes();
        rebuildNoteBounds();
        selectNote(newId, false);
        gridView.repaint();
        pianoLane.repaint();
        velocityLane.repaint();
        if (onNotesChanged)
            onNotesChanged();

        if (onAuditionNote)
        {
            onAuditionNote(pitch, 100, true);

            juce::Component::SafePointer<MidiEditorPanel> safeThis(this);
            juce::Timer::callAfterDelay(200, [safeThis, pitch]
            {
                if (safeThis != nullptr && safeThis->onAuditionNote)
                    safeThis->onAuditionNote(pitch, 0, false);
            });
        }
    }
}

void MidiEditorPanel::handleGridMouseMove(const juce::MouseEvent& event)
{
    updateHoverBeat(xToBeat(event.position.x));

    juce::MouseCursor nextCursor = juce::MouseCursor::NormalCursor;
    for (int i = noteBounds.size() - 1; i >= 0; --i)
    {
        if (! noteBounds[i].contains(event.position))
            continue;

        const auto& bounds = noteBounds.getReference(i);
        if (event.position.x <= bounds.getX() + noteResizeHandleWidth
            || event.position.x >= bounds.getRight() - noteResizeHandleWidth)
            nextCursor = juce::MouseCursor::LeftRightResizeCursor;
        else
            nextCursor = juce::MouseCursor::DraggingHandCursor;
        break;
    }

    gridView.setMouseCursor(nextCursor);
}

void MidiEditorPanel::handleGridMouseExit()
{
    clearHoverBeat();
    gridView.setMouseCursor(juce::MouseCursor::NormalCursor);
}

void MidiEditorPanel::handleVelocityMouseDown(const juce::MouseEvent& event)
{
    handleVelocityMouseDrag(event);
}

void MidiEditorPanel::handlePianoLaneMouseDown(const juce::MouseEvent& event)
{
    auto scrollY = (float) gridViewport.getViewPositionY();
    auto pitch = yToPitch(event.position.y + scrollY);
    sustainNoteClickAudition = false;
    auditioningPitch = pitch;
    if (onAuditionNote)
        onAuditionNote(pitch, 100, true);
}

void MidiEditorPanel::handlePianoLaneMouseDrag(const juce::MouseEvent& event)
{
    auto scrollY = (float) gridViewport.getViewPositionY();
    auto pitch = yToPitch(event.position.y + scrollY);
    sustainNoteClickAudition = false;
    if (pitch == auditioningPitch)
        return;

    if (auditioningPitch >= 0 && onAuditionNote)
        onAuditionNote(auditioningPitch, 0, false);

    auditioningPitch = pitch;
    if (onAuditionNote)
        onAuditionNote(pitch, 100, true);
}

void MidiEditorPanel::handlePianoLaneMouseUp()
{
    if (auditioningPitch >= 0 && onAuditionNote)
        onAuditionNote(auditioningPitch, 0, false);
    auditioningPitch = -1;
}

void MidiEditorPanel::handleVelocityMouseDrag(const juce::MouseEvent& event)
{
    if (timelineModel == nullptr || selectedNoteIds.isEmpty())
        return;

    auto height = (float) velocityLane.getHeight();
    auto velocity = juce::jlimit(1, 127, (int) juce::jmap(height - event.position.y, 4.0f, height - 4.0f, 1.0f, 127.0f));

    for (const auto& noteId : selectedNoteIds)
        timelineModel->setMidiNoteVelocity(editingClipIndex, noteId, velocity);

    velocityLane.repaint();
    gridView.repaint();
    if (onNotesChanged)
        onNotesChanged();
}

void MidiEditorPanel::handleTimeRulerMouseMove(const juce::MouseEvent& event)
{
    auto scrollX = (float) gridViewport.getViewPositionX();
    updateHoverBeat(xToBeat(event.position.x + scrollX));
}

void MidiEditorPanel::handleTimeRulerMouseDown(const juce::MouseEvent& event)
{
    auto scrollX = (float) gridViewport.getViewPositionX();
    rulerDragStartBeat = xToBeat(event.position.x + scrollX);
    rulerDragCurrentBeat = rulerDragStartBeat;
    rulerDraggingLoop = true;
    rulerLoopSelectionChanged = false;
    updateHoverBeat(rulerDragStartBeat);
}

void MidiEditorPanel::handleTimeRulerMouseDrag(const juce::MouseEvent& event)
{
    auto scrollX = (float) gridViewport.getViewPositionX();
    rulerDragCurrentBeat = xToBeat(event.position.x + scrollX);
    rulerLoopSelectionChanged = std::abs(rulerDragCurrentBeat - rulerDragStartBeat) >= 0.125;
    updateHoverBeat(rulerDragCurrentBeat);
    timeRuler.repaint();
}

void MidiEditorPanel::handleTimeRulerMouseUp(const juce::MouseEvent&)
{
    if (! rulerDraggingLoop)
        return;

    if (rulerLoopSelectionChanged)
    {
        auto startBeat = juce::jmin(rulerDragStartBeat, rulerDragCurrentBeat);
        auto endBeat = juce::jmax(rulerDragStartBeat, rulerDragCurrentBeat);
        if (timelineModel != nullptr && onLoopRegionChanged)
        {
            localLoopStartSeconds = timelineModel->beatToSeconds(startBeat);
            localLoopEndSeconds = timelineModel->beatToSeconds(endBeat);
            localLoopEnabled = true;
            loopToggle.setToggleState(true, juce::dontSendNotification);
            onLoopRegionChanged(localLoopStartSeconds, localLoopEndSeconds);
        }
    }
    else if (timelineModel != nullptr && onTransportChanged)
    {
        localTransportSeconds = timelineModel->beatToSeconds(rulerDragStartBeat);
        displayedTransportSeconds = localTransportSeconds;
        transportDrivenByMain = false;
        onTransportChanged(localTransportSeconds);
    }

    rulerDraggingLoop = false;
    rulerLoopSelectionChanged = false;
    timeRuler.repaint();
    gridView.repaint();
}

void MidiEditorPanel::handleMouseWheel(const juce::MouseEvent& event,
                                       const juce::MouseWheelDetails& wheel,
                                       bool fromGrid)
{
    if (std::abs(wheel.deltaY) > std::abs(wheel.deltaX) && (event.mods.isCtrlDown() || event.mods.isAltDown() || event.mods.isCommandDown()))
    {
        auto anchorX = fromGrid ? event.position.x : event.position.x + (float) gridViewport.getViewPositionX();
        applyHorizontalZoom(wheel.deltaY > 0.0f ? 1.8 : (1.0 / 1.8), anchorX);
        return;
    }

    auto currentX = gridViewport.getViewPositionX();
    auto currentY = gridViewport.getViewPositionY();
    auto horizontalDelta = std::abs(wheel.deltaX) > 0.001f ? wheel.deltaX : (event.mods.isShiftDown() ? wheel.deltaY : 0.0f);
    auto verticalDelta = event.mods.isShiftDown() ? 0.0f : wheel.deltaY;

    auto nextX = currentX - juce::roundToInt(horizontalDelta * 180.0f);
    auto nextY = currentY - juce::roundToInt(verticalDelta * 90.0f);
    gridViewport.setViewPosition(juce::jmax(0, nextX), juce::jmax(0, nextY));

    if (! fromGrid)
        updateHoverBeat(xToBeat(event.position.x + (float) gridViewport.getViewPositionX()));
}

void MidiEditorPanel::updateHoverBeat(double beat)
{
    auto clamped = juce::jlimit(0.0, (double) totalBeatsVisible(), beat);
    if (std::abs(clamped - hoveredBeat) < 0.0001)
        return;

    hoveredBeat = clamped;
    gridView.repaint();
    timeRuler.repaint();
}

void MidiEditorPanel::clearHoverBeat()
{
    if (hoveredBeat < 0.0)
        return;

    hoveredBeat = -1.0;
    gridView.repaint();
    timeRuler.repaint();
}

void MidiEditorPanel::applyHorizontalZoom(double factor, float anchorX)
{
    auto clampedFactor = juce::jlimit(0.05, 20.0, factor);
    auto anchorBeat = xToBeat(anchorX + (float) gridViewport.getViewPositionX());
    horizontalZoom = juce::jlimit(0.125, 16.0, horizontalZoom * clampedFactor);
    rebuildNoteBounds();
    auto newViewX = juce::roundToInt(beatToX(anchorBeat) - anchorX);
    gridViewport.setViewPosition(juce::jmax(0, newViewX), gridViewport.getViewPositionY());
    gridView.repaint();
    timeRuler.repaint();
    pianoLane.repaint();
    velocityLane.repaint();
    ccLane.repaint();
}

void MidiEditorPanel::selectNote(const juce::String& noteId, bool addToSelection)
{
    if (! addToSelection)
        selectedNoteIds.clear();

    if (! selectedNoteIds.contains(noteId))
        selectedNoteIds.add(noteId);

    gridView.repaint();
    pianoLane.repaint();
    velocityLane.repaint();
}

void MidiEditorPanel::clearSelection()
{
    selectedNoteIds.clear();
    gridView.repaint();
    pianoLane.repaint();
    velocityLane.repaint();
}

void MidiEditorPanel::deleteSelectedNotes()
{
    if (timelineModel == nullptr || selectedNoteIds.isEmpty())
        return;

    timelineModel->removeMidiNotes(editingClipIndex, selectedNoteIds);
    clearSelection();
    rebuildNoteBounds();
    gridView.repaint();
    pianoLane.repaint();
    velocityLane.repaint();
    if (onNotesChanged)
        onNotesChanged();
}

void MidiEditorPanel::quantizeSelectedNotes()
{
    if (timelineModel == nullptr)
        return;

    auto gridBeats = gridBeatsForComboId(gridSizeCombo.getSelectedId());
    auto strength = (float) quantizeStrengthSlider.getValue();
    timelineModel->quantizeMidiNotes(editingClipIndex, selectedNoteIds, gridBeats, strength);
    extendClipToFitNotes();
    rebuildNoteBounds();
    gridView.repaint();
    pianoLane.repaint();
    if (onNotesChanged)
        onNotesChanged();
}

void MidiEditorPanel::applySwingToSelection()
{
    if (timelineModel == nullptr)
        return;

    auto gridBeats = gridBeatsForComboId(gridSizeCombo.getSelectedId());
    auto swingAmount = (float) swingSlider.getValue();
    timelineModel->applySwing(editingClipIndex, selectedNoteIds, gridBeats, swingAmount);
    extendClipToFitNotes();
    rebuildNoteBounds();
    gridView.repaint();
    pianoLane.repaint();
    if (onNotesChanged)
        onNotesChanged();
}

void MidiEditorPanel::humanizeSelection()
{
    if (timelineModel == nullptr)
        return;

    auto timingAmount = humanizeTimingSlider.getValue();
    auto velocityAmount = (int) humanizeVelocitySlider.getValue();
    auto lengthAmount = (float) humanizeLengthSlider.getValue();
    timelineModel->humanizeMidiNotes(editingClipIndex, selectedNoteIds, timingAmount, velocityAmount, lengthAmount);
    extendClipToFitNotes();
    rebuildNoteBounds();
    gridView.repaint();
    pianoLane.repaint();
    velocityLane.repaint();
    if (onNotesChanged)
        onNotesChanged();
}

void MidiEditorPanel::updateLaneVisibility()
{
    auto controller = controllerForLaneComboId(laneSelectorCombo.getSelectedId());
    if (controller < 0)
    {
        velocityLane.setVisible(true);
        ccLane.setVisible(false);
    }
    else
    {
        velocityLane.setVisible(false);
        ccLane.setController(controller);
        ccLane.setVisible(true);
    }
}

void MidiEditorPanel::extendClipToFitNotes()
{
    if (timelineModel == nullptr || ! juce::isPositiveAndBelow(editingClipIndex, (int) timelineModel->getClips().size()))
        return;

    auto* notes = timelineModel->getMidiNotes(editingClipIndex);
    if (notes == nullptr || notes->empty())
        return;

    double maxEndBeat = 0.0;
    for (const auto& note : *notes)
        maxEndBeat = juce::jmax(maxEndBeat, note.startBeats + note.lengthBeats);

    auto requiredSeconds = timelineModel->beatToSeconds(maxEndBeat) + 0.05;
    const auto& clip = timelineModel->getClips()[(size_t) editingClipIndex];

    // Only ever grows the clip to fit its notes - never shrinks it out from under content the
    // user placed further along than the clip's current visible length, since the timeline's
    // total/loop duration is derived from clip length, not from the notes inside it. Without
    // this, notes added past the clip's edge silently never play - playback loops back before
    // reaching them.
    if (requiredSeconds > clip.durationSeconds)
        timelineModel->setClipDuration(editingClipIndex, requiredSeconds);
}

void MidiEditorPanel::generateTestBeat()
{
    if (timelineModel == nullptr || ! juce::isPositiveAndBelow(editingClipIndex, (int) timelineModel->getClips().size()))
        return;

    // Clear existing notes in this clip.
    if (auto* existingNotes = timelineModel->getMidiNotes(editingClipIndex))
    {
        juce::StringArray idsToRemove;
        for (const auto& note : *existingNotes)
            idsToRemove.add(note.id);
        timelineModel->removeMidiNotes(editingClipIndex, idsToRemove);
    }
    clearSelection();

    constexpr int kickPitch = 36;
    constexpr int snarePitch = 38;
    constexpr int hatPitch = 42;

    const auto tempoBpm = juce::jmax(20.0, timelineModel->getTempoBpm());
    const auto totalBeats = tempoBpm; // 60 seconds worth of beats at this tempo

    for (double beat = 0.0; beat < totalBeats; beat += 1.0)
    {
        auto beatInBar = std::fmod(beat, 4.0);

        // Kick on 1 and 3, snare on 2 and 4 - a basic rock backbeat.
        if (beatInBar < 0.001 || std::abs(beatInBar - 2.0) < 0.001)
            timelineModel->addMidiNote(editingClipIndex, kickPitch, beat, 0.25, 110);
        if (std::abs(beatInBar - 1.0) < 0.001 || std::abs(beatInBar - 3.0) < 0.001)
            timelineModel->addMidiNote(editingClipIndex, snarePitch, beat, 0.25, 105);
    }

    for (double beat = 0.0; beat < totalBeats; beat += 0.5)
        timelineModel->addMidiNote(editingClipIndex, hatPitch, beat, 0.2, 80);

    timelineModel->setClipDuration(editingClipIndex, 60.0);

    rebuildNoteBounds();
    gridView.repaint();
    pianoLane.repaint();
    velocityLane.repaint();
    ccLane.repaint();

    if (onNotesChanged)
        onNotesChanged();
}

bool MidiEditorPanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelectedNotes();
        return true;
    }

    return false;
}

void MidiEditorPanel::timerCallback()
{
    auto pos = gridViewport.getViewPosition();
    if (pos != lastViewPosition)
    {
        lastViewPosition = pos;
        pianoLane.repaint();
        timeRuler.repaint();
        velocityLane.repaint();
        ccLane.repaint();
    }

    if (isPlaying || rulerDraggingLoop)
    {
        gridView.repaint();
        timeRuler.repaint();
    }
}

void MidiEditorPanel::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());

    auto content = getLocalBounds().reduced(14);
    auto topHeader = content.removeFromTop(88).toFloat();
    g.setColour(juce::Colour(0xff0f1115));
    g.fillRoundedRectangle(topHeader, 18.0f);
    g.setColour(juce::Colour(0xff27364a));
    g.drawRoundedRectangle(topHeader, 18.0f, 1.0f);

    auto transportGlow = juce::Rectangle<float>(topHeader.getX() + 18.0f,
                                                topHeader.getY() + 42.0f,
                                                246.0f,
                                                36.0f).expanded(8.0f, 8.0f);
    g.setColour(juce::Colour(0x1f59dfff));
    g.fillRoundedRectangle(transportGlow, 14.0f);
    g.setColour(juce::Colour(0xff59dfff).withAlpha(0.45f));
    g.drawRoundedRectangle(transportGlow, 14.0f, 1.0f);

    content.removeFromTop(10);
    auto specialRow1 = content.removeFromTop(34).toFloat();
    auto specialRow2 = content.removeFromTop(40).toFloat();

    auto controlsPanel = specialRow1.getUnion(specialRow2).expanded(0.0f, 4.0f);
    g.setColour(juce::Colour(0xff10141a));
    g.fillRoundedRectangle(controlsPanel, 14.0f);
    g.setColour(juce::Colour(0xff263140));
    g.drawRoundedRectangle(controlsPanel, 14.0f, 1.0f);

    auto drawControlGroup = [&g](juce::Rectangle<int> bounds)
    {
        auto group = bounds.toFloat().expanded(8.0f, 6.0f);
        g.setColour(juce::Colour(0xff151b23));
        g.fillRoundedRectangle(group, 10.0f);
        g.setColour(juce::Colour(0xff2d3c4f));
        g.drawRoundedRectangle(group, 10.0f, 1.0f);
    };

    drawControlGroup(gridSizeCombo.getBounds()
                         .getUnion(snapToggle.getBounds())
                         .getUnion(zoomOutButton.getBounds())
                         .getUnion(zoomInButton.getBounds()));
    drawControlGroup(quantizeButton.getBounds()
                         .getUnion(quantizeStrengthSlider.getBounds())
                         .getUnion(swingButton.getBounds())
                         .getUnion(swingSlider.getBounds()));
    drawControlGroup(laneSelectorCombo.getBounds()
                         .getUnion(humanizeButton.getBounds())
                         .getUnion(humanizeTimingSlider.getBounds())
                         .getUnion(humanizeVelocitySlider.getBounds())
                         .getUnion(humanizeLengthSlider.getBounds()));
    drawControlGroup(generateTestBeatButton.getBounds());

    auto titleDividerY = topHeader.getY() + 38.0f;
    g.setColour(juce::Colour(0xff242a36));
    g.drawLine(topHeader.getX() + 16.0f, titleDividerY, topHeader.getRight() - 16.0f, titleDividerY, 1.0f);

    g.setColour(juce::Colour(0xff27364a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 10.0f, 1.0f);
}

void MidiEditorPanel::resized()
{
    auto area = getLocalBounds().reduced(14);

    auto headerPanel = area.removeFromTop(88);
    auto titleRow = headerPanel.removeFromTop(30);
    titleLabel.setBounds(titleRow.removeFromLeft(260));
    closeButton.setBounds(titleRow.removeFromRight(92));
    clipLabel.setBounds(titleRow);

    headerPanel.removeFromTop(12);
    auto transportRow = headerPanel.removeFromTop(36);
    transportRow.removeFromLeft(8);
    auto transportButtons = transportRow.removeFromLeft(228);
    playButton.setBounds(transportButtons.removeFromLeft(68));
    transportButtons.removeFromLeft(8);
    stopButton.setBounds(transportButtons.removeFromLeft(56));
    transportButtons.removeFromLeft(8);
    loopToggle.setBounds(transportButtons.removeFromLeft(56));

    area.removeFromTop(10);

    auto toolRow = area.removeFromTop(34);
    auto timingGroup = toolRow.removeFromLeft(250);
    gridSizeCombo.setBounds(timingGroup.removeFromLeft(104));
    timingGroup.removeFromLeft(8);
    snapToggle.setBounds(timingGroup.removeFromLeft(72));
    timingGroup.removeFromLeft(12);
    zoomOutButton.setBounds(timingGroup.removeFromLeft(30));
    timingGroup.removeFromLeft(6);
    zoomInButton.setBounds(timingGroup.removeFromLeft(30));

    toolRow.removeFromLeft(20);
    auto grooveGroup = toolRow.removeFromLeft(300);
    quantizeButton.setBounds(grooveGroup.removeFromLeft(82));
    grooveGroup.removeFromLeft(8);
    quantizeStrengthSlider.setBounds(grooveGroup.removeFromLeft(128));
    grooveGroup.removeFromLeft(14);
    swingButton.setBounds(grooveGroup.removeFromLeft(68));
    grooveGroup.removeFromLeft(8);
    swingSlider.setBounds(grooveGroup.removeFromLeft(128));

    area.removeFromTop(6);

    auto toolRow2 = area.removeFromTop(34);
    auto laneGroup = toolRow2.removeFromLeft(560);
    laneSelectorCombo.setBounds(laneGroup.removeFromLeft(170));
    laneGroup.removeFromLeft(14);
    humanizeButton.setBounds(laneGroup.removeFromLeft(88));
    laneGroup.removeFromLeft(8);
    humanizeTimingSlider.setBounds(laneGroup.removeFromLeft(118));
    laneGroup.removeFromLeft(6);
    humanizeVelocitySlider.setBounds(laneGroup.removeFromLeft(118));
    laneGroup.removeFromLeft(6);
    humanizeLengthSlider.setBounds(laneGroup.removeFromLeft(118));

    toolRow2.removeFromLeft(20);
    generateTestBeatButton.setBounds(toolRow2.removeFromLeft(170));

    area.removeFromTop(6);
    hintLabel.setBounds(area.removeFromTop(16));
    area.removeFromTop(6);

    auto lowerLaneArea = area.removeFromBottom((int) velocityLaneHeight);
    velocityLane.setBounds(lowerLaneArea);
    ccLane.setBounds(lowerLaneArea);
    area.removeFromBottom(4);

    auto pianoArea = area.removeFromLeft((int) keyGutterWidth);
    pianoLane.setBounds(pianoArea);
    area.removeFromLeft(2);
    auto rulerArea = area.removeFromTop((int) rulerHeight);
    timeRuler.setBounds(rulerArea);
    area.removeFromTop(2);
    gridViewport.setBounds(area);
    rebuildNoteBounds();
    pianoLane.repaint();
    timeRuler.repaint();
}
