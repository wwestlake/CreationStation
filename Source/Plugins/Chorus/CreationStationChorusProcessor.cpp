#include "CreationStationChorusProcessor.h"
#include "CreationStationChorusEditor.h"

namespace cs::plugins
{

namespace
{
constexpr auto rateId = "rate";
constexpr auto depthId = "depth";
constexpr auto centreDelayId = "centreDelay";
constexpr auto feedbackId = "feedback";
constexpr auto mixId = "mix";
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationChorusProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { rateId, 1 }, "Rate",
        juce::NormalisableRange<float>(0.01f, 10.0f, 0.01f, 0.4f), 1.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { depthId, 1 }, "Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 25.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { centreDelayId, 1 }, "Delay",
        juce::NormalisableRange<float>(1.0f, 100.0f, 0.1f, 0.5f), 7.5f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

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

CreationStationChorusProcessor::CreationStationChorusProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    rateParam = parameters.getRawParameterValue(rateId);
    depthParam = parameters.getRawParameterValue(depthId);
    centreDelayParam = parameters.getRawParameterValue(centreDelayId);
    feedbackParam = parameters.getRawParameterValue(feedbackId);
    mixParam = parameters.getRawParameterValue(mixId);
}

CreationStationChorusProcessor::~CreationStationChorusProcessor() = default;

void CreationStationChorusProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax(1, getTotalNumOutputChannels());

    chorus.prepare(spec);
}

void CreationStationChorusProcessor::releaseResources()
{
    chorus.reset();
}

bool CreationStationChorusProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void CreationStationChorusProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    chorus.setRate(rateParam->load());
    chorus.setDepth(depthParam->load() * 0.01f);
    chorus.setCentreDelay(centreDelayParam->load());
    chorus.setFeedback(feedbackParam->load() * 0.01f);
    chorus.setMix(mixParam->load() * 0.01f);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    chorus.process(context);
}

juce::AudioProcessorEditor* CreationStationChorusProcessor::createEditor()
{
    return new CreationStationChorusEditor(*this);
}

void CreationStationChorusProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationChorusProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationChorusProcessor();
}
