#pragma once

#include <JuceHeader.h>
#include "CreationStationResonanceSuppressorProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/EqCurveComponent.h"
#include <array>

namespace cs::plugins
{
class CreationStationResonanceSuppressorEditor final : public juce::AudioProcessorEditor,
                                                       private juce::Timer
{
public:
    explicit CreationStationResonanceSuppressorEditor(CreationStationResonanceSuppressorProcessor& processorToEdit);
    ~CreationStationResonanceSuppressorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct BandStrip
    {
        juce::Label title;
        juce::Slider freqSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider depthSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider thresholdSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider qSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::TextButton bypassButton { "Bypass" };
        juce::Label reductionLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttachment;
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

    CreationStationResonanceSuppressorProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Resonance Suppressor" };

    EqCurveComponent curve;
    std::array<BandStrip, (size_t) CreationStationResonanceSuppressorProcessor::numBands> bandStrips;
    GlobalKnob attackKnob;
    GlobalKnob releaseKnob;
    GlobalKnob mixKnob;
    GlobalKnob outputKnob;
    juce::TextButton masterBypassButton { "Bypass" };
    juce::Label hintLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> masterBypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationResonanceSuppressorEditor)
};
}
