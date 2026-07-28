#include "CreationStationLimiterProcessor.h"
#include "CreationStationLimiterEditor.h"

namespace cs::plugins
{
CreationStationLimiterProcessor::CreationStationLimiterProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

CreationStationLimiterProcessor::~CreationStationLimiterProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationLimiterProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Threshold", "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f),
        -18.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Release", "Release",
        juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f),
        100.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "MakeupGain", "Makeup Gain",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f),
        0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Mix", "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        100.0f));

    return layout;
}

void CreationStationLimiterProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.numChannels = getMainBusNumOutputChannels();
    spec.maximumBlockSize = samplesPerBlock;

    limiter.prepare(spec);
    limiter.setThreshold(-18.0f);
    limiter.setRelease(100.0f);
}

void CreationStationLimiterProcessor::releaseResources()
{
    limiter.reset();
}

bool CreationStationLimiterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void CreationStationLimiterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    auto* thresholdParam = apvts.getRawParameterValue("Threshold");
    auto* releaseParam = apvts.getRawParameterValue("Release");
    auto* makeupParam = apvts.getRawParameterValue("MakeupGain");
    auto* mixParam = apvts.getRawParameterValue("Mix");

    float threshold = thresholdParam->load();
    float release = releaseParam->load();
    float makeup = dbToLinear(makeupParam->load());
    float mix = mixParam->load() / 100.0f;

    limiter.setThreshold(threshold);
    limiter.setRelease(release);

    // Measure input level before processing
    float maxLevel = 0.0f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        maxLevel = std::max(maxLevel, buffer.getRMSLevel(ch, 0, buffer.getNumSamples()));

    inputLevelDb = linearToDb(maxLevel);

    juce::AudioBuffer<float> dryBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());

    // Process
    juce::dsp::AudioBlock<float> block(buffer);
    auto context = juce::dsp::ProcessContextReplacing<float>(block);
    limiter.process(context);

    // Measure gain reduction (simplified: compare max before/after)
    float maxAfter = 0.0f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        maxAfter = std::max(maxAfter, buffer.getRMSLevel(ch, 0, buffer.getNumSamples()));

    float reduction = linearToDb(maxLevel) - linearToDb(maxAfter);
    gainReductionDb = std::max(0.0f, reduction);

    // Apply makeup gain and mix
    buffer.applyGain(makeup);

    // Dry/wet mix
    if (mix < 1.0f)
    {
        buffer.applyGain(mix);
        dryBuffer.applyGain(1.0f - mix);
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
            buffer.addFrom(ch, 0, dryBuffer, ch, 0, buffer.getNumSamples());
    }
}

juce::AudioProcessorEditor* CreationStationLimiterProcessor::createEditor()
{
    return new CreationStationLimiterEditor(*this);
}

void CreationStationLimiterProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationLimiterProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

float CreationStationLimiterProcessor::dbToLinear(float db) const
{
    return std::pow(10.0f, db / 20.0f);
}

float CreationStationLimiterProcessor::linearToDb(float linear) const
{
    return 20.0f * std::log10(std::max(1e-6f, linear));
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationLimiterProcessor();
}
