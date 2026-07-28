#include "CreationStationPhaserEditor.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{

CreationStationPhaserEditor::CreationStationPhaserEditor(CreationStationPhaserProcessor& processorToEdit)
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

    configureKnob(rateKnob, "rate", "Rate");
    configureKnob(depthKnob, "depth", "Depth");
    configureKnob(centerKnob, "centreFrequency", "Center");
    configureKnob(feedbackKnob, "feedback", "Feedback");
    configureKnob(mixKnob, "mix", "Mix");

    setSize(640, 360);
}

CreationStationPhaserEditor::~CreationStationPhaserEditor()
{
    setLookAndFeel(nullptr);
}

void CreationStationPhaserEditor::configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText)
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

void CreationStationPhaserEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void CreationStationPhaserEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(56));
    area.reduce(16, 16);

    auto leftColumn = area.removeFromLeft(90);
    inLabel.setBounds(leftColumn.removeFromTop(16));
    leftColumn.removeFromTop(4);
    inputMeter.setBounds(leftColumn);

    area.removeFromLeft(20);

    auto knobWidth = area.getWidth() / 5;
    auto layoutKnob = [&](KnobControl& knob)
    {
        auto slot = area.removeFromLeft(knobWidth).reduced(6, 20);
        knob.label.setBounds(slot.removeFromTop(18));
        knob.slider.setBounds(slot);
    };

    layoutKnob(rateKnob);
    layoutKnob(depthKnob);
    layoutKnob(centerKnob);
    layoutKnob(feedbackKnob);
    layoutKnob(mixKnob);
}

}
