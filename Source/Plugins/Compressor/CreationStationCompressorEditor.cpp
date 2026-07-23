#include "CreationStationCompressorEditor.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{

CreationStationCompressorEditor::CreationStationCompressorEditor(CreationStationCompressorProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), processorRef(processorToEdit)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(header);
    addAndMakeVisible(meter);
    meter.setLevelSource(&processorRef.getGainReductionValue(), 0.0f, 24.0f);

    configureKnob(thresholdKnob, "threshold", "Threshold");
    configureKnob(ratioKnob, "ratio", "Ratio");
    configureKnob(attackKnob, "attack", "Attack");
    configureKnob(releaseKnob, "release", "Release");
    configureKnob(makeupKnob, "makeup", "Makeup");
    configureKnob(mixKnob, "mix", "Mix");

    setSize(560, 320);
}

CreationStationCompressorEditor::~CreationStationCompressorEditor()
{
    setLookAndFeel(nullptr);
}

void CreationStationCompressorEditor::configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText)
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

void CreationStationCompressorEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void CreationStationCompressorEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(56));
    area.reduce(16, 16);

    auto meterArea = area.removeFromRight(48);
    meter.setBounds(meterArea.reduced(4, 0));
    area.removeFromRight(16);

    auto knobWidth = area.getWidth() / 6;
    auto layoutKnob = [&](KnobControl& knob)
    {
        auto slot = area.removeFromLeft(knobWidth).reduced(6, 0);
        knob.label.setBounds(slot.removeFromTop(18));
        knob.slider.setBounds(slot);
    };

    layoutKnob(thresholdKnob);
    layoutKnob(ratioKnob);
    layoutKnob(attackKnob);
    layoutKnob(releaseKnob);
    layoutKnob(makeupKnob);
    layoutKnob(mixKnob);
}

}
