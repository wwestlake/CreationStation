#include "CreationStationDelayProcessor.h"
#include "CreationStationDelayEditor.h"

namespace cs::plugins
{

namespace
{
constexpr auto timeId = "time";
constexpr auto feedbackId = "feedback";
constexpr auto mixId = "mix";
constexpr auto toneId = "tone";
constexpr auto pingPongId = "pingPong";
constexpr auto maxDelaySeconds = 2.5;
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationDelayProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { timeId, 1 }, "Time",
        juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f, 0.3f), 350.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { feedbackId, 1 }, "Feedback",
        juce::NormalisableRange<float>(0.0f, 95.0f, 0.1f), 35.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { mixId, 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { toneId, 1 }, "Tone",
        juce::NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.3f), 8000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { pingPongId, 1 }, "Ping-Pong", false));

    return { params.begin(), params.end() };
}

CreationStationDelayProcessor::CreationStationDelayProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    timeParam = parameters.getRawParameterValue(timeId);
    feedbackParam = parameters.getRawParameterValue(feedbackId);
    mixParam = parameters.getRawParameterValue(mixId);
    toneParam = parameters.getRawParameterValue(toneId);
    pingPongParam = parameters.getRawParameterValue(pingPongId);
}

CreationStationDelayProcessor::~CreationStationDelayProcessor() = default;

void CreationStationDelayProcessor::updateFeedbackFilterCoefficients(float toneHz)
{
    if (juce::approximatelyEqual(toneHz, lastToneHz))
        return;

    lastToneHz = toneHz;
    auto safeHz = juce::jlimit(20.0f, (float) (currentSampleRate * 0.49), toneHz);
    auto coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, safeHz);

    for (auto& filter : feedbackFilters)
        filter.coefficients = coefficients;
}

void CreationStationDelayProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax(1, getTotalNumOutputChannels());

    delayLine.setMaximumDelayInSamples((int) (maxDelaySeconds * sampleRate) + 1);
    delayLine.prepare(spec);

    for (auto& filter : feedbackFilters)
        filter.prepare(spec);

    auto blockRate = sampleRate / (double) juce::jmax(1, samplesPerBlock);
    toneSmoother.reset(blockRate, 0.03);
    toneSmoother.setCurrentAndTargetValue(toneParam->load());

    // A longer ramp than the tone filter's - a fast glide on a big time jump still reads as a
    // deliberate tape-echo-style pitch bend, while an instant jump is an audible skip/click.
    timeSmoother.reset(blockRate, 0.08);
    timeSmoother.setCurrentAndTargetValue(timeParam->load());

    lastToneHz = -1.0f;
    updateFeedbackFilterCoefficients(toneSmoother.getCurrentValue());
}

void CreationStationDelayProcessor::releaseResources()
{
    delayLine.reset();
    for (auto& filter : feedbackFilters)
        filter.reset();
}

bool CreationStationDelayProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void CreationStationDelayProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    auto numSamples = buffer.getNumSamples();
    auto numChannels = juce::jmin(buffer.getNumChannels(), 2);

    auto inputPeak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
        inputPeak = juce::jmax(inputPeak, buffer.getMagnitude(channel, 0, numSamples));
    currentInputLevelDb.store(inputPeak > 0.0f ? juce::Decibels::gainToDecibels(inputPeak) : -100.0f);

    timeSmoother.setTargetValue(timeParam->load());
    auto delaySamples = (float) (timeSmoother.getNextValue() * 0.001 * currentSampleRate);
    delayLine.setDelay(delaySamples);

    toneSmoother.setTargetValue(toneParam->load());
    updateFeedbackFilterCoefficients(toneSmoother.getNextValue());

    auto feedbackAmount = feedbackParam->load() * 0.01f;
    auto mix = mixParam->load() * 0.01f;
    bool pingPong = pingPongParam->load() > 0.5f && numChannels == 2;

    for (int n = 0; n < numSamples; ++n)
    {
        float input[2] { 0.0f, 0.0f };
        float delayed[2] { 0.0f, 0.0f };

        for (int channel = 0; channel < numChannels; ++channel)
        {
            input[channel] = buffer.getSample(channel, n);
            delayed[channel] = delayLine.popSample(channel);
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto feedbackSourceChannel = pingPong ? (1 - channel) : channel;
            auto filteredFeedback = feedbackFilters[(size_t) channel].processSample(delayed[feedbackSourceChannel] * feedbackAmount);
            delayLine.pushSample(channel, input[channel] + filteredFeedback);

            auto output = input[channel] * (1.0f - mix) + delayed[channel] * mix;
            buffer.setSample(channel, n, output);
        }
    }
}

juce::AudioProcessorEditor* CreationStationDelayProcessor::createEditor()
{
    return new CreationStationDelayEditor(*this);
}

void CreationStationDelayProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationDelayProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationDelayProcessor();
}
