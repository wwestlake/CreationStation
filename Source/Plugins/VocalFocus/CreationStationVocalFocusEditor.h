#pragma once

#include <JuceHeader.h>
#include "CreationStationVocalFocusProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"

namespace cs::plugins
{
class CreationStationVocalFocusEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CreationStationVocalFocusEditor(CreationStationVocalFocusProcessor& processorToEdit);
    ~CreationStationVocalFocusEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct KnobControl
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText);

    CreationStationVocalFocusProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Vocal Focus" };

    juce::Label modeLabel;
    juce::ComboBox modeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

    juce::Label helpLabel;

    KnobControl amountKnob;
    KnobControl lowCutKnob;
    KnobControl highCutKnob;
    KnobControl bleedKnob;
    KnobControl mixKnob;
    KnobControl outputKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationVocalFocusEditor)
};
}
