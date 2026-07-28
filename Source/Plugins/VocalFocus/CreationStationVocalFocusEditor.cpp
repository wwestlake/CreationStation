#include "CreationStationVocalFocusEditor.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{
CreationStationVocalFocusEditor::CreationStationVocalFocusEditor(CreationStationVocalFocusProcessor& processorToEdit)
    : juce::AudioProcessorEditor(&processorToEdit), processorRef(processorToEdit)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(header);

    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centred);
    modeLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    modeLabel.setFont(juce::Font(12.0f).boldened());
    addAndMakeVisible(modeLabel);

    modeCombo.addItemList({ "Reduce Center", "Isolate Center" }, 1);
    addAndMakeVisible(modeCombo);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getValueTreeState(), "mode", modeCombo);

    helpLabel.setText("Center-focused stereo math for vocal reduction or rough isolation. Best on stereo mixes with vocals near the middle.",
                      juce::dontSendNotification);
    helpLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    helpLabel.setJustificationType(juce::Justification::centredLeft);
    helpLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(helpLabel);

    configureKnob(amountKnob, "amount", "Amount");
    configureKnob(lowCutKnob, "lowCut", "Low Cut");
    configureKnob(highCutKnob, "highCut", "High Cut");
    configureKnob(bleedKnob, "bleed", "Side Bleed");
    configureKnob(mixKnob, "mix", "Mix");
    configureKnob(outputKnob, "output", "Output");

    setSize(700, 360);
}

CreationStationVocalFocusEditor::~CreationStationVocalFocusEditor()
{
    setLookAndFeel(nullptr);
}

void CreationStationVocalFocusEditor::configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText)
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

void CreationStationVocalFocusEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void CreationStationVocalFocusEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(56));
    area.reduce(16, 16);

    auto topRow = area.removeFromTop(56);
    modeLabel.setBounds(topRow.removeFromLeft(64));
    modeCombo.setBounds(topRow.removeFromLeft(170).withSizeKeepingCentre(170, 24));

    area.removeFromTop(4);
    helpLabel.setBounds(area.removeFromTop(36));
    area.removeFromTop(8);

    auto topKnobs = area.removeFromTop(area.getHeight() / 2);
    auto bottomKnobs = area;

    auto layoutThree = [](juce::Rectangle<int> row, KnobControl& a, KnobControl& b, KnobControl& c)
    {
        auto slotWidth = row.getWidth() / 3;
        auto layoutOne = [&](KnobControl& knob)
        {
            auto slot = row.removeFromLeft(slotWidth).reduced(8, 14);
            knob.label.setBounds(slot.removeFromTop(18));
            knob.slider.setBounds(slot);
        };

        layoutOne(a);
        layoutOne(b);
        layoutOne(c);
    };

    layoutThree(topKnobs, amountKnob, lowCutKnob, highCutKnob);
    layoutThree(bottomKnobs, bleedKnob, mixKnob, outputKnob);
}
}
