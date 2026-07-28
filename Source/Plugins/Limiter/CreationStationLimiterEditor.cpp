#include "CreationStationLimiterEditor.h"

namespace cs::plugins
{
CreationStationLimiterEditor::CreationStationLimiterEditor(CreationStationLimiterProcessor& processorToEdit)
    : juce::AudioProcessorEditor(&processorToEdit), processorRef(processorToEdit)
{
    setLookAndFeel(&lookAndFeel);
    setSize(400, 250);

    addAndMakeVisible(header);

    addAndMakeVisible(inputMeter);
    inputMeter.setLevelSource(&processorRef.inputLevelDb, -60.0f, 0.0f);
    addAndMakeVisible(inLabel);
    inLabel.setText("Input", juce::dontSendNotification);
    inLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(gainReductionMeter);
    gainReductionMeter.setLevelSource(&processorRef.gainReductionDb, 0.0f, 24.0f);
    addAndMakeVisible(grLabel);
    grLabel.setText("GR", juce::dontSendNotification);
    grLabel.setJustificationType(juce::Justification::centred);

    configureKnob(thresholdKnob, "Threshold", "Threshold");
    configureKnob(releaseKnob, "Release", "Release");
    configureKnob(makeupKnob, "MakeupGain", "Makeup");
    configureKnob(mixKnob, "Mix", "Mix");

    startTimer(50);
}

CreationStationLimiterEditor::~CreationStationLimiterEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void CreationStationLimiterEditor::configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText)
{
    addAndMakeVisible(knob.slider);
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    knob.slider.setNumDecimalPlacesToDisplay(1);

    addAndMakeVisible(knob.label);
    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.attachToComponent(&knob.slider, false);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, parameterId, knob.slider);
}

void CreationStationLimiterEditor::paint(juce::Graphics& g)
{
    g.fillAll(lookAndFeel.findColour(juce::ResizableWindow::backgroundColourId));
}

void CreationStationLimiterEditor::resized()
{
    auto bounds = getLocalBounds();

    header.setBounds(bounds.removeFromTop(40));

    auto meterArea = bounds.removeFromLeft(60);
    inLabel.setBounds(meterArea.removeFromTop(20));
    inputMeter.setBounds(meterArea.removeFromTop(100));

    auto grMeterArea = bounds.removeFromLeft(60);
    grLabel.setBounds(grMeterArea.removeFromTop(20));
    gainReductionMeter.setBounds(grMeterArea.removeFromTop(100));

    auto knobWidth = 80;
    auto spacing = 10;

    thresholdKnob.slider.setBounds(bounds.removeFromLeft(knobWidth + spacing));
    releaseKnob.slider.setBounds(bounds.removeFromLeft(knobWidth + spacing));
    makeupKnob.slider.setBounds(bounds.removeFromLeft(knobWidth + spacing));
    mixKnob.slider.setBounds(bounds.removeFromLeft(knobWidth + spacing));
}

void CreationStationLimiterEditor::timerCallback()
{
    inputMeter.repaint();
    gainReductionMeter.repaint();
}
}
