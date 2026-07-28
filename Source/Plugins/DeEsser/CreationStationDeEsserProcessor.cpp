#include "CreationStationDeEsserProcessor.h"
#include "CreationStationDeEsserEditor.h"

namespace cs::plugins
{

namespace
{
constexpr auto frequencyId = "frequency";
constexpr auto thresholdId = "threshold";
constexpr auto ratioId = "ratio";
constexpr auto listenId = "listen";
constexpr auto gainRampSeconds = 0.005;
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationDeEsserProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { frequencyId, 1 }, "Frequency",
        juce::NormalisableRange<float>(2000.0f, 12000.0f, 1.0f, 0.4f), 6000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { thresholdId, 1 }, "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -24.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ratioId, 1 }, "Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 4.0f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { listenId, 1 }, "Listen", false));

    return { params.begin(), params.end() };
}

CreationStationDeEsserProcessor::CreationStationDeEsserProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    frequencyParam = parameters.getRawParameterValue(frequencyId);
    thresholdParam = parameters.getRawParameterValue(thresholdId);
    ratioParam = parameters.getRawParameterValue(ratioId);
    listenParam = parameters.getRawParameterValue(listenId);
}

CreationStationDeEsserProcessor::~CreationStationDeEsserProcessor() = default;

void CreationStationDeEsserProcessor::updateSplitCoefficients(float frequencyHz)
{
    if (juce::approximatelyEqual(frequencyHz, lastSplitFrequency))
        return;

    lastSplitFrequency = frequencyHz;
    auto safeHz = juce::jlimit(20.0f, (float) (currentSampleRate * 0.49), frequencyHz);

    *lowFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, safeHz);
    *highFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, safeHz);
}

void CreationStationDeEsserProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    auto numChannels = juce::jmax(1, getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) numChannels;

    lowFilter.prepare(spec);
    highFilter.prepare(spec);

    lowBandBuffer.setSize(numChannels, samplesPerBlock);
    highBandBuffer.setSize(numChannels, samplesPerBlock);

    gainSmoother.reset(sampleRate, gainRampSeconds);
    gainSmoother.setCurrentAndTargetValue(1.0f);

    frequencySmoother.reset(sampleRate / (double) juce::jmax(1, samplesPerBlock), 0.03);
    frequencySmoother.setCurrentAndTargetValue(frequencyParam->load());

    lastSplitFrequency = -1.0f;
    updateSplitCoefficients(frequencySmoother.getCurrentValue());

    displayedReductionDb = 0.0f;
    currentReductionDb.store(0.0f);
}

void CreationStationDeEsserProcessor::releaseResources()
{
    lowFilter.reset();
    highFilter.reset();
}

bool CreationStationDeEsserProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void CreationStationDeEsserProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    frequencySmoother.setTargetValue(frequencyParam->load());
    updateSplitCoefficients(frequencySmoother.getNextValue());

    for (int channel = 0; channel < numChannels; ++channel)
    {
        lowBandBuffer.copyFrom(channel, 0, buffer, channel, 0, numSamples);
        highBandBuffer.copyFrom(channel, 0, buffer, channel, 0, numSamples);
    }

    juce::dsp::AudioBlock<float> lowBlock(lowBandBuffer);
    lowBlock = lowBlock.getSubBlock(0, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> lowContext(lowBlock);
    lowFilter.process(lowContext);

    juce::dsp::AudioBlock<float> highBlock(highBandBuffer);
    highBlock = highBlock.getSubBlock(0, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> highContext(highBlock);
    highFilter.process(highContext);

    // Threshold/ratio gain reduction, derived from the sibilance band's peak level and applied
    // only to that band - a classic downward-compressor formula, same shape as the Compressor
    // plugin's ratio math but scoped to the high band instead of the full signal.
    auto highPeak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
        highPeak = juce::jmax(highPeak, highBandBuffer.getMagnitude(channel, 0, numSamples));

    auto levelDb = highPeak > 0.0f ? juce::Decibels::gainToDecibels(highPeak) : -100.0f;
    auto threshold = thresholdParam->load();
    auto ratio = ratioParam->load();

    auto overDb = levelDb - threshold;
    auto reductionDb = overDb > 0.0f ? overDb - (overDb / ratio) : 0.0f;
    auto targetGain = juce::Decibels::decibelsToGain(-reductionDb);

    gainSmoother.setTargetValue(targetGain);

    for (int n = 0; n < numSamples; ++n)
    {
        auto gain = gainSmoother.getNextValue();
        for (int channel = 0; channel < numChannels; ++channel)
            highBandBuffer.setSample(channel, n, highBandBuffer.getSample(channel, n) * gain);
    }

    displayedReductionDb = reductionDb > displayedReductionDb ? reductionDb
                                                              : displayedReductionDb * 0.85f + reductionDb * 0.15f;
    currentReductionDb.store(displayedReductionDb);

    bool listen = listenParam->load() > 0.5f;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            auto output = listen ? highBandBuffer.getSample(channel, n)
                                  : lowBandBuffer.getSample(channel, n) + highBandBuffer.getSample(channel, n);
            buffer.setSample(channel, n, output);
        }
    }
}

juce::AudioProcessorEditor* CreationStationDeEsserProcessor::createEditor()
{
    return new CreationStationDeEsserEditor(*this);
}

void CreationStationDeEsserProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationDeEsserProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationDeEsserProcessor();
}
