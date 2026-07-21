#include "ScorePanel.h"

namespace
{
juce::Colour panelColour() { return juce::Colour(0xff11151c); }
juce::Colour cardColour() { return juce::Colour(0xff1a2030); }
juce::Colour accentColour() { return juce::Colour(0xff6fa8ff); }
juce::Colour noteColour() { return juce::Colour(0xfff2cc60); }
juce::Colour restColour() { return juce::Colour(0xff9fb0c8); }
juce::Colour gridColour() { return juce::Colour(0xff2f3c50); }
juce::Colour labelColour() { return juce::Colour(0xff8ea0b7); }

constexpr float systemHeight = 282.0f;
constexpr float systemSpacing = 18.0f;
constexpr int minimumMeasureCount = 8;
constexpr int pianoRollLowestMidi = 48;
constexpr int pianoRollHighestMidi = 84;
constexpr float pianoRollRowHeight = 22.0f;

struct DurationStyle
{
    float canonicalBeats = 1.0f;
    int denominator = 4;
    bool dotted = false;
};

DurationStyle classifyDuration(float beats)
{
    static const std::array<DurationStyle, 7> styles
    {{
        { 4.0f, 1, false },
        { 3.0f, 2, true },
        { 2.0f, 2, false },
        { 1.5f, 4, true },
        { 1.0f, 4, false },
        { 0.75f, 8, true },
        { 0.5f, 8, false }
    }};

    auto best = styles.front();
    auto bestDistance = std::abs(beats - best.canonicalBeats);

    for (const auto& style : styles)
    {
        auto distance = std::abs(beats - style.canonicalBeats);
        if (distance < bestDistance)
        {
            best = style;
            bestDistance = distance;
        }
    }

    if (beats <= 0.375f)
        return { 0.25f, 16, false };

    return best;
}

float noteYForMidi(int midiNote, juce::Rectangle<float> systemArea)
{
    auto topMidi = 81.0f;
    auto bottomMidi = 43.0f;
    return juce::jmap((float) midiNote, topMidi, bottomMidi, systemArea.getY() + 8.0f, systemArea.getBottom() - 8.0f);
}

void drawFlags(juce::Graphics& g, float stemX, float stemTipY, bool stemUp, int flagCount)
{
    for (int flag = 0; flag < flagCount; ++flag)
    {
        auto offset = (float) flag * 7.0f;
        juce::Path path;
        if (stemUp)
        {
            path.startNewSubPath(stemX, stemTipY + offset);
            path.quadraticTo(stemX + 10.0f, stemTipY + 4.0f + offset, stemX + 12.0f, stemTipY + 14.0f + offset);
        }
        else
        {
            path.startNewSubPath(stemX, stemTipY - offset);
            path.quadraticTo(stemX - 10.0f, stemTipY - 4.0f - offset, stemX - 12.0f, stemTipY - 14.0f - offset);
        }

        g.strokePath(path, juce::PathStrokeType(1.5f));
    }
}

void drawRestSymbol(juce::Graphics& g, juce::Rectangle<float> rect, const DurationStyle& style, bool selected)
{
    g.setColour(selected ? accentColour() : restColour());
    auto cx = rect.getCentreX();
    auto cy = rect.getCentreY();

    switch (style.denominator)
    {
        case 1:
            g.fillRect(cx - 10.0f, cy - 2.0f, 20.0f, 6.0f);
            break;
        case 2:
            g.fillRect(cx - 10.0f, cy - 8.0f, 20.0f, 6.0f);
            break;
        case 4:
        {
            juce::Path path;
            path.startNewSubPath(cx + 4.0f, cy - 18.0f);
            path.lineTo(cx - 3.0f, cy - 2.0f);
            path.lineTo(cx + 5.0f, cy - 2.0f);
            path.lineTo(cx - 5.0f, cy + 16.0f);
            path.lineTo(cx + 2.0f, cy + 16.0f);
            g.strokePath(path, juce::PathStrokeType(2.4f));
            break;
        }
        case 8:
        case 16:
        {
            auto stemTop = cy - 14.0f;
            g.drawLine(cx, stemTop, cx, cy + 10.0f, 2.0f);
            juce::Path hook;
            hook.startNewSubPath(cx, stemTop + 1.0f);
            hook.quadraticTo(cx + 10.0f, stemTop + 6.0f, cx + 5.0f, stemTop + 16.0f);
            g.strokePath(hook, juce::PathStrokeType(1.8f));
            if (style.denominator == 16)
            {
                juce::Path secondHook;
                secondHook.startNewSubPath(cx, stemTop + 8.0f);
                secondHook.quadraticTo(cx + 10.0f, stemTop + 14.0f, cx + 5.0f, stemTop + 24.0f);
                g.strokePath(secondHook, juce::PathStrokeType(1.8f));
            }
            break;
        }
        default:
            g.fillEllipse(rect.reduced(6.0f));
            break;
    }

    if (style.dotted)
        g.fillEllipse(cx + 12.0f, cy - 2.0f, 4.0f, 4.0f);
}
}

int ScorePanel::StaffCanvas::getTotalMeasures() const
{
    auto highestMeasure = 0;
    for (const auto& note : notes)
        highestMeasure = juce::jmax(highestMeasure, note.measure);

    return juce::jmax(minimumMeasureCount, highestMeasure + 1);
}

int ScorePanel::StaffCanvas::getSystemCount() const
{
    return juce::jmax(1, (int) std::ceil((double) getTotalMeasures() / (double) juce::jmax(1, measuresPerSystem)));
}

juce::Rectangle<float> ScorePanel::StaffCanvas::getSystemArea(int systemIndex) const
{
    auto bounds = getLocalBounds().toFloat().reduced(18.0f, 18.0f);
    auto top = bounds.getY() + 18.0f + (float) systemIndex * (systemHeight + systemSpacing);
    return { bounds.getX(), top, bounds.getWidth(), systemHeight };
}

juce::Rectangle<float> ScorePanel::StaffCanvas::getTrebleStaffArea(int systemIndex) const
{
    auto area = getSystemArea(systemIndex);
    return { area.getX() + 32.0f, area.getY() + 20.0f, area.getWidth() - 50.0f, 96.0f };
}

juce::Rectangle<float> ScorePanel::StaffCanvas::getBassStaffArea(int systemIndex) const
{
    auto area = getSystemArea(systemIndex);
    return { area.getX() + 32.0f, area.getY() + 150.0f, area.getWidth() - 50.0f, 96.0f };
}

juce::Point<float> ScorePanel::StaffCanvas::getNotePosition(const NoteEvent& note) const
{
    auto localMeasuresPerSystem = juce::jmax(1, measuresPerSystem);
    auto systemIndex = juce::jmax(0, note.measure / localMeasuresPerSystem);
    auto measureInSystem = note.measure % localMeasuresPerSystem;
    auto systemArea = getSystemArea(systemIndex);
    auto x = juce::jmap((float) measureInSystem + (note.beat - 1.0f) / 4.0f,
                        0.0f,
                        (float) localMeasuresPerSystem,
                        systemArea.getX() + 68.0f,
                        systemArea.getRight() - 30.0f);
    auto y = note.isRest ? systemArea.getCentreY() : noteYForMidi(note.midiNote, systemArea);
    return { x, y };
}

int ScorePanel::StaffCanvas::midiForY(float y, juce::Rectangle<float> systemArea)
{
    return juce::roundToInt(juce::jmap(y,
                                       systemArea.getY() + 8.0f,
                                       systemArea.getBottom() - 8.0f,
                                       81.0f,
                                       43.0f));
}

float ScorePanel::StaffCanvas::beatGridForX(float x, int systemIndex, int& measureOut) const
{
    auto systemArea = getSystemArea(systemIndex);
    auto localMeasuresPerSystem = juce::jmax(1, measuresPerSystem);
    auto normalized = juce::jlimit(0.0f, 0.999f, (x - (systemArea.getX() + 68.0f)) / juce::jmax(1.0f, systemArea.getWidth() - 98.0f));
    auto beatsInSystem = (float) localMeasuresPerSystem * 4.0f;
    auto totalBeats = normalized * beatsInSystem;
    auto absoluteBeat = juce::jlimit(0, juce::jmax(0, (localMeasuresPerSystem * 4) - 1), juce::roundToInt(totalBeats * 4.0f) / 4);
    measureOut = systemIndex * localMeasuresPerSystem + (absoluteBeat / 4);
    return 1.0f + (float) (absoluteBeat % 4);
}

void ScorePanel::StaffCanvas::updateLayoutForWidth(int width)
{
    measuresPerSystem = width >= 1120 ? 8 : width >= 760 ? 6 : 4;
    auto systems = getSystemCount();
    auto totalHeight = juce::roundToInt(18.0f + 18.0f + (float) systems * systemHeight
                                        + (float) juce::jmax(0, systems - 1) * systemSpacing + 18.0f);
    setSize(juce::jmax(320, width), totalHeight);
    repaint();
}

void ScorePanel::StaffCanvas::setNotes(const juce::Array<NoteEvent>& newNotes, int newSelectedIndex)
{
    notes = newNotes;
    selectedIndex = newSelectedIndex;
    repaint();
}

void ScorePanel::StaffCanvas::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(cardColour());
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0xff2a3445));
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    noteBounds.clear();

    auto drawStaff = [&g](juce::Rectangle<float> systemArea, juce::Rectangle<float> area, const juce::String& clef)
    {
        for (int line = 0; line < 5; ++line)
        {
            auto y = juce::jmap((float) line / 4.0f, area.getY(), area.getBottom());
            g.setColour(labelColour().withAlpha(0.7f));
            g.drawLine(area.getX(), y, area.getRight(), y, 1.6f);
        }

        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.setFont(juce::Font(30.0f));
        g.drawText(clef, juce::Rectangle<int>((int) systemArea.getX() + 4, (int) area.getY() - 8, 24, 44), juce::Justification::centred, false);
    };

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText("Grand Staff", getLocalBounds().reduced(16, 10).removeFromTop(20), juce::Justification::centredLeft, false);

    for (int systemIndex = 0; systemIndex < getSystemCount(); ++systemIndex)
    {
        auto systemArea = getSystemArea(systemIndex);
        auto trebleArea = getTrebleStaffArea(systemIndex);
        auto bassArea = getBassStaffArea(systemIndex);

        drawStaff(systemArea, trebleArea, juce::String::fromUTF8("\xF0\x9D\x84\x9E"));
        drawStaff(systemArea, bassArea, juce::String::fromUTF8("\xF0\x9D\x84\xA2"));

        for (int measure = 0; measure <= measuresPerSystem; ++measure)
        {
            auto x = juce::jmap((float) measure / (float) measuresPerSystem, systemArea.getX() + 68.0f, systemArea.getRight() - 20.0f);
            g.setColour(gridColour());
            g.drawVerticalLine((int) x, trebleArea.getY(), bassArea.getBottom());

            if (measure < measuresPerSystem)
            {
                auto actualMeasure = systemIndex * measuresPerSystem + measure;
                g.setColour(labelColour());
                g.setFont(juce::Font(12.0f).boldened());
                g.drawText(juce::String(actualMeasure + 1),
                           juce::Rectangle<int>((int) x + 4, (int) trebleArea.getY() - 18, 28, 14),
                           juce::Justification::left,
                           false);
            }
        }
    }

    for (int index = 0; index < notes.size(); ++index)
    {
        const auto& note = notes.getReference(index);
        auto position = getNotePosition(note);
        auto x = position.x;
        auto y = position.y;
        auto localMeasuresPerSystem = juce::jmax(1, measuresPerSystem);
        auto systemIndex = juce::jmax(0, note.measure / localMeasuresPerSystem);
        auto systemArea = getSystemArea(systemIndex);
        auto bassArea = getBassStaffArea(systemIndex);
        auto noteRect = juce::Rectangle<float>(x - 12.0f, y - 18.0f, 24.0f, 36.0f);
        noteBounds.add(noteRect);

        auto style = classifyDuration(note.durationBeats);
        auto isSelected = index == selectedIndex;

        if (note.isRest)
        {
            drawRestSymbol(g, noteRect, style, isSelected);
        }
        else
        {
            auto ellipse = juce::Rectangle<float>(x - 9.0f, y - 7.0f, 18.0f, 14.0f);
            g.setColour(isSelected ? accentColour() : noteColour());

            auto filledHead = style.denominator >= 4;
            if (filledHead)
                g.fillEllipse(ellipse);
            else
                g.drawEllipse(ellipse, 2.0f);

            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawEllipse(ellipse, 1.0f);

            if (style.denominator != 1)
            {
                auto stemUp = note.midiNote < 64;
                auto stemLength = 24.0f;
                auto stemX = stemUp ? ellipse.getRight() - 2.0f : ellipse.getX() + 2.0f;
                auto stemStartY = ellipse.getCentreY();
                auto stemTipY = stemUp ? ellipse.getY() - stemLength : ellipse.getBottom() + stemLength;
                g.drawLine(stemX, stemStartY, stemX, stemTipY, 1.6f);

                if (style.denominator >= 8)
                    drawFlags(g, stemX, stemTipY, stemUp, style.denominator >= 16 ? 2 : 1);
            }

            if (style.dotted)
                g.fillEllipse(x + 12.0f, y - 2.0f, 4.0f, 4.0f);

            auto middleCY = juce::jmap(60.0f, 81.0f, 43.0f, systemArea.getY() + 8.0f, systemArea.getBottom() - 8.0f);
            if (std::abs(y - middleCY) < 9.0f)
            {
                g.setColour(juce::Colours::white.withAlpha(0.8f));
                g.drawLine(x - 14.0f, middleCY, x + 14.0f, middleCY, 1.4f);
            }
        }

        if (! note.isRest && note.lyric.isNotEmpty())
        {
            g.setColour(juce::Colour(0xffd7deea));
            g.setFont(juce::Font(12.0f));
            g.drawText(note.lyric,
                       juce::Rectangle<int>((int) (x - 30.0f), (int) (bassArea.getBottom() + 18.0f), 60, 18),
                       juce::Justification::centred,
                       true);
        }
    }
}

void ScorePanel::StaffCanvas::mouseDown(const juce::MouseEvent& event)
{
    for (int index = 0; index < noteBounds.size(); ++index)
    {
        if (noteBounds.getReference(index).contains(event.position))
        {
            dragState.noteIndex = index;
            dragState.startMeasure = notes.getReference(index).measure;
            dragState.startBeat = notes.getReference(index).beat;
            dragState.startMidiNote = notes.getReference(index).midiNote;
            dragState.startPosition = event.position;
            if (onNoteSelected)
                onNoteSelected(index);
            return;
        }
    }
}

void ScorePanel::StaffCanvas::mouseDrag(const juce::MouseEvent& event)
{
    if (! juce::isPositiveAndBelow(dragState.noteIndex, notes.size()))
        return;

    auto relativeY = event.position.y - 36.0f;
    auto systemIndex = juce::jlimit(0, juce::jmax(0, getSystemCount() - 1),
                                    (int) std::floor(relativeY / (systemHeight + systemSpacing)));
    int measure = dragState.startMeasure;
    auto beat = beatGridForX(event.position.x, systemIndex, measure);
    auto midi = juce::jlimit(43, 81, midiForY(event.position.y, getSystemArea(systemIndex)));

    if (onNoteDragged)
        onNoteDragged(dragState.noteIndex, midi, measure, beat);
}

void ScorePanel::StaffCanvas::mouseUp(const juce::MouseEvent&)
{
    dragState = {};
    dragState.noteIndex = -1;
}

int ScorePanel::PianoRollCanvas::getTotalMeasures() const
{
    auto highestMeasure = 0;
    for (const auto& note : notes)
        highestMeasure = juce::jmax(highestMeasure, note.measure + (int) std::ceil(note.durationBeats / 4.0f));

    return juce::jmax(minimumMeasureCount, highestMeasure + 1);
}

float ScorePanel::PianoRollCanvas::getPixelsPerBeat() const
{
    auto grid = getGridArea();
    return juce::jmax(18.0f, grid.getWidth() / (float) (juce::jmax(1, visibleMeasures) * 4));
}

juce::Rectangle<float> ScorePanel::PianoRollCanvas::getGridArea() const
{
    return getLocalBounds().toFloat().reduced(18.0f, 18.0f).withTrimmedTop(28.0f).withTrimmedLeft(60.0f);
}

juce::Rectangle<float> ScorePanel::PianoRollCanvas::getNoteBounds(const NoteEvent& note) const
{
    auto grid = getGridArea();
    auto pixelsPerBeat = getPixelsPerBeat();
    auto totalBeats = (float) note.measure * 4.0f + (note.beat - 1.0f);
    auto x = grid.getX() + totalBeats * pixelsPerBeat;
    auto y = grid.getY() + (float) (pianoRollHighestMidi - note.midiNote) * pianoRollRowHeight;
    auto width = juce::jmax(10.0f, note.durationBeats * pixelsPerBeat);
    return { x, y + 2.0f, width, pianoRollRowHeight - 4.0f };
}

int ScorePanel::PianoRollCanvas::midiForY(float y) const
{
    auto grid = getGridArea();
    auto row = juce::jlimit(0, pianoRollHighestMidi - pianoRollLowestMidi,
                            (int) std::floor((y - grid.getY()) / pianoRollRowHeight));
    return pianoRollHighestMidi - row;
}

float ScorePanel::PianoRollCanvas::snappedBeatForX(float x, int& measureOut) const
{
    auto grid = getGridArea();
    auto pixelsPerBeat = getPixelsPerBeat();
    auto totalBeats = juce::jmax(0.0f, (x - grid.getX()) / pixelsPerBeat);
    auto snappedQuarter = juce::roundToInt(totalBeats * 4.0f);
    auto snappedBeats = snappedQuarter / 4.0f;
    auto beatIndex = juce::jmax(0, (int) std::floor(snappedBeats));
    measureOut = beatIndex / 4;
    auto beatInMeasure = 1.0f + (snappedBeats - (float) (measureOut * 4));
    return juce::jlimit(1.0f, 4.75f, beatInMeasure);
}

void ScorePanel::PianoRollCanvas::setNotes(const juce::Array<NoteEvent>& newNotes, int newSelectedIndex)
{
    notes = newNotes;
    selectedIndex = newSelectedIndex;
    repaint();
}

void ScorePanel::PianoRollCanvas::updateLayoutForViewport(int viewportWidth)
{
    visibleMeasures = getTotalMeasures();
    auto width = juce::jmax(viewportWidth, 120 + visibleMeasures * 4 * 28);
    auto height = juce::roundToInt(70.0f + (float) (pianoRollHighestMidi - pianoRollLowestMidi + 1) * pianoRollRowHeight);
    setSize(width, height);
    repaint();
}

void ScorePanel::PianoRollCanvas::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(cardColour());
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0xff2a3445));
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    auto grid = getGridArea();
    auto pixelsPerBeat = getPixelsPerBeat();
    noteBounds.clear();

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText("Piano Roll", getLocalBounds().reduced(16, 10).removeFromTop(20), juce::Justification::centredLeft, false);

    for (int midi = pianoRollHighestMidi; midi >= pianoRollLowestMidi; --midi)
    {
        auto rowIndex = pianoRollHighestMidi - midi;
        auto y = grid.getY() + (float) rowIndex * pianoRollRowHeight;
        auto name = ScorePanel::midiName(midi);
        auto pitchClass = name.retainCharacters("ABCDEFG#");
        auto isBlackKey = juce::StringArray({ "C#", "D#", "F#", "G#", "A#" }).contains(pitchClass);
        g.setColour((isBlackKey ? juce::Colour(0xff18202b) : juce::Colour(0xff1e2734)).withAlpha(0.9f));
        g.fillRect(grid.getX(), y, grid.getWidth(), pianoRollRowHeight);
        g.setColour(gridColour());
        g.drawHorizontalLine((int) y, grid.getX(), grid.getRight());
        g.setColour(labelColour());
        g.setFont(juce::Font(11.0f));
        g.drawText(name, juce::Rectangle<int>(8, (int) y, 44, (int) pianoRollRowHeight), juce::Justification::centredLeft, false);
    }

    for (int beat = 0; beat <= visibleMeasures * 4; ++beat)
    {
        auto x = grid.getX() + (float) beat * pixelsPerBeat;
        auto atMeasure = (beat % 4) == 0;
        g.setColour(atMeasure ? juce::Colour(0xff50617a) : gridColour());
        g.drawVerticalLine((int) x, grid.getY(), grid.getBottom());

        if (atMeasure && beat < visibleMeasures * 4)
        {
            g.setColour(labelColour());
            g.setFont(juce::Font(12.0f).boldened());
            g.drawText(juce::String((beat / 4) + 1), juce::Rectangle<int>((int) x + 4, 10, 26, 14), juce::Justification::left, false);
        }
    }

    for (int index = 0; index < notes.size(); ++index)
    {
        const auto& note = notes.getReference(index);
        auto rect = getNoteBounds(note);
        noteBounds.add(rect);
        g.setColour(index == selectedIndex ? accentColour() : (note.isRest ? restColour() : noteColour()));
        g.fillRoundedRectangle(rect, 5.0f);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawRoundedRectangle(rect, 5.0f, 1.0f);

        if (note.isRest)
        {
            g.drawLine(rect.getX() + 4.0f, rect.getY() + 4.0f, rect.getRight() - 4.0f, rect.getBottom() - 4.0f, 1.4f);
            g.drawLine(rect.getRight() - 4.0f, rect.getY() + 4.0f, rect.getX() + 4.0f, rect.getBottom() - 4.0f, 1.4f);
        }
    }
}

void ScorePanel::PianoRollCanvas::mouseDown(const juce::MouseEvent& event)
{
    for (int index = 0; index < noteBounds.size(); ++index)
    {
        if (noteBounds.getReference(index).contains(event.position))
        {
            dragState.noteIndex = index;
            auto note = notes.getReference(index);
            auto totalStartBeat = (float) note.measure * 4.0f + (note.beat - 1.0f);
            dragState.anchorBeat = totalStartBeat;
            dragState.mode = event.position.x >= noteBounds.getReference(index).getRight() - 10.0f
                ? DragState::Mode::resize
                : DragState::Mode::move;
            if (onNoteSelected)
                onNoteSelected(index);
            return;
        }
    }
}

void ScorePanel::PianoRollCanvas::mouseDrag(const juce::MouseEvent& event)
{
    if (! juce::isPositiveAndBelow(dragState.noteIndex, notes.size()))
        return;

    if (dragState.mode == DragState::Mode::resize)
    {
        int endMeasure = 0;
        auto endBeat = snappedBeatForX(event.position.x, endMeasure);
        auto totalEndBeat = (float) endMeasure * 4.0f + (endBeat - 1.0f);
        auto duration = juce::jmax(0.25f, totalEndBeat - dragState.anchorBeat + 0.25f);
        auto snappedDuration = juce::jmax(0.25f, std::round(duration * 4.0f) / 4.0f);

        if (onNoteResized)
            onNoteResized(dragState.noteIndex, snappedDuration);

        return;
    }

    int measure = 0;
    auto beat = snappedBeatForX(event.position.x, measure);
    auto midi = juce::jlimit(pianoRollLowestMidi, pianoRollHighestMidi, midiForY(event.position.y));

    if (onNoteDragged)
        onNoteDragged(dragState.noteIndex, midi, measure, beat);
}

void ScorePanel::PianoRollCanvas::mouseDoubleClick(const juce::MouseEvent& event)
{
    int measure = 0;
    auto beat = snappedBeatForX(event.position.x, measure);
    auto midi = juce::jlimit(pianoRollLowestMidi, pianoRollHighestMidi, midiForY(event.position.y));

    if (onNoteCreated)
        onNoteCreated(midi, (float) measure, beat);
}

void ScorePanel::PianoRollCanvas::mouseUp(const juce::MouseEvent&)
{
    dragState = {};
    dragState.noteIndex = -1;
    dragState.mode = DragState::Mode::none;
}

ScorePanel::ScorePanel()
{
    notes.add({ 0, 1.0f, 1.0f, 64, false, "Ma" });
    notes.add({ 0, 2.0f, 1.0f, 62, false, "ry" });
    notes.add({ 0, 3.0f, 1.0f, 60, false, "had" });
    notes.add({ 0, 4.0f, 1.0f, 62, false, "a" });
    notes.add({ 1, 1.0f, 1.0f, 64, false, "lit" });
    notes.add({ 1, 2.0f, 1.0f, 64, false, "tle" });
    notes.add({ 1, 3.0f, 1.0f, 64, false, "lamb" });
    selectedNoteIndex = 0;

    titleLabel.setText("Score Studio", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Compose with notation, lyrics, teaching cues, and AI-assisted revision.", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, labelColour());
    addAndMakeVisible(subtitleLabel);

    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa7b6cb));
    addAndMakeVisible(statusLabel);

    auto setupLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(label);
    };

    setupLabel(songTitleLabel, "Song Title");
    songTitleEditor.setText(songTitle, juce::dontSendNotification);
    songTitleEditor.onTextChange = [this]
    {
        songTitle = songTitleEditor.getText().trim();
        updateStatusText();
    };
    addAndMakeVisible(songTitleEditor);

    setupLabel(tempoLabel, "Tempo");
    tempoSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
    tempoSlider.setRange(40.0, 220.0, 1.0);
    tempoSlider.setValue(tempoBpm, juce::dontSendNotification);
    tempoSlider.onValueChange = [this]
    {
        tempoBpm = (int) tempoSlider.getValue();
        updateStatusText();
    };
    addAndMakeVisible(tempoSlider);

    setupLabel(keyLabel, "Key");
    keySelector.addItemList({ "C Major", "G Major", "D Major", "A Minor", "E Minor", "F Major" }, 1);
    keySelector.setText(keySignature, juce::dontSendNotification);
    keySelector.onChange = [this]
    {
        keySignature = keySelector.getText();
        updateStatusText();
    };
    addAndMakeVisible(keySelector);

    setupLabel(timeSigLabel, "Time");
    timeSigSelector.addItemList({ "4/4", "3/4", "6/8", "5/4" }, 1);
    timeSigSelector.setText(timeSignature, juce::dontSendNotification);
    timeSigSelector.onChange = [this]
    {
        timeSignature = timeSigSelector.getText();
        updateStatusText();
    };
    addAndMakeVisible(timeSigSelector);

    scoreViewButton.setClickingTogglesState(true);
    scoreViewButton.onClick = [this]
    {
        activeSurface = EditorSurface::staff;
        refreshEditorSurface();
    };
    addAndMakeVisible(scoreViewButton);

    pianoRollViewButton.setClickingTogglesState(true);
    pianoRollViewButton.onClick = [this]
    {
        activeSurface = EditorSurface::pianoRoll;
        refreshEditorSurface();
    };
    addAndMakeVisible(pianoRollViewButton);

    playScoreButton.onClick = [this]
    {
        if (onPlayRequested)
            onPlayRequested(createPlaybackRequest());
    };
    addAndMakeVisible(playScoreButton);

    addNoteButton.onClick = [this] { addNote(); };
    removeNoteButton.onClick = [this] { removeSelectedNote(); };
    restToggleButton.onClick = [this] { applySelectedNoteEditors(); };
    noteUpButton.onClick = [this] { nudgeSelectedNotePitch(1); };
    noteDownButton.onClick = [this] { nudgeSelectedNotePitch(-1); };
    beatLeftButton.onClick = [this] { nudgeSelectedNoteBeat(-1.0f); };
    beatRightButton.onClick = [this] { nudgeSelectedNoteBeat(1.0f); };
    addAndMakeVisible(addNoteButton);
    addAndMakeVisible(removeNoteButton);
    addAndMakeVisible(restToggleButton);
    addAndMakeVisible(noteUpButton);
    addAndMakeVisible(noteDownButton);
    addAndMakeVisible(beatLeftButton);
    addAndMakeVisible(beatRightButton);

    selectedNoteLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(selectedNoteLabel);

    pitchSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    pitchSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
    pitchSlider.setRange(48.0, 84.0, 1.0);
    pitchSlider.onValueChange = [this] { applySelectedNoteEditors(); };
    addAndMakeVisible(pitchSlider);

    measureSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    measureSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
    measureSlider.setRange(0.0, 7.0, 1.0);
    measureSlider.onValueChange = [this] { applySelectedNoteEditors(); };
    addAndMakeVisible(measureSlider);

    beatSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    beatSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
    beatSlider.setRange(1.0, 4.75, 0.25);
    beatSlider.onValueChange = [this] { applySelectedNoteEditors(); };
    addAndMakeVisible(beatSlider);

    durationSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    durationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
    durationSlider.setRange(0.25, 4.0, 0.25);
    durationSlider.onValueChange = [this] { applySelectedNoteEditors(); };
    addAndMakeVisible(durationSlider);

    lyricEditor.setTextToShowWhenEmpty("Lyric syllable for the selected note", juce::Colour(0xff7f8ea4));
    lyricEditor.onTextChange = [this] { applySelectedNoteEditors(); };
    addAndMakeVisible(lyricEditor);

    aiCoachLabel.setText("AI Teaching / Composition Coach", juce::dontSendNotification);
    aiCoachLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(aiCoachLabel);

    aiCoachEditor.setMultiLine(true);
    aiCoachEditor.setReadOnly(true);
    aiCoachEditor.setText("Use this view to sketch melody, align lyric syllables under notes, and teach by talking through phrase shape, key, rhythm, and revision choices.\n\nSuggested AI tasks:\n- rewrite this phrase with stronger cadence\n- simplify the rhythm for beginners\n- move the lyric stress to the downbeat\n- reharmonize this melody in a minor key", juce::dontSendNotification);
    addAndMakeVisible(aiCoachEditor);

    scoreViewport.setScrollBarsShown(true, true);
    addAndMakeVisible(scoreViewport);

    staffCanvas.onNoteSelected = [this](int index)
    {
        selectedNoteIndex = index;
        refreshEditors();
    };
    staffCanvas.onNoteDragged = [this](int index, int midiNote, int measure, float beat)
    {
        if (! juce::isPositiveAndBelow(index, notes.size()))
            return;

        auto& note = notes.getReference(index);
        note.measure = juce::jmax(0, measure);
        note.beat = juce::jlimit(1.0f, 4.75f, beat);
        if (! note.isRest)
            note.midiNote = juce::jlimit(43, 81, midiNote);
        selectedNoteIndex = index;
        refreshEditors();
        updateStatusText();
    };

    pianoRollCanvas.onNoteSelected = [this](int index)
    {
        selectedNoteIndex = index;
        refreshEditors();
    };
    pianoRollCanvas.onNoteDragged = [this](int index, int midiNote, int measure, float beat)
    {
        if (! juce::isPositiveAndBelow(index, notes.size()))
            return;

        auto& note = notes.getReference(index);
        note.measure = juce::jmax(0, measure);
        note.beat = juce::jlimit(1.0f, 4.75f, beat);
        if (! note.isRest)
            note.midiNote = juce::jlimit(pianoRollLowestMidi, pianoRollHighestMidi, midiNote);
        selectedNoteIndex = index;
        refreshEditors();
        updateStatusText();
    };
    pianoRollCanvas.onNoteResized = [this](int index, float durationBeats)
    {
        if (! juce::isPositiveAndBelow(index, notes.size()))
            return;

        notes.getReference(index).durationBeats = juce::jlimit(0.25f, 8.0f, durationBeats);
        selectedNoteIndex = index;
        refreshEditors();
        updateStatusText();
    };
    pianoRollCanvas.onNoteCreated = [this](int midiNote, float measurePacked, float beat)
    {
        NoteEvent note;
        note.measure = juce::jmax(0, (int) measurePacked);
        note.beat = juce::jlimit(1.0f, 4.75f, beat);
        note.durationBeats = 1.0f;
        note.midiNote = midiNote;
        notes.add(note);
        selectedNoteIndex = notes.size() - 1;
        refreshEditors();
        updateStatusText();
    };

    refreshEditors();
    updateStatusText();
    refreshEditorSurface();
}

juce::ValueTree ScorePanel::createState() const
{
    juce::ValueTree state("ScoreView");
    state.setProperty("songTitle", songTitle, nullptr);
    state.setProperty("tempoBpm", tempoBpm, nullptr);
    state.setProperty("keySignature", keySignature, nullptr);
    state.setProperty("timeSignature", timeSignature, nullptr);
    state.setProperty("selectedNoteIndex", selectedNoteIndex, nullptr);
    state.setProperty("activeSurface", activeSurface == EditorSurface::pianoRoll ? "pianoRoll" : "staff", nullptr);

    for (const auto& note : notes)
    {
        juce::ValueTree noteState("Note");
        noteState.setProperty("measure", note.measure, nullptr);
        noteState.setProperty("beat", note.beat, nullptr);
        noteState.setProperty("durationBeats", note.durationBeats, nullptr);
        noteState.setProperty("midiNote", note.midiNote, nullptr);
        noteState.setProperty("isRest", note.isRest, nullptr);
        noteState.setProperty("lyric", note.lyric, nullptr);
        state.addChild(noteState, -1, nullptr);
    }

    return state;
}

void ScorePanel::restoreState(const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    songTitle = state.getProperty("songTitle", songTitle).toString();
    tempoBpm = (int) state.getProperty("tempoBpm", tempoBpm);
    keySignature = state.getProperty("keySignature", keySignature).toString();
    timeSignature = state.getProperty("timeSignature", timeSignature).toString();
    selectedNoteIndex = (int) state.getProperty("selectedNoteIndex", selectedNoteIndex);
    activeSurface = state.getProperty("activeSurface").toString().equalsIgnoreCase("pianoRoll")
        ? EditorSurface::pianoRoll : EditorSurface::staff;

    notes.clear();
    for (const auto child : state)
    {
        if (! child.hasType("Note"))
            continue;

        NoteEvent note;
        note.measure = (int) child.getProperty("measure", 0);
        note.beat = (float) child.getProperty("beat", 1.0f);
        note.durationBeats = (float) child.getProperty("durationBeats", 1.0f);
        note.midiNote = (int) child.getProperty("midiNote", 60);
        note.isRest = (bool) child.getProperty("isRest", false);
        note.lyric = child.getProperty("lyric").toString();
        notes.add(note);
    }

    if (notes.isEmpty())
        notes.add({ 0, 1.0f, 1.0f, 60, false, {} });

    selectedNoteIndex = juce::jlimit(0, juce::jmax(0, notes.size() - 1), selectedNoteIndex);
    refreshEditors();
    updateStatusText();
    refreshEditorSurface();
}

ScorePanel::PlaybackRequest ScorePanel::createPlaybackRequest() const
{
    PlaybackRequest request;
    request.notes = notes;
    request.tempoBpm = tempoBpm;
    request.songTitle = songTitle;
    return request;
}

void ScorePanel::paint(juce::Graphics& g)
{
    g.fillAll(panelColour());
}

void ScorePanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(32));
    subtitleLabel.setBounds(area.removeFromTop(22));
    statusLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(10);

    auto topBar = area.removeFromTop(28);
    songTitleLabel.setBounds(topBar.removeFromLeft(72));
    songTitleEditor.setBounds(topBar.removeFromLeft(180));
    topBar.removeFromLeft(12);
    tempoLabel.setBounds(topBar.removeFromLeft(46));
    tempoSlider.setBounds(topBar.removeFromLeft(150));
    topBar.removeFromLeft(12);
    keyLabel.setBounds(topBar.removeFromLeft(30));
    keySelector.setBounds(topBar.removeFromLeft(120));
    topBar.removeFromLeft(12);
    timeSigLabel.setBounds(topBar.removeFromLeft(36));
    timeSigSelector.setBounds(topBar.removeFromLeft(90));
    topBar.removeFromLeft(12);
    scoreViewButton.setBounds(topBar.removeFromLeft(72));
    topBar.removeFromLeft(8);
    pianoRollViewButton.setBounds(topBar.removeFromLeft(92));
    topBar.removeFromLeft(12);
    playScoreButton.setBounds(topBar.removeFromLeft(110));
    area.removeFromTop(10);

    auto actionRow = area.removeFromTop(30);
    addNoteButton.setBounds(actionRow.removeFromLeft(92));
    actionRow.removeFromLeft(8);
    removeNoteButton.setBounds(actionRow.removeFromLeft(108));
    actionRow.removeFromLeft(8);
    restToggleButton.setBounds(actionRow.removeFromLeft(72));
    actionRow.removeFromLeft(16);
    noteUpButton.setBounds(actionRow.removeFromLeft(84));
    actionRow.removeFromLeft(8);
    noteDownButton.setBounds(actionRow.removeFromLeft(96));
    actionRow.removeFromLeft(8);
    beatLeftButton.setBounds(actionRow.removeFromLeft(90));
    actionRow.removeFromLeft(8);
    beatRightButton.setBounds(actionRow.removeFromLeft(96));
    area.removeFromTop(10);

    auto lower = area.removeFromBottom(180);
    auto inspector = lower.removeFromLeft(420);
    selectedNoteLabel.setBounds(inspector.removeFromTop(24));
    pitchSlider.setBounds(inspector.removeFromTop(30));
    inspector.removeFromTop(6);
    measureSlider.setBounds(inspector.removeFromTop(30));
    inspector.removeFromTop(6);
    beatSlider.setBounds(inspector.removeFromTop(30));
    inspector.removeFromTop(6);
    durationSlider.setBounds(inspector.removeFromTop(30));
    inspector.removeFromTop(8);
    lyricEditor.setBounds(inspector.removeFromTop(28));

    lower.removeFromLeft(12);
    aiCoachLabel.setBounds(lower.removeFromTop(22));
    aiCoachEditor.setBounds(lower);

    area.removeFromTop(6);
    scoreViewport.setBounds(area);
    refreshEditorSurface();
}

void ScorePanel::refreshEditors()
{
    suppressEditorCallbacks = true;

    songTitleEditor.setText(songTitle, juce::dontSendNotification);
    tempoSlider.setValue(tempoBpm, juce::dontSendNotification);
    keySelector.setText(keySignature, juce::dontSendNotification);
    timeSigSelector.setText(timeSignature, juce::dontSendNotification);

    if (juce::isPositiveAndBelow(selectedNoteIndex, notes.size()))
    {
        const auto& note = notes.getReference(selectedNoteIndex);
        auto thingName = note.isRest ? "Selected Rest: " : "Selected Note: ";
        selectedNoteLabel.setText(thingName + (note.isRest ? juce::String("Rest") : midiName(note.midiNote))
                                      + "  |  " + juce::String(note.durationBeats, 2) + " beat(s)",
                                  juce::dontSendNotification);
        pitchSlider.setValue(note.midiNote, juce::dontSendNotification);
        measureSlider.setValue(note.measure, juce::dontSendNotification);
        beatSlider.setValue(note.beat, juce::dontSendNotification);
        durationSlider.setValue(note.durationBeats, juce::dontSendNotification);
        restToggleButton.setToggleState(note.isRest, juce::dontSendNotification);
        lyricEditor.setText(note.lyric, juce::dontSendNotification);
        pitchSlider.setEnabled(! note.isRest);
        noteUpButton.setEnabled(! note.isRest);
        noteDownButton.setEnabled(! note.isRest);
    }
    else
    {
        selectedNoteLabel.setText("No note selected", juce::dontSendNotification);
        restToggleButton.setToggleState(false, juce::dontSendNotification);
        lyricEditor.setText({}, juce::dontSendNotification);
        pitchSlider.setEnabled(false);
        noteUpButton.setEnabled(false);
        noteDownButton.setEnabled(false);
    }

    auto totalMeasures = 0;
    for (const auto& note : notes)
        totalMeasures = juce::jmax(totalMeasures, note.measure + (int) std::ceil(note.durationBeats / 4.0f));

    measureSlider.setRange(0.0, juce::jmax(7, totalMeasures + 3), 1.0);

    suppressEditorCallbacks = false;
    staffCanvas.setNotes(notes, selectedNoteIndex);
    pianoRollCanvas.setNotes(notes, selectedNoteIndex);
    refreshEditorSurface();
}

void ScorePanel::refreshEditorSurface()
{
    scoreViewButton.setToggleState(activeSurface == EditorSurface::staff, juce::dontSendNotification);
    pianoRollViewButton.setToggleState(activeSurface == EditorSurface::pianoRoll, juce::dontSendNotification);

    auto viewportWidth = juce::jmax(320, scoreViewport.getWidth() - 18);
    if (activeSurface == EditorSurface::staff)
    {
        staffCanvas.updateLayoutForWidth(viewportWidth);
        scoreViewport.setViewedComponent(&staffCanvas, false);
    }
    else
    {
        pianoRollCanvas.updateLayoutForViewport(viewportWidth);
        scoreViewport.setViewedComponent(&pianoRollCanvas, false);
    }
}

void ScorePanel::applySelectedNoteEditors()
{
    if (suppressEditorCallbacks || ! juce::isPositiveAndBelow(selectedNoteIndex, notes.size()))
        return;

    auto& note = notes.getReference(selectedNoteIndex);
    note.isRest = restToggleButton.getToggleState();
    note.midiNote = (int) pitchSlider.getValue();
    note.measure = (int) measureSlider.getValue();
    note.beat = (float) beatSlider.getValue();
    note.durationBeats = (float) durationSlider.getValue();
    note.lyric = note.isRest ? juce::String() : lyricEditor.getText().trim();
    refreshEditors();
    updateStatusText();
}

void ScorePanel::addNote()
{
    NoteEvent note;
    if (juce::isPositiveAndBelow(selectedNoteIndex, notes.size()))
    {
        auto selected = notes.getReference(selectedNoteIndex);
        note.measure = selected.measure;
        note.beat = juce::jlimit(1.0f, 4.75f, selected.beat + selected.durationBeats);
        note.durationBeats = selected.durationBeats;
        note.midiNote = selected.midiNote;
        note.isRest = selected.isRest;
    }

    notes.add(note);
    selectedNoteIndex = notes.size() - 1;
    refreshEditors();
    updateStatusText();
}

void ScorePanel::removeSelectedNote()
{
    if (! juce::isPositiveAndBelow(selectedNoteIndex, notes.size()))
        return;

    notes.remove(selectedNoteIndex);
    selectedNoteIndex = juce::jlimit(0, juce::jmax(0, notes.size() - 1), selectedNoteIndex - 1);
    refreshEditors();
    updateStatusText();
}

void ScorePanel::nudgeSelectedNotePitch(int semitones)
{
    if (! juce::isPositiveAndBelow(selectedNoteIndex, notes.size()))
        return;

    auto& note = notes.getReference(selectedNoteIndex);
    if (note.isRest)
        return;

    note.midiNote = juce::jlimit(36, 96, note.midiNote + semitones);
    refreshEditors();
    updateStatusText();
}

void ScorePanel::nudgeSelectedNoteBeat(float delta)
{
    if (! juce::isPositiveAndBelow(selectedNoteIndex, notes.size()))
        return;

    auto& note = notes.getReference(selectedNoteIndex);
    note.beat += delta;

    while (note.beat > 4.75f)
    {
        note.beat -= 4.0f;
        ++note.measure;
    }

    while (note.beat < 1.0f)
    {
        if (note.measure > 0)
        {
            note.beat += 4.0f;
            --note.measure;
        }
        else
        {
            note.beat = 1.0f;
            break;
        }
    }

    refreshEditors();
    updateStatusText();
}

void ScorePanel::updateStatusText()
{
    auto highestMeasure = 0;
    auto restCount = 0;
    for (const auto& note : notes)
    {
        highestMeasure = juce::jmax(highestMeasure, note.measure + 1);
        if (note.isRest)
            ++restCount;
    }

    statusLabel.setText(songTitle + "  |  " + keySignature + "  |  " + timeSignature + "  |  "
                            + juce::String(tempoBpm) + " BPM  |  "
                            + juce::String(notes.size()) + " event(s)  |  "
                            + juce::String(restCount) + " rest(s)  |  "
                            + juce::String(highestMeasure) + " measure(s)",
                        juce::dontSendNotification);
}

juce::String ScorePanel::midiName(int midiNote)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    auto octave = (midiNote / 12) - 1;
    auto index = juce::jlimit(0, 11, midiNote % 12);
    return juce::String(names[index]) + juce::String(octave);
}
