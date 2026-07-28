#include "CreationStationCompressorEditor.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{

CreationStationCompressorEditor::CreationStationCompressorEditor(CreationStationCompressorProcessor& processorToEdit)
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
    configureMeterLabel(inLabel, "IN / THRESH");
    configureMeterLabel(grLabel, "GR");

    inputMeter.setLevelSource(&processorRef.getInputLevelValue());
    inputMeter.setThresholdDb(processorRef.getValueTreeState().getRawParameterValue("threshold")->load());
    inputMeter.onThresholdChanged = [this](float db)
    {
        if (auto* p = processorRef.getValueTreeState().getParameter("threshold"))
            p->setValueNotifyingHost(p->convertTo0to1(db));
    };
    addAndMakeVisible(inputMeter);

    gainReductionMeter.setLevelSource(&processorRef.getGainReductionValue(), 0.0f, 24.0f);
    addAndMakeVisible(gainReductionMeter);

    configureKnob(ratioKnob, "ratio", "Ratio");
    configureKnob(attackKnob, "attack", "Attack");
    configureKnob(releaseKnob, "release", "Release");
    configureKnob(makeupKnob, "makeup", "Makeup");
    configureKnob(mixKnob, "mix", "Mix");

    setSize(640, 360);
    startTimerHz(30);
}

CreationStationCompressorEditor::~CreationStationCompressorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void CreationStationCompressorEditor::timerCallback()
{
    // Keep the threshold line in sync with the parameter (automation / preset load / host edits).
    inputMeter.setThresholdDb(processorRef.getValueTreeState().getRawParameterValue("threshold")->load());
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

    // Left: input/threshold meter + gain-reduction meter, each with a caption.
    auto metersArea = area.removeFromLeft(150);
    auto inColumn = metersArea.removeFromLeft(96);
    metersArea.removeFromLeft(10);
    auto grColumn = metersArea;

    inLabel.setBounds(inColumn.removeFromTop(16));
    inColumn.removeFromTop(4);
    inputMeter.setBounds(inColumn);

    grLabel.setBounds(grColumn.removeFromTop(16));
    grColumn.removeFromTop(4);
    gainReductionMeter.setBounds(grColumn.reduced(0, 0));

    area.removeFromLeft(20);

    // Right: the five remaining controls in a row.
    auto knobWidth = area.getWidth() / 5;
    auto layoutKnob = [&](KnobControl& knob)
    {
        auto slot = area.removeFromLeft(knobWidth).reduced(6, 20);
        knob.label.setBounds(slot.removeFromTop(18));
        knob.slider.setBounds(slot);
    };

    layoutKnob(ratioKnob);
    layoutKnob(attackKnob);
    layoutKnob(releaseKnob);
    layoutKnob(makeupKnob);
    layoutKnob(mixKnob);
}

}
