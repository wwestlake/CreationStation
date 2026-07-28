#include "CreationStationTuneEditor.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{
void CreationStationTuneEditor::PitchDisplay::setHistory(const std::array<float, CreationStationTuneProcessor::displayHistorySize>& values, int count)
{
    history = values;
    historyCount = count;
    repaint();
}

void CreationStationTuneEditor::PitchDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(palette::panelBackground);
    g.fillRoundedRectangle(bounds, 14.0f);
    g.setColour(palette::outline);
    g.drawRoundedRectangle(bounds, 14.0f, 1.0f);

    auto area = getLocalBounds().reduced(14);
    auto top = area.removeFromTop(56);

    g.setColour(palette::textPrimary);
    g.setFont(juce::Font(28.0f).boldened());
    g.drawText(detectedNote, top.removeFromLeft(120), juce::Justification::centredLeft, false);

    g.setFont(juce::Font(13.0f).boldened());
    g.setColour(palette::textSecondary);
    g.drawText("Target " + targetNote, top.removeFromLeft(130), juce::Justification::centredLeft, false);
    g.drawText("Detect " + juce::String((int) std::round(detectedCents)) + " ct",
               top.removeFromLeft(120), juce::Justification::centredLeft, false);
    g.drawText("Correct " + juce::String((int) std::round(appliedCents)) + " ct",
               top.removeFromLeft(130), juce::Justification::centredLeft, false);
    g.drawText("Confidence " + juce::String((int) std::round(confidence * 100.0f)) + "%",
               top, juce::Justification::centredLeft, false);

    auto graph = area.reduced(0, 8);
    g.setColour(juce::Colour(0x1836bffa));
    for (int cents : { -100, -50, 0, 50, 100 })
    {
        auto y = juce::jmap((float) cents, -100.0f, 100.0f, (float) graph.getBottom(), (float) graph.getY());
        g.drawHorizontalLine((int) y, (float) graph.getX(), (float) graph.getRight());
    }

    auto zeroY = juce::jmap(0.0f, -100.0f, 100.0f, (float) graph.getBottom(), (float) graph.getY());
    g.setColour(palette::accentDim);
    g.drawHorizontalLine((int) zeroY, (float) graph.getX(), (float) graph.getRight());

    if (historyCount <= 1)
        return;

    juce::Path path;
    for (int index = 0; index < historyCount; ++index)
    {
        auto x = juce::jmap((float) index, 0.0f, (float) juce::jmax(1, historyCount - 1),
                            (float) graph.getX(), (float) graph.getRight());
        auto y = juce::jmap(history[(size_t) index], -100.0f, 100.0f,
                            (float) graph.getBottom(), (float) graph.getY());
        if (index == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    g.setColour(palette::accent);
    g.strokePath(path, juce::PathStrokeType(2.0f, juce::PathStrokeType::JointStyle::curved));
}

CreationStationTuneEditor::CreationStationTuneEditor(CreationStationTuneProcessor& processorToEdit)
    : juce::AudioProcessorEditor(&processorToEdit), processorRef(processorToEdit)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(header);

    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centred);
    modeLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    modeLabel.setFont(juce::Font(12.0f).boldened());
    addAndMakeVisible(modeLabel);

    modeCombo.addItemList({ "Tuner", "Auto", "Manual" }, 1);
    addAndMakeVisible(modeCombo);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getValueTreeState(), "mode", modeCombo);

    addAndMakeVisible(pitchDisplay);

    keyLabel.setText("Key", juce::dontSendNotification);
    keyLabel.setJustificationType(juce::Justification::centred);
    keyLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    keyLabel.setFont(juce::Font(12.0f).boldened());
    addAndMakeVisible(keyLabel);

    keyCombo.addItemList({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    addAndMakeVisible(keyCombo);
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getValueTreeState(), "key", keyCombo);

    scaleLabel.setText("Scale", juce::dontSendNotification);
    scaleLabel.setJustificationType(juce::Justification::centred);
    scaleLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    scaleLabel.setFont(juce::Font(12.0f).boldened());
    addAndMakeVisible(scaleLabel);

    scaleCombo.addItemList({ "Chromatic", "Major", "Minor", "Dorian", "Phrygian", "Lydian",
                             "Mixolydian", "Locrian", "Arabian", "Egyptian" }, 1);
    addAndMakeVisible(scaleCombo);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getValueTreeState(), "scale", scaleCombo);

    configureKnob(referenceKnob, "reference", "Reference");
    configureKnob(speedKnob, "speed", "Speed");
    configureKnob(strengthKnob, "strength", "Strength");
    configureKnob(mixKnob, "mix", "Mix");

    octaveProtectButton.setClickingTogglesState(true);
    addAndMakeVisible(octaveProtectButton);
    octaveProtectAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.getValueTreeState(), "octaveProtect", octaveProtectButton);

    notesLabel.setText("Allowed Notes", juce::dontSendNotification);
    notesLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    notesLabel.setFont(juce::Font(12.0f).boldened());
    addAndMakeVisible(notesLabel);

    for (int noteIndex = 0; noteIndex < 12; ++noteIndex)
    {
        noteButtons[(size_t) noteIndex].setClickingTogglesState(true);
        addAndMakeVisible(noteButtons[(size_t) noteIndex]);
        noteAttachments[(size_t) noteIndex] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processorRef.getValueTreeState(), "allowedNote" + juce::String(noteIndex), noteButtons[(size_t) noteIndex]);
    }

    helpLabel.setText("Manual mode is the next layer: note-segment editing and clip-aware correction will live here. For now, use Tuner or Auto for real-time work.",
                      juce::dontSendNotification);
    helpLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    helpLabel.setJustificationType(juce::Justification::topLeft);
    helpLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(helpLabel);

    setSize(900, 620);
    startTimerHz(24);
    updateManualVisibility();
}

CreationStationTuneEditor::~CreationStationTuneEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void CreationStationTuneEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void CreationStationTuneEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(56));
    area.reduce(16, 16);

    auto topRow = area.removeFromTop(34);
    modeLabel.setBounds(topRow.removeFromLeft(50));
    modeCombo.setBounds(topRow.removeFromLeft(130).withSizeKeepingCentre(130, 24));
    topRow.removeFromLeft(20);
    keyLabel.setBounds(topRow.removeFromLeft(40));
    keyCombo.setBounds(topRow.removeFromLeft(90).withSizeKeepingCentre(90, 24));
    topRow.removeFromLeft(12);
    scaleLabel.setBounds(topRow.removeFromLeft(50));
    scaleCombo.setBounds(topRow.removeFromLeft(150).withSizeKeepingCentre(150, 24));
    octaveProtectButton.setBounds(topRow.removeFromLeft(150).withSizeKeepingCentre(140, 24));

    area.removeFromTop(12);
    pitchDisplay.setBounds(area.removeFromTop(200));
    area.removeFromTop(12);

    auto controls = area.removeFromTop(170);
    auto slotWidth = controls.getWidth() / 4;
    auto layoutKnob = [&](KnobControl& knob)
    {
        auto slot = controls.removeFromLeft(slotWidth).reduced(10, 12);
        knob.label.setBounds(slot.removeFromTop(20));
        knob.slider.setBounds(slot);
    };

    layoutKnob(referenceKnob);
    layoutKnob(speedKnob);
    layoutKnob(strengthKnob);
    layoutKnob(mixKnob);

    area.removeFromTop(4);
    notesLabel.setBounds(area.removeFromTop(20));
    auto notesArea = area.removeFromTop(78);
    auto noteWidth = notesArea.getWidth() / 6;
    for (int row = 0; row < 2; ++row)
    {
        auto rowArea = notesArea.removeFromTop(32);
        for (int column = 0; column < 6; ++column)
        {
            auto index = row * 6 + column;
            noteButtons[(size_t) index].setBounds(rowArea.removeFromLeft(noteWidth).reduced(4, 2));
        }
        notesArea.removeFromTop(6);
    }

    area.removeFromTop(8);
    helpLabel.setBounds(area);
}

void CreationStationTuneEditor::timerCallback()
{
    pitchDisplay.setDetectedNote(CreationStationTuneProcessor::noteNameForMidi(processorRef.getDetectedMidi()));
    pitchDisplay.setTargetNote(CreationStationTuneProcessor::noteNameForMidi(processorRef.getTargetMidi()));
    pitchDisplay.setDetectedCents(processorRef.getDetectedCents());
    pitchDisplay.setAppliedCents(processorRef.getAppliedCorrectionCents());
    pitchDisplay.setConfidence(processorRef.getDetectionConfidence());

    std::array<float, CreationStationTuneProcessor::displayHistorySize> history {};
    int historyCount = 0;
    processorRef.copyPitchHistory(history, historyCount);
    pitchDisplay.setHistory(history, historyCount);
    updateManualVisibility();
}

void CreationStationTuneEditor::configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText)
{
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 74, 18);
    knob.slider.setColour(juce::Slider::textBoxTextColourId, palette::textPrimary);
    addAndMakeVisible(knob.slider);

    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, palette::textSecondary);
    knob.label.setFont(juce::Font(12.0f).boldened());
    addAndMakeVisible(knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getValueTreeState(), parameterId, knob.slider);
}

void CreationStationTuneEditor::updateManualVisibility()
{
    auto isManual = processorRef.getCurrentModeIndex() == (int) CreationStationTuneProcessor::Mode::manual;

    keyLabel.setVisible(! isManual);
    keyCombo.setVisible(! isManual);
    scaleLabel.setVisible(! isManual);
    scaleCombo.setVisible(! isManual);
    referenceKnob.label.setVisible(! isManual);
    referenceKnob.slider.setVisible(! isManual);
    speedKnob.label.setVisible(! isManual);
    speedKnob.slider.setVisible(! isManual);
    strengthKnob.label.setVisible(! isManual);
    strengthKnob.slider.setVisible(! isManual);
    mixKnob.label.setVisible(! isManual);
    mixKnob.slider.setVisible(! isManual);
    octaveProtectButton.setVisible(! isManual);
    notesLabel.setVisible(! isManual);
    for (auto& button : noteButtons)
        button.setVisible(! isManual);

    helpLabel.setVisible(isManual);
}
}
