#include "CreationStationParametricEQEditor.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{
CreationStationParametricEQEditor::CreationStationParametricEQEditor(CreationStationParametricEQProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), processorRef(processorToEdit)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(header);
    addAndMakeVisible(curve);

    curve.onBandDragged = [this](int band, double freq, float gainDb)
    {
        if (auto* freqParam = processorRef.apvts.getParameter(CreationStationParametricEQProcessor::freqParamId(band)))
            freqParam->setValueNotifyingHost(freqParam->convertTo0to1((float) freq));

        if (auto* gainParam = processorRef.apvts.getParameter(CreationStationParametricEQProcessor::gainParamId(band)))
            gainParam->setValueNotifyingHost(gainParam->convertTo0to1(gainDb));
    };

    auto configureBandKnob = [this](juce::Slider& slider,
                                     std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                                     const juce::String& parameterId)
    {
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 16);
        slider.setColour(juce::Slider::textBoxTextColourId, palette::textPrimary);
        addAndMakeVisible(slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, parameterId, slider);
    };

    for (int band = 0; band < CreationStationParametricEQProcessor::numBands; ++band)
    {
        auto& strip = bandStrips[(size_t) band];

        strip.label.setText("Band " + juce::String(band + 1), juce::dontSendNotification);
        strip.label.setJustificationType(juce::Justification::centred);
        strip.label.setColour(juce::Label::textColourId, palette::textSecondary);
        strip.label.setFont(juce::Font(12.0f).boldened());
        addAndMakeVisible(strip.label);

        strip.typeCombo.addItemList({ "Bell", "Low Shelf", "High Shelf", "Low-Pass", "High-Pass" }, 1);
        addAndMakeVisible(strip.typeCombo);
        strip.typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processorRef.apvts, CreationStationParametricEQProcessor::typeParamId(band), strip.typeCombo);

        configureBandKnob(strip.freqSlider, strip.freqAttachment, CreationStationParametricEQProcessor::freqParamId(band));
        configureBandKnob(strip.gainSlider, strip.gainAttachment, CreationStationParametricEQProcessor::gainParamId(band));
        configureBandKnob(strip.qSlider, strip.qAttachment, CreationStationParametricEQProcessor::qParamId(band));

        strip.bypassButton.setClickingTogglesState(true);
        addAndMakeVisible(strip.bypassButton);
        strip.bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processorRef.apvts, CreationStationParametricEQProcessor::bypassParamId(band), strip.bypassButton);
    }

    masterBypassButton.setClickingTogglesState(true);
    addAndMakeVisible(masterBypassButton);
    masterBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.apvts, "masterBypass", masterBypassButton);

    outputTrimLabel.setText("Output Trim", juce::dontSendNotification);
    outputTrimLabel.setJustificationType(juce::Justification::centredLeft);
    outputTrimLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    outputTrimLabel.setFont(juce::Font(12.0f).boldened());
    addAndMakeVisible(outputTrimLabel);

    outputTrimSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 16);
    outputTrimSlider.setColour(juce::Slider::textBoxTextColourId, palette::textPrimary);
    addAndMakeVisible(outputTrimSlider);
    outputTrimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "outputTrim", outputTrimSlider);

    refreshCurve();
    refreshSpectrum();

    setSize(760, 664);
    startTimerHz(30);
}

CreationStationParametricEQEditor::~CreationStationParametricEQEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void CreationStationParametricEQEditor::timerCallback()
{
    refreshCurve();
    refreshSpectrum();
}

void CreationStationParametricEQEditor::refreshCurve()
{
    curve.setSampleRate(processorRef.getCurrentSampleRate());

    std::vector<EqCurveComponent::Band> bands;
    for (int band = 0; band < CreationStationParametricEQProcessor::numBands; ++band)
    {
        EqCurveComponent::Band b;
        b.coefficients = processorRef.getBandCoefficients(band);
        b.frequencyHz = processorRef.getBandFrequency(band);
        b.gainDb = processorRef.getBandGainDb(band);

        auto type = processorRef.getBandType(band);
        b.hasGainAxis = (type != CreationStationParametricEQProcessor::BandType::lowPass
                          && type != CreationStationParametricEQProcessor::BandType::highPass);

        bands.push_back(b);
    }
    curve.setBands(std::move(bands));
}

void CreationStationParametricEQEditor::refreshSpectrum()
{
    juce::AudioBuffer<float> analyzerBuffer;
    if (! processorRef.copyAnalyzerBuffer(analyzerBuffer))
        return;

    curve.updateAnalyzer(analyzerBuffer);
}

void CreationStationParametricEQEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void CreationStationParametricEQEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(56));
    area.reduce(16, 16);

    curve.setBounds(area.removeFromTop(312));
    area.removeFromTop(12);

    auto stripsArea = area.removeFromTop(area.getHeight() - 56);
    auto stripWidth = stripsArea.getWidth() / CreationStationParametricEQProcessor::numBands;

    for (auto& strip : bandStrips)
    {
        auto stripArea = stripsArea.removeFromLeft(stripWidth).reduced(6, 0);

        strip.label.setBounds(stripArea.removeFromTop(16));
        stripArea.removeFromTop(4);
        strip.typeCombo.setBounds(stripArea.removeFromTop(24));
        stripArea.removeFromTop(6);

        auto bypassRow = stripArea.removeFromBottom(22);
        strip.bypassButton.setBounds(bypassRow.withSizeKeepingCentre(70, 20));

        auto knobWidth = stripArea.getWidth() / 3;
        strip.freqSlider.setBounds(stripArea.removeFromLeft(knobWidth).reduced(2, 0));
        strip.gainSlider.setBounds(stripArea.removeFromLeft(knobWidth).reduced(2, 0));
        strip.qSlider.setBounds(stripArea.reduced(2, 0));
    }

    area.removeFromTop(8);
    auto masterRow = area;
    masterBypassButton.setBounds(masterRow.removeFromLeft(80).withSizeKeepingCentre(70, 24));
    masterRow.removeFromLeft(16);
    outputTrimLabel.setBounds(masterRow.removeFromLeft(90));
    outputTrimSlider.setBounds(masterRow.removeFromLeft(70));
}

}
