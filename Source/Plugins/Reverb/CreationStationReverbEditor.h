#pragma once

#include <JuceHeader.h>
#include "CreationStationReverbProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/LevelMeterComponent.h"

namespace cs::plugins
{
class CreationStationReverbEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CreationStationReverbEditor(CreationStationReverbProcessor& processorToEdit);
    ~CreationStationReverbEditor() override;

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

    CreationStationReverbProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Reverb" };

    LevelMeterComponent inputMeter;
    juce::Label inLabel;

    KnobControl sizeKnob, dampingKnob, widthKnob, mixKnob;

    juce::TextButton freezeButton { "Freeze" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationReverbEditor)
};
}
