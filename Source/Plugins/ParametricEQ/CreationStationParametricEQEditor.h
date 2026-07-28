#pragma once

#include <JuceHeader.h>
#include "CreationStationParametricEQProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/EqCurveComponent.h"
#include <array>

namespace cs::plugins
{
class CreationStationParametricEQEditor final : public juce::AudioProcessorEditor,
                                                private juce::Timer
{
public:
    explicit CreationStationParametricEQEditor(CreationStationParametricEQProcessor& processorToEdit);
    ~CreationStationParametricEQEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct BandStrip
    {
        juce::ComboBox typeCombo;
        juce::Slider freqSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider gainSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider qSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::TextButton bypassButton { "Bypass" };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    };

    void timerCallback() override;
    void refreshCurve();
    void refreshSpectrum();

    CreationStationParametricEQProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Parametric EQ" };

    EqCurveComponent curve;
    std::array<BandStrip, (size_t) CreationStationParametricEQProcessor::numBands> bandStrips;

    juce::TextButton masterBypassButton { "Bypass" };
    juce::Label outputTrimLabel;
    juce::Slider outputTrimSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> masterBypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputTrimAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationParametricEQEditor)
};
}
