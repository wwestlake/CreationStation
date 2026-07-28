#include "CreationStationDeEsserEditor.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{

CreationStationDeEsserEditor::CreationStationDeEsserEditor(CreationStationDeEsserProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), processorRef(processorToEdit)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(header);

    reductionLabel.setText("REDUCTION", juce::dontSendNotification);
    reductionLabel.setJustificationType(juce::Justification::centred);
    reductionLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    reductionLabel.setFont(juce::Font(11.0f).boldened());
    addAndMakeVisible(reductionLabel);

    reductionMeter.setLevelSource(&processorRef.getReductionValue(), 0.0f, 24.0f);
    addAndMakeVisible(reductionMeter);

    configureKnob(frequencyKnob, "frequency", "Frequency");
    configureKnob(thresholdKnob, "threshold", "Threshold");
    configureKnob(ratioKnob, "ratio", "Ratio");

    listenButton.setClickingTogglesState(true);
    addAndMakeVisible(listenButton);
    listenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.getValueTreeState(), "listen", listenButton);

    setSize(480, 360);
}

CreationStationDeEsserEditor::~CreationStationDeEsserEditor()
{
    setLookAndFeel(nullptr);
}

void CreationStationDeEsserEditor::configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText)
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

void CreationStationDeEsserEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void CreationStationDeEsserEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(56));
    area.reduce(16, 16);

    // Left: reduction meter with caption, and the Listen toggle beneath it.
    auto leftColumn = area.removeFromLeft(90);
    reductionLabel.setBounds(leftColumn.removeFromTop(16));
    leftColumn.removeFromTop(4);
    listenButton.setBounds(leftColumn.removeFromBottom(26));
    leftColumn.removeFromBottom(8);
    reductionMeter.setBounds(leftColumn);

    area.removeFromLeft(20);

    // Right: the three knobs in a row.
    auto knobWidth = area.getWidth() / 3;
    auto layoutKnob = [&](KnobControl& knob)
    {
        auto slot = area.removeFromLeft(knobWidth).reduced(6, 20);
        knob.label.setBounds(slot.removeFromTop(18));
        knob.slider.setBounds(slot);
    };

    layoutKnob(frequencyKnob);
    layoutKnob(thresholdKnob);
    layoutKnob(ratioKnob);
}

}
