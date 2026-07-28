#include "CreationStationGateProcessor.h"
#include "CreationStationGateEditor.h"

namespace cs::plugins
{

namespace
{
constexpr auto thresholdId = "threshold";
constexpr auto ratioId = "ratio";
constexpr auto attackId = "attack";
constexpr auto releaseId = "release";
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationGateProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { thresholdId, 1 }, "Threshold",
        juce::NormalisableRange<float>(-80.0f, 0.0f, 0.1f), -40.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ratioId, 1 }, "Ratio",
        juce::NormalisableRange<float>(1.0f, 100.0f, 0.1f, 0.3f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { attackId, 1 }, "Attack",
        juce::NormalisableRange<float>(0.1f, 200.0f, 0.1f, 0.35f), 1.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { releaseId, 1 }, "Release",
        juce::NormalisableRange<float>(5.0f, 1000.0f, 1.0f, 0.35f), 150.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    return { params.begin(), params.end() };
}

CreationStationGateProcessor::CreationStationGateProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    thresholdParam = parameters.getRawParameterValue(thresholdId);
    ratioParam = parameters.getRawParameterValue(ratioId);
    attackParam = parameters.getRawParameterValue(attackId);
    releaseParam = parameters.getRawParameterValue(releaseId);
}

CreationStationGateProcessor::~CreationStationGateProcessor() = default;

void CreationStationGateProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax(1, getTotalNumOutputChannels());

    gate.prepare(spec);
}

void CreationStationGateProcessor::releaseResources()
{
}

bool CreationStationGateProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void CreationStationGateProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    gate.setThreshold(thresholdParam->load());
    gate.setRatio(ratioParam->load());
    gate.setAttack(attackParam->load());
    gate.setRelease(releaseParam->load());

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    // Input level (peak across channels) in dBFS, for the UI's live meter - measured before any
    // processing so the threshold line the user drags reads against the true incoming signal.
    auto inputPeak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
        inputPeak = juce::jmax(inputPeak, buffer.getMagnitude(channel, 0, numSamples));
    currentInputLevelDb.store(inputPeak > 0.0f ? juce::Decibels::gainToDecibels(inputPeak) : -100.0f);

    auto preRms = buffer.getRMSLevel(0, 0, numSamples);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    gate.process(context);

    // Approximate the gate's current attenuation from the pre/post RMS ratio, with simple meter
    // ballistics (jump down instantly, recover back toward 0 gradually) so the meter reads
    // steadily rather than flickering per block - same approach as the Compressor's GR meter.
    auto instantReductionDb = 0.0f;
    auto postGateRms = buffer.getRMSLevel(0, 0, numSamples);
    if (preRms > 0.0001f && postGateRms >= 0.0f)
        instantReductionDb = juce::jmax(0.0f, -juce::Decibels::gainToDecibels((float) (postGateRms / preRms)));

    auto smoothedReduction = currentGainReductionDb.load();
    smoothedReduction = instantReductionDb > smoothedReduction ? instantReductionDb
                                                               : smoothedReduction * 0.85f + instantReductionDb * 0.15f;
    currentGainReductionDb.store(smoothedReduction);
}

juce::AudioProcessorEditor* CreationStationGateProcessor::createEditor()
{
    return new CreationStationGateEditor(*this);
}

void CreationStationGateProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationGateProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationGateProcessor();
}
