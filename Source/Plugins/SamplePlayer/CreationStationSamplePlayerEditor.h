#pragma once

#include <JuceHeader.h>
#include "CreationStationSamplePlayerProcessor.h"
#include "../../PluginKit/CreationStationPluginLookAndFeel.h"
#include "../../PluginKit/CreationStationPluginHeader.h"
#include "../../PluginKit/LevelMeterComponent.h"
#include "../../Audio/SamplePackCatalog.h"

namespace cs::plugins
{
class CreationStationSamplePlayerEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CreationStationSamplePlayerEditor(CreationStationSamplePlayerProcessor& processorToEdit);
    ~CreationStationSamplePlayerEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct LayerRow
    {
        juce::ComboBox packCombo;
        juce::Slider gainSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    };

    struct KnobControl
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void chooseLibraryFolder();
    void refreshCatalogAndCombos();
    void populateLayerCombo(int layerIndex);
    void configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText);

    CreationStationSamplePlayerProcessor& processorRef;
    CreationStationPluginLookAndFeel lookAndFeel;
    CreationStationPluginHeader header { "Sample Player" };

    juce::TextButton libraryFolderButton { "Library Folder..." };
    juce::Label libraryPathLabel;

    std::array<LayerRow, CreationStationSamplePlayerProcessor::numLayers> layerRows;
    KnobControl attackKnob, decayKnob, sustainKnob, releaseKnob;
    LevelMeterComponent outputMeter;

    SamplePackCatalog catalog;
    std::unique_ptr<juce::FileChooser> activeChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationSamplePlayerEditor)
};
}
