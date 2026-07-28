#pragma once

#include <JuceHeader.h>
#include "CreationStationClipperProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/LevelMeterComponent.h"

namespace cs::plugins
{
class CreationStationClipperEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit CreationStationClipperEditor(CreationStationClipperProcessor& processorToEdit);
    ~CreationStationClipperEditor() override;

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
    void timerCallback() override;

    CreationStationClipperProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Clipper / Soft-Saturator" };

    LevelMeterComponent inputMeter, outputMeter, reductionMeter;
    juce::Label inLabel, outLabel, reductionLabel;
    juce::ComboBox typeCombo;
    juce::Label typeLabel;
    juce::Label hintLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;

    KnobControl driveKnob, ceilingKnob, softnessKnob, outputKnob, mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationClipperEditor)
};
}
