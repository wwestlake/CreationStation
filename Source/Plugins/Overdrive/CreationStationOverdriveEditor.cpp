#include "CreationStationOverdriveEditor.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{

CreationStationOverdriveEditor::CreationStationOverdriveEditor(CreationStationOverdriveProcessor& processorToEdit)
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

    inputMeter.setLevelSource(&processorRef.getInputLevelValue(), -60.0f, 6.0f);
    addAndMakeVisible(inputMeter);

    outputMeter.setLevelSource(&processorRef.getOutputLevelValue(), -60.0f, 6.0f);
    addAndMakeVisible(outputMeter);

    typeLabel.setText("Type", juce::dontSendNotification);
    typeLabel.setJustificationType(juce::Justification::centred);
    typeLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    typeLabel.setFont(juce::Font(12.0f).boldened());
    addAndMakeVisible(typeLabel);

    typeCombo.addItemList({ "Soft", "Hard", "Tube" }, 1);
    addAndMakeVisible(typeCombo);
    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getValueTreeState(), "type", typeCombo);

    configureKnob(driveKnob, "drive", "Drive");
    configureKnob(toneKnob, "tone", "Tone");
    configureKnob(outputKnob, "output", "Output");
    configureKnob(mixKnob, "mix", "Mix");

    setSize(560, 360);
}

CreationStationOverdriveEditor::~CreationStationOverdriveEditor()
{
    setLookAndFeel(nullptr);
}

void CreationStationOverdriveEditor::configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText)
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

void CreationStationOverdriveEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void CreationStationOverdriveEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(56));
    area.reduce(16, 16);

    // Left: input/output level meters, each with a caption.
    auto metersArea = area.removeFromLeft(150);
    auto inColumn = metersArea.removeFromLeft(70);
    metersArea.removeFromLeft(10);
    auto outColumn = metersArea;

    inLabel.setBounds(inColumn.removeFromTop(16));
    inColumn.removeFromTop(4);
    inputMeter.setBounds(inColumn);

    outLabel.setBounds(outColumn.removeFromTop(16));
    outColumn.removeFromTop(4);
    outputMeter.setBounds(outColumn);

    area.removeFromLeft(20);

    // Right: Type combo above, then the four knobs in a row.
    auto typeArea = area.removeFromTop(48);
    typeLabel.setBounds(typeArea.removeFromLeft(60));
    typeCombo.setBounds(typeArea.removeFromLeft(140).withSizeKeepingCentre(140, 24));

    area.removeFromTop(10);

    auto knobWidth = area.getWidth() / 4;
    auto layoutKnob = [&](KnobControl& knob)
    {
        auto slot = area.removeFromLeft(knobWidth).reduced(6, 20);
        knob.label.setBounds(slot.removeFromTop(18));
        knob.slider.setBounds(slot);
    };

    layoutKnob(driveKnob);
    layoutKnob(toneKnob);
    layoutKnob(outputKnob);
    layoutKnob(mixKnob);
}

}
