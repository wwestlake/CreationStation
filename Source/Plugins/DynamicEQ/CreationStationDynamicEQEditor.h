#pragma once

#include <JuceHeader.h>
#include "CreationStationDynamicEQProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/EqCurveComponent.h"
#include <array>

namespace cs::plugins
{
class CreationStationDynamicEQEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit CreationStationDynamicEQEditor(CreationStationDynamicEQProcessor& processorToEdit);
    ~CreationStationDynamicEQEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct BandStrip
    {
        juce::Label title;
        juce::ComboBox typeCombo;
        juce::ComboBox modeCombo;
        juce::Slider freqSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider gainSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider rangeSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider thresholdSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider qSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::TextButton bypassButton { "Bypass" };
        juce::Label dynamicGainLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rangeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    };

    struct GlobalKnob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void refreshCurve();
    void refreshSpectrum();
    void configureBandKnob(juce::Slider& slider,
                           std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                           const juce::String& parameterId);
    void configureGlobalKnob(GlobalKnob& knob, const juce::String& parameterId, const juce::String& labelText);

    CreationStationDynamicEQProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Dynamic EQ" };

    EqCurveComponent curve;
    std::array<BandStrip, (size_t) CreationStationDynamicEQProcessor::numBands> bandStrips;
    GlobalKnob attackKnob;
    GlobalKnob releaseKnob;
    GlobalKnob mixKnob;
    GlobalKnob outputKnob;
    juce::TextButton masterBypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> masterBypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationDynamicEQEditor)
};
}
