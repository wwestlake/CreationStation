#pragma once

#include <JuceHeader.h>
#include "CreationStationCompressorProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/LevelMeterComponent.h"

namespace cs::plugins
{
class CreationStationCompressorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CreationStationCompressorEditor(CreationStationCompressorProcessor& processorToEdit);
    ~CreationStationCompressorEditor() override;

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

    CreationStationCompressorProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Compressor" };
    LevelMeterComponent meter;

    KnobControl thresholdKnob, ratioKnob, attackKnob, releaseKnob, makeupKnob, mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationCompressorEditor)
};
}
