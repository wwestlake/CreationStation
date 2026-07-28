#pragma once

#include <JuceHeader.h>
#include "CreationStationPhaserProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/LevelMeterComponent.h"

namespace cs::plugins
{
class CreationStationPhaserEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CreationStationPhaserEditor(CreationStationPhaserProcessor& processorToEdit);
    ~CreationStationPhaserEditor() override;

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

    CreationStationPhaserProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Phaser" };

    LevelMeterComponent inputMeter;
    juce::Label inLabel;

    KnobControl rateKnob, depthKnob, centerKnob, feedbackKnob, mixKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationPhaserEditor)
};
}
