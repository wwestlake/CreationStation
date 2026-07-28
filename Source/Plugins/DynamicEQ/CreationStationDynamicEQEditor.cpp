#include "CreationStationDynamicEQEditor.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{
CreationStationDynamicEQEditor::CreationStationDynamicEQEditor(CreationStationDynamicEQProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), processorRef(processorToEdit)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(header);
    addAndMakeVisible(curve);

    curve.onBandDragged = [this](int band, double freq, float gainDb)
    {
        if (auto* freqParam = processorRef.apvts.getParameter(CreationStationDynamicEQProcessor::freqParamId(band)))
            freqParam->setValueNotifyingHost(freqParam->convertTo0to1((float) freq));

        if (auto* gainParam = processorRef.apvts.getParameter(CreationStationDynamicEQProcessor::gainParamId(band)))
            gainParam->setValueNotifyingHost(gainParam->convertTo0to1(gainDb - processorRef.getBandDynamicGainDb(band)));
    };

    for (int band = 0; band < CreationStationDynamicEQProcessor::numBands; ++band)
    {
        auto& strip = bandStrips[(size_t) band];
        strip.title.setText("Band " + juce::String(band + 1), juce::dontSendNotification);
        strip.title.setJustificationType(juce::Justification::centred);
        strip.title.setColour(juce::Label::textColourId, palette::textPrimary);
        strip.title.setFont(juce::Font(12.0f).boldened());
        addAndMakeVisible(strip.title);

        strip.typeCombo.addItemList({ "Bell", "Low Shelf", "High Shelf" }, 1);
        addAndMakeVisible(strip.typeCombo);
        strip.typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processorRef.apvts, CreationStationDynamicEQProcessor::typeParamId(band), strip.typeCombo);

        strip.modeCombo.addItemList({ "Cut", "Boost" }, 1);
        addAndMakeVisible(strip.modeCombo);
        strip.modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processorRef.apvts, CreationStationDynamicEQProcessor::modeParamId(band), strip.modeCombo);

        configureBandKnob(strip.freqSlider, strip.freqAttachment, CreationStationDynamicEQProcessor::freqParamId(band));
        configureBandKnob(strip.gainSlider, strip.gainAttachment, CreationStationDynamicEQProcessor::gainParamId(band));
        configureBandKnob(strip.rangeSlider, strip.rangeAttachment, CreationStationDynamicEQProcessor::rangeParamId(band));
        configureBandKnob(strip.thresholdSlider, strip.thresholdAttachment, CreationStationDynamicEQProcessor::thresholdParamId(band));
        configureBandKnob(strip.qSlider, strip.qAttachment, CreationStationDynamicEQProcessor::qParamId(band));

        strip.bypassButton.setClickingTogglesState(true);
        addAndMakeVisible(strip.bypassButton);
        strip.bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processorRef.apvts, CreationStationDynamicEQProcessor::bypassParamId(band), strip.bypassButton);

        strip.dynamicGainLabel.setText("0.0 dB dyn", juce::dontSendNotification);
        strip.dynamicGainLabel.setJustificationType(juce::Justification::centred);
        strip.dynamicGainLabel.setColour(juce::Label::textColourId, palette::textSecondary);
        strip.dynamicGainLabel.setFont(juce::Font(11.0f));
        addAndMakeVisible(strip.dynamicGainLabel);
    }

    configureGlobalKnob(attackKnob, "attack", "Attack");
    configureGlobalKnob(releaseKnob, "release", "Release");
    configureGlobalKnob(mixKnob, "mix", "Mix");
    configureGlobalKnob(outputKnob, "outputTrim", "Output");

    masterBypassButton.setClickingTogglesState(true);
    addAndMakeVisible(masterBypassButton);
    masterBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.apvts, "masterBypass", masterBypassButton);

    refreshCurve();
    refreshSpectrum();
    setSize(1040, 760);
    startTimerHz(30);
}

CreationStationDynamicEQEditor::~CreationStationDynamicEQEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void CreationStationDynamicEQEditor::timerCallback()
{
    refreshCurve();
    refreshSpectrum();

    for (int band = 0; band < CreationStationDynamicEQProcessor::numBands; ++band)
        bandStrips[(size_t) band].dynamicGainLabel.setText(juce::String(processorRef.getBandDynamicGainDb(band), 1) + " dB dyn",
                                                           juce::dontSendNotification);
}

void CreationStationDynamicEQEditor::refreshCurve()
{
    curve.setSampleRate(processorRef.getCurrentSampleRate());

    std::vector<EqCurveComponent::Band> curveBands;
    for (int band = 0; band < CreationStationDynamicEQProcessor::numBands; ++band)
    {
        EqCurveComponent::Band b;
        b.coefficients = processorRef.getBandCoefficients(band);
        b.frequencyHz = processorRef.getBandFrequency(band);
        b.gainDb = processorRef.getBandGainDb(band);
        curveBands.push_back(b);
    }

    curve.setBands(std::move(curveBands));
}

void CreationStationDynamicEQEditor::refreshSpectrum()
{
    juce::AudioBuffer<float> analyzerBuffer;
    if (! processorRef.copyAnalyzerBuffer(analyzerBuffer))
        return;

    curve.updateAnalyzer(analyzerBuffer);
}

void CreationStationDynamicEQEditor::configureBandKnob(
    juce::Slider& slider,
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
    const juce::String& parameterId)
{
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    slider.setColour(juce::Slider::textBoxTextColourId, palette::textPrimary);
    addAndMakeVisible(slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, parameterId, slider);
}

void CreationStationDynamicEQEditor::configureGlobalKnob(GlobalKnob& knob,
                                                         const juce::String& parameterId,
                                                         const juce::String& labelText)
{
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    knob.slider.setColour(juce::Slider::textBoxTextColourId, palette::textPrimary);
    addAndMakeVisible(knob.slider);

    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, palette::textSecondary);
    knob.label.setFont(juce::Font(12.0f).boldened());
    addAndMakeVisible(knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, parameterId, knob.slider);
}

void CreationStationDynamicEQEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void CreationStationDynamicEQEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(56));
    area.reduce(16, 16);

    curve.setBounds(area.removeFromTop(312));
    area.removeFromTop(14);

    auto bandsArea = area.removeFromTop(420);
    auto bandWidth = bandsArea.getWidth() / CreationStationDynamicEQProcessor::numBands;

    for (auto& strip : bandStrips)
    {
        auto bandArea = bandsArea.removeFromLeft(bandWidth).reduced(6, 0);
        strip.title.setBounds(bandArea.removeFromTop(18));
        bandArea.removeFromTop(4);
        auto comboRow = bandArea.removeFromTop(26);
        strip.typeCombo.setBounds(comboRow.removeFromLeft(comboRow.getWidth() / 2).reduced(2, 0));
        strip.modeCombo.setBounds(comboRow.reduced(2, 0));
        bandArea.removeFromTop(6);
        strip.dynamicGainLabel.setBounds(bandArea.removeFromTop(16));
        bandArea.removeFromTop(6);

        auto topKnobs = bandArea.removeFromTop(150);
        auto bottomKnobs = bandArea.removeFromTop(150);
        auto footer = bandArea;

        auto layoutRow = [](juce::Rectangle<int> row, juce::Slider& a, juce::Slider& b, juce::Slider& c)
        {
            auto width = row.getWidth() / 3;
            a.setBounds(row.removeFromLeft(width).reduced(3, 0));
            b.setBounds(row.removeFromLeft(width).reduced(3, 0));
            c.setBounds(row.reduced(3, 0));
        };

        layoutRow(topKnobs, strip.freqSlider, strip.gainSlider, strip.rangeSlider);

        auto thresholdArea = bottomKnobs.removeFromLeft(bottomKnobs.getWidth() / 2).reduced(3, 0);
        auto qArea = bottomKnobs.reduced(3, 0);
        strip.thresholdSlider.setBounds(thresholdArea);
        strip.qSlider.setBounds(qArea);

        strip.bypassButton.setBounds(footer.removeFromTop(26).withSizeKeepingCentre(78, 22));
    }

    auto globals = area;
    auto bypassArea = globals.removeFromLeft(90);
    masterBypassButton.setBounds(bypassArea.withSizeKeepingCentre(78, 24));
    globals.removeFromLeft(12);
    auto globalWidth = globals.getWidth() / 4;

    auto layoutGlobal = [&](GlobalKnob& knob)
    {
        auto slot = globals.removeFromLeft(globalWidth).reduced(6, 4);
        knob.label.setBounds(slot.removeFromTop(18));
        knob.slider.setBounds(slot);
    };

    layoutGlobal(attackKnob);
    layoutGlobal(releaseKnob);
    layoutGlobal(mixKnob);
    layoutGlobal(outputKnob);
}
}
