#include "CreationStationPhaserProcessor.h"
#include "CreationStationPhaserEditor.h"

namespace cs::plugins
{

namespace
{
constexpr auto rateId = "rate";
constexpr auto depthId = "depth";
constexpr auto centreFrequencyId = "centreFrequency";
constexpr auto feedbackId = "feedback";
constexpr auto mixId = "mix";
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationPhaserProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { rateId, 1 }, "Rate",
        juce::NormalisableRange<float>(0.01f, 10.0f, 0.01f, 0.4f), 0.5f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { depthId, 1 }, "Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { centreFrequencyId, 1 }, "Center",
        juce::NormalisableRange<float>(50.0f, 5000.0f, 1.0f, 0.4f), 500.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { feedbackId, 1 }, "Feedback",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { mixId, 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return { params.begin(), params.end() };
}

CreationStationPhaserProcessor::CreationStationPhaserProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    rateParam = parameters.getRawParameterValue(rateId);
    depthParam = parameters.getRawParameterValue(depthId);
    centreFrequencyParam = parameters.getRawParameterValue(centreFrequencyId);
    feedbackParam = parameters.getRawParameterValue(feedbackId);
    mixParam = parameters.getRawParameterValue(mixId);
}

CreationStationPhaserProcessor::~CreationStationPhaserProcessor() = default;

void CreationStationPhaserProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax(1, getTotalNumOutputChannels());

    phaser.prepare(spec);
}

void CreationStationPhaserProcessor::releaseResources()
{
    phaser.reset();
}

bool CreationStationPhaserProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void CreationStationPhaserProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    auto inputPeak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
        inputPeak = juce::jmax(inputPeak, buffer.getMagnitude(channel, 0, numSamples));
    currentInputLevelDb.store(inputPeak > 0.0f ? juce::Decibels::gainToDecibels(inputPeak) : -100.0f);

    phaser.setRate(rateParam->load());
    phaser.setDepth(depthParam->load() * 0.01f);
    phaser.setCentreFrequency(centreFrequencyParam->load());
    phaser.setFeedback(feedbackParam->load() * 0.01f);
    phaser.setMix(mixParam->load() * 0.01f);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    phaser.process(context);
}

juce::AudioProcessorEditor* CreationStationPhaserProcessor::createEditor()
{
    return new CreationStationPhaserEditor(*this);
}

void CreationStationPhaserProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationPhaserProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationPhaserProcessor();
}
