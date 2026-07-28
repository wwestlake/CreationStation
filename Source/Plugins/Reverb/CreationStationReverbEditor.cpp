#include "CreationStationReverbEditor.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{

CreationStationReverbEditor::CreationStationReverbEditor(CreationStationReverbProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), processorRef(processorToEdit)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(header);

    inLabel.setText("IN", juce::dontSendNotification);
    inLabel.setJustificationType(juce::Justification::centred);
    inLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    inLabel.setFont(juce::Font(11.0f).boldened());
    addAndMakeVisible(inLabel);

    inputMeter.setLevelSource(&processorRef.getInputLevelValue(), -60.0f, 6.0f);
    addAndMakeVisible(inputMeter);

    configureKnob(sizeKnob, "size", "Size");
    configureKnob(dampingKnob, "damping", "Damping");
    configureKnob(widthKnob, "width", "Width");
    configureKnob(mixKnob, "mix", "Mix");

    freezeButton.setClickingTogglesState(true);
    addAndMakeVisible(freezeButton);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.getValueTreeState(), "freeze", freezeButton);

    setSize(560, 360);
}

CreationStationReverbEditor::~CreationStationReverbEditor()
{
    setLookAndFeel(nullptr);
}

void CreationStationReverbEditor::configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText)
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

void CreationStationReverbEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void CreationStationReverbEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(56));
    area.reduce(16, 16);

    // Left: input level meter with caption, and the Freeze toggle beneath it.
    auto leftColumn = area.removeFromLeft(90);
    inLabel.setBounds(leftColumn.removeFromTop(16));
    leftColumn.removeFromTop(4);
    freezeButton.setBounds(leftColumn.removeFromBottom(26));
    leftColumn.removeFromBottom(8);
    inputMeter.setBounds(leftColumn);

    area.removeFromLeft(20);

    // Right: the four knobs in a row.
    auto knobWidth = area.getWidth() / 4;
    auto layoutKnob = [&](KnobControl& knob)
    {
        auto slot = area.removeFromLeft(knobWidth).reduced(6, 20);
        knob.label.setBounds(slot.removeFromTop(18));
        knob.slider.setBounds(slot);
    };

    layoutKnob(sizeKnob);
    layoutKnob(dampingKnob);
    layoutKnob(widthKnob);
    layoutKnob(mixKnob);
}

}
