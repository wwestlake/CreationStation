#pragma once

#include <JuceHeader.h>
#include "CreationStationDelayProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/LevelMeterComponent.h"

namespace cs::plugins
{
class CreationStationDelayEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CreationStationDelayEditor(CreationStationDelayProcessor& processorToEdit);
    ~CreationStationDelayEditor() override;

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

    CreationStationDelayProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Delay" };

    LevelMeterComponent inputMeter;
    juce::Label inLabel;

    KnobControl timeKnob, feedbackKnob, mixKnob, toneKnob;

    juce::TextButton pingPongButton { "Ping-Pong" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pingPongAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationDelayEditor)
};
}
