#pragma once

#include <JuceHeader.h>
#include "CreationStationChorusProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/LevelMeterComponent.h"

namespace cs::plugins
{
class CreationStationChorusEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CreationStationChorusEditor(CreationStationChorusProcessor& processorToEdit);
    ~CreationStationChorusEditor() override;

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

    CreationStationChorusProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Chorus" };

    LevelMeterComponent inputMeter;
    juce::Label inLabel;

    KnobControl rateKnob, depthKnob, delayKnob, feedbackKnob, mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationChorusEditor)
};
}
