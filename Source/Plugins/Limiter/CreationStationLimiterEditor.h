#pragma once

#include <JuceHeader.h>
#include "CreationStationLimiterProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/LevelMeterComponent.h"

namespace cs::plugins
{
class CreationStationLimiterEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit CreationStationLimiterEditor(CreationStationLimiterProcessor& processorToEdit);
    ~CreationStationLimiterEditor() override;

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

    CreationStationLimiterProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Limiter" };

    LevelMeterComponent inputMeter;
    LevelMeterComponent gainReductionMeter;
    juce::Label inLabel, grLabel;

    KnobControl thresholdKnob, releaseKnob, makeupKnob, mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationLimiterEditor)
};
}
