#include "CreationStationReverbProcessor.h"
#include "CreationStationReverbEditor.h"

namespace cs::plugins
{

namespace
{
constexpr auto sizeId = "size";
constexpr auto dampingId = "damping";
constexpr auto widthId = "width";
constexpr auto mixId = "mix";
constexpr auto freezeId = "freeze";
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationReverbProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { sizeId, 1 }, "Size",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { dampingId, 1 }, "Damping",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { widthId, 1 }, "Width",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { mixId, 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 30.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { freezeId, 1 }, "Freeze", false));

    return { params.begin(), params.end() };
}

CreationStationReverbProcessor::CreationStationReverbProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    sizeParam = parameters.getRawParameterValue(sizeId);
    dampingParam = parameters.getRawParameterValue(dampingId);
    widthParam = parameters.getRawParameterValue(widthId);
    mixParam = parameters.getRawParameterValue(mixId);
    freezeParam = parameters.getRawParameterValue(freezeId);
}

CreationStationReverbProcessor::~CreationStationReverbProcessor() = default;

void CreationStationReverbProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax(1, getTotalNumOutputChannels());

    reverb.prepare(spec);
}

void CreationStationReverbProcessor::releaseResources()
{
    reverb.reset();
}

bool CreationStationReverbProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void CreationStationReverbProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    auto mix = mixParam->load() * 0.01f;

    juce::dsp::Reverb::Parameters reverbParams;
    reverbParams.roomSize = sizeParam->load() * 0.01f;
    reverbParams.damping = dampingParam->load() * 0.01f;
    reverbParams.width = widthParam->load() * 0.01f;
    reverbParams.wetLevel = mix;
    reverbParams.dryLevel = 1.0f - mix;
    reverbParams.freezeMode = freezeParam->load() > 0.5f ? 1.0f : 0.0f;
    reverb.setParameters(reverbParams);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    reverb.process(context);
}

juce::AudioProcessorEditor* CreationStationReverbProcessor::createEditor()
{
    return new CreationStationReverbEditor(*this);
}

void CreationStationReverbProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationReverbProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationReverbProcessor();
}
