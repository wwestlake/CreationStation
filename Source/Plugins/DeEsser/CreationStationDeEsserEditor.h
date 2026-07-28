#pragma once

#include <JuceHeader.h>
#include "CreationStationDeEsserProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/LevelMeterComponent.h"

namespace cs::plugins
{
class CreationStationDeEsserEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CreationStationDeEsserEditor(CreationStationDeEsserProcessor& processorToEdit);
    ~CreationStationDeEsserEditor() override;

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

    CreationStationDeEsserProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "De-Esser" };

    LevelMeterComponent reductionMeter;
    juce::Label reductionLabel;

    KnobControl frequencyKnob, thresholdKnob, ratioKnob;

    juce::TextButton listenButton { "Listen" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> listenAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationDeEsserEditor)
};
}
