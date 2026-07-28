#include "CreationStationResonanceSuppressorEditor.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{
CreationStationResonanceSuppressorEditor::CreationStationResonanceSuppressorEditor(
    CreationStationResonanceSuppressorProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), processorRef(processorToEdit)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(header);
    addAndMakeVisible(curve);
    addAndMakeVisible(hintLabel);

    hintLabel.setText("Dial bands onto harsh areas, then let depth + threshold tuck them only when they flare up.",
                      juce::dontSendNotification);
    hintLabel.setJustificationType(juce::Justification::centredLeft);
    hintLabel.setColour(juce::Label::textColourId, palette::textSecondary);

    curve.onBandDragged = [this](int band, double freq, float gainDb)
    {
        if (auto* freqParam = processorRef.apvts.getParameter(CreationStationResonanceSuppressorProcessor::freqParamId(band)))
            freqParam->setValueNotifyingHost(freqParam->convertTo0to1((float) freq));

        if (auto* depthParam = processorRef.apvts.getParameter(CreationStationResonanceSuppressorProcessor::depthParamId(band)))
            depthParam->setValueNotifyingHost(depthParam->convertTo0to1(juce::jlimit(0.0f, 18.0f, -gainDb)));
    };

    for (int band = 0; band < CreationStationResonanceSuppressorProcessor::numBands; ++band)
    {
        auto& strip = bandStrips[(size_t) band];
        strip.title.setText("Band " + juce::String(band + 1), juce::dontSendNotification);
        strip.title.setJustificationType(juce::Justification::centred);
        strip.title.setColour(juce::Label::textColourId, palette::textPrimary);
        strip.title.setFont(juce::Font(12.0f).boldened());
        addAndMakeVisible(strip.title);

        configureBandKnob(strip.freqSlider, strip.freqAttachment, CreationStationResonanceSuppressorProcessor::freqParamId(band));
        configureBandKnob(strip.depthSlider, strip.depthAttachment, CreationStationResonanceSuppressorProcessor::depthParamId(band));
        configureBandKnob(strip.thresholdSlider, strip.thresholdAttachment, CreationStationResonanceSuppressorProcessor::thresholdParamId(band));
        configureBandKnob(strip.qSlider, strip.qAttachment, CreationStationResonanceSuppressorProcessor::qParamId(band));

        strip.bypassButton.setClickingTogglesState(true);
        addAndMakeVisible(strip.bypassButton);
        strip.bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processorRef.apvts, CreationStationResonanceSuppressorProcessor::bypassParamId(band), strip.bypassButton);

        strip.reductionLabel.setText("0.0 dB cut", juce::dontSendNotification);
        strip.reductionLabel.setJustificationType(juce::Justification::centred);
        strip.reductionLabel.setColour(juce::Label::textColourId, palette::textSecondary);
        strip.reductionLabel.setFont(juce::Font(11.0f));
        addAndMakeVisible(strip.reductionLabel);
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
    setSize(1120, 760);
    startTimerHz(30);
}

CreationStationResonanceSuppressorEditor::~CreationStationResonanceSuppressorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void CreationStationResonanceSuppressorEditor::timerCallback()
{
    refreshCurve();
    refreshSpectrum();

    for (int band = 0; band < CreationStationResonanceSuppressorProcessor::numBands; ++band)
        bandStrips[(size_t) band].reductionLabel.setText(juce::String(processorRef.getBandDynamicGainDb(band), 1) + " dB cut",
                                                         juce::dontSendNotification);
}

void CreationStationResonanceSuppressorEditor::refreshCurve()
{
    curve.setSampleRate(processorRef.getCurrentSampleRate());

    std::vector<EqCurveComponent::Band> curveBands;
    for (int band = 0; band < CreationStationResonanceSuppressorProcessor::numBands; ++band)
    {
        EqCurveComponent::Band curveBand;
        curveBand.coefficients = processorRef.getBandCoefficients(band);
        curveBand.frequencyHz = processorRef.getBandFrequency(band);
        curveBand.gainDb = -processorRef.getBandDepthDb(band);
        curveBands.push_back(curveBand);
    }

    curve.setBands(std::move(curveBands));
}

void CreationStationResonanceSuppressorEditor::refreshSpectrum()
{
    juce::AudioBuffer<float> analyzerBuffer;
    if (! processorRef.copyAnalyzerBuffer(analyzerBuffer))
        return;

    curve.updateAnalyzer(analyzerBuffer);
}

void CreationStationResonanceSuppressorEditor::configureBandKnob(
    juce::Slider& slider,
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
    const juce::String& parameterId)
{
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    slider.setColour(juce::Slider::textBoxTextColourId, palette::textPrimary);
    addAndMakeVisible(slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.apvts, parameterId, slider);
}

void CreationStationResonanceSuppressorEditor::configureGlobalKnob(GlobalKnob& knob,
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

void CreationStationResonanceSuppressorEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void CreationStationResonanceSuppressorEditor::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(56));
    area.reduce(16, 16);

    curve.setBounds(area.removeFromTop(292));
    area.removeFromTop(8);
    hintLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(14);

    auto bandsArea = area.removeFromTop(420);
    auto bandWidth = bandsArea.getWidth() / CreationStationResonanceSuppressorProcessor::numBands;

    for (auto& strip : bandStrips)
    {
        auto bandArea = bandsArea.removeFromLeft(bandWidth).reduced(6, 0);
        strip.title.setBounds(bandArea.removeFromTop(18));
        bandArea.removeFromTop(6);
        strip.reductionLabel.setBounds(bandArea.removeFromTop(16));
        bandArea.removeFromTop(6);

        auto topKnobs = bandArea.removeFromTop(150);
        auto bottomKnobs = bandArea.removeFromTop(150);
        auto footer = bandArea;

        auto width = topKnobs.getWidth() / 2;
        strip.freqSlider.setBounds(topKnobs.removeFromLeft(width).reduced(3, 0));
        strip.depthSlider.setBounds(topKnobs.reduced(3, 0));

        width = bottomKnobs.getWidth() / 2;
        strip.thresholdSlider.setBounds(bottomKnobs.removeFromLeft(width).reduced(3, 0));
        strip.qSlider.setBounds(bottomKnobs.reduced(3, 0));

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
