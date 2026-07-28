#include "CreationStationCompressorProcessor.h"
#include "CreationStationCompressorEditor.h"

namespace cs::plugins
{

namespace
{
constexpr auto thresholdId = "threshold";
constexpr auto ratioId = "ratio";
constexpr auto attackId = "attack";
constexpr auto releaseId = "release";
constexpr auto makeupId = "makeup";
constexpr auto mixId = "mix";
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationCompressorProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { thresholdId, 1 }, "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -18.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ratioId, 1 }, "Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 4.0f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { attackId, 1 }, "Attack",
        juce::NormalisableRange<float>(0.1f, 200.0f, 0.1f, 0.35f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { releaseId, 1 }, "Release",
        juce::NormalisableRange<float>(5.0f, 1000.0f, 1.0f, 0.35f), 120.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { makeupId, 1 }, "Makeup",
        juce::NormalisableRange<float>(-12.0f, 24.0f, 0.1f), 3.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { mixId, 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return { params.begin(), params.end() };
}

CreationStationCompressorProcessor::CreationStationCompressorProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    thresholdParam = parameters.getRawParameterValue(thresholdId);
    ratioParam = parameters.getRawParameterValue(ratioId);
    attackParam = parameters.getRawParameterValue(attackId);
    releaseParam = parameters.getRawParameterValue(releaseId);
    makeupGainParam = parameters.getRawParameterValue(makeupId);
    mixParam = parameters.getRawParameterValue(mixId);
}

CreationStationCompressorProcessor::~CreationStationCompressorProcessor() = default;

void CreationStationCompressorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax(1, getTotalNumOutputChannels());

    compressor.prepare(spec);
    dryWetMixer.prepare(spec);
    makeupGain.prepare(spec);
}

void CreationStationCompressorProcessor::releaseResources()
{
}

bool CreationStationCompressorProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void CreationStationCompressorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    compressor.setThreshold(thresholdParam->load());
    compressor.setRatio(ratioParam->load());
    compressor.setAttack(attackParam->load());
    compressor.setRelease(releaseParam->load());
    makeupGain.setGainDecibels(makeupGainParam->load());
    dryWetMixer.setWetMixProportion(mixParam->load() * 0.01f);

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    // Input level (peak across channels) in dBFS, for the UI's live meter - measured before any
    // processing so the threshold line the user drags reads against the true incoming signal.
    auto inputPeak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
        inputPeak = juce::jmax(inputPeak, buffer.getMagnitude(channel, 0, numSamples));
    currentInputLevelDb.store(inputPeak > 0.0f ? juce::Decibels::gainToDecibels(inputPeak) : -100.0f);

    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);

    auto preRms = buffer.getRMSLevel(0, 0, numSamples);

    juce::dsp::ProcessContextReplacing<float> context(block);
    compressor.process(context);

    // Approximate gain reduction from the pre/post RMS ratio, with simple meter ballistics
    // (jump up instantly, decay back toward 0) so the GR meter reads steadily rather than
    // flickering per block.
    auto instantReductionDb = 0.0f;
    auto postCompressionRms = buffer.getRMSLevel(0, 0, numSamples);
    if (preRms > 0.0001f && postCompressionRms > 0.0f)
        instantReductionDb = juce::jmax(0.0f, -juce::Decibels::gainToDecibels((float) (postCompressionRms / preRms)));

    auto smoothedReduction = currentGainReductionDb.load();
    smoothedReduction = instantReductionDb > smoothedReduction ? instantReductionDb
                                                               : smoothedReduction * 0.85f + instantReductionDb * 0.15f;
    currentGainReductionDb.store(smoothedReduction);

    makeupGain.process(context);
    dryWetMixer.mixWetSamples(block);
}

juce::AudioProcessorEditor* CreationStationCompressorProcessor::createEditor()
{
    return new CreationStationCompressorEditor(*this);
}

void CreationStationCompressorProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationCompressorProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationCompressorProcessor();
}
