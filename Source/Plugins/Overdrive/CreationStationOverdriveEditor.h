#pragma once

#include <JuceHeader.h>
#include "CreationStationOverdriveProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/LevelMeterComponent.h"

namespace cs::plugins
{
class CreationStationOverdriveEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CreationStationOverdriveEditor(CreationStationOverdriveProcessor& processorToEdit);
    ~CreationStationOverdriveEditor() override;

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

    CreationStationOverdriveProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Overdrive / Saturation" };

    LevelMeterComponent inputMeter, outputMeter;
    juce::Label inLabel, outLabel;

    juce::ComboBox typeCombo;
    juce::Label typeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;

    KnobControl driveKnob, toneKnob, outputKnob, mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationOverdriveEditor)
};
}
