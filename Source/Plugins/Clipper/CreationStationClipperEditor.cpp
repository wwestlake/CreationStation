#include "CreationStationClipperEditor.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{
CreationStationClipperEditor::CreationStationClipperEditor(CreationStationClipperProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), processorRef(processorToEdit)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(header);

    auto configureMeterLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, palette::textSecondary);
        label.setFont(juce::Font(11.0f).boldened());
        addAndMakeVisible(label);
    };

    configureMeterLabel(inLabel, "IN");
    configureMeterLabel(outLabel, "OUT");
    configureMeterLabel(reductionLabel, "CLIP");

    inputMeter.setLevelSource(&processorRef.getInputLevelValue(), -60.0f, 6.0f);
    outputMeter.setLevelSource(&processorRef.getOutputLevelValue(), -60.0f, 6.0f);
    reductionMeter.setLevelSource(&processorRef.getClipReductionValue(), 0.0f, 18.0f);
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(reductionMeter);

    typeLabel.setText("Curve", juce::dontSendNotification);
    typeLabel.setJustificationType(juce::Justification::centred);
    typeLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    typeLabel.setFont(juce::Font(12.0f).boldened());
    addAndMakeVisible(typeLabel);

    typeCombo.addItemList({ "Soft", "Hard", "Asym" }, 1);
    addAndMakeVisible(typeCombo);
    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getValueTreeState(), "type", typeCombo);

    hintLabel.setText("Use soft for glue, hard for peak chopping, and asym for extra bite and color.",
                      juce::dontSendNotification);
    hintLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hintLabel);

    configureKnob(driveKnob, "drive", "Drive");
    configureKnob(ceilingKnob, "ceiling", "Ceiling");
    configureKnob(softnessKnob, "softness", "Softness");
    configureKnob(outputKnob, "output", "Output");
    configureKnob(mixKnob, "mix", "Mix");

    setSize(760, 380);
    startTimerHz(20);
}

CreationStationClipperEditor::~CreationStationClipperEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void CreationStationClipperEditor::configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText)
{
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
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

void CreationStationClipperEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void CreationStationClipperEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(56));
    area.reduce(16, 16);

    auto meterArea = area.removeFromLeft(190);
    auto inColumn = meterArea.removeFromLeft(55);
    meterArea.removeFromLeft(12);
    auto outColumn = meterArea.removeFromLeft(55);
    meterArea.removeFromLeft(12);
    auto clipColumn = meterArea;

    inLabel.setBounds(inColumn.removeFromTop(16));
    inColumn.removeFromTop(4);
    inputMeter.setBounds(inColumn);

    outLabel.setBounds(outColumn.removeFromTop(16));
    outColumn.removeFromTop(4);
    outputMeter.setBounds(outColumn);

    reductionLabel.setBounds(clipColumn.removeFromTop(16));
    clipColumn.removeFromTop(4);
    reductionMeter.setBounds(clipColumn);

    area.removeFromLeft(20);

    auto topRow = area.removeFromTop(48);
    typeLabel.setBounds(topRow.removeFromLeft(60));
    typeCombo.setBounds(topRow.removeFromLeft(140).withSizeKeepingCentre(140, 24));

    hintLabel.setBounds(area.removeFromTop(26));
    area.removeFromTop(8);

    auto knobWidth = area.getWidth() / 5;
    auto layoutKnob = [&](KnobControl& knob)
    {
        auto slot = area.removeFromLeft(knobWidth).reduced(6, 12);
        knob.label.setBounds(slot.removeFromTop(18));
        knob.slider.setBounds(slot);
    };

    layoutKnob(driveKnob);
    layoutKnob(ceilingKnob);
    layoutKnob(softnessKnob);
    layoutKnob(outputKnob);
    layoutKnob(mixKnob);
}

void CreationStationClipperEditor::timerCallback()
{
    inputMeter.repaint();
    outputMeter.repaint();
    reductionMeter.repaint();
}
}
