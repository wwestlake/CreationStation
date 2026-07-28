#include "CreationStationClipperProcessor.h"
#include "CreationStationClipperEditor.h"

namespace cs::plugins
{
namespace
{
constexpr auto typeId = "type";
constexpr auto driveId = "drive";
constexpr auto ceilingId = "ceiling";
constexpr auto softnessId = "softness";
constexpr auto outputId = "output";
constexpr auto mixId = "mix";
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationClipperProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { typeId, 1 }, "Type",
        juce::StringArray { "Soft", "Hard", "Asym" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { driveId, 1 }, "Drive",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 6.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ceilingId, 1 }, "Ceiling",
        juce::NormalisableRange<float>(-12.0f, 0.0f, 0.1f), -0.5f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { softnessId, 1 }, "Softness",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 40.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { outputId, 1 }, "Output",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { mixId, 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return { params.begin(), params.end() };
}

CreationStationClipperProcessor::CreationStationClipperProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    typeParam = parameters.getRawParameterValue(typeId);
    driveParam = parameters.getRawParameterValue(driveId);
    ceilingParam = parameters.getRawParameterValue(ceilingId);
    softnessParam = parameters.getRawParameterValue(softnessId);
    outputParam = parameters.getRawParameterValue(outputId);
    mixParam = parameters.getRawParameterValue(mixId);
}

CreationStationClipperProcessor::~CreationStationClipperProcessor() = default;

void CreationStationClipperProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    auto numChannels = (juce::uint32) juce::jmax(1, getTotalNumOutputChannels());
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = numChannels;

    driveGain.prepare(spec);
    outputGain.prepare(spec);
    dryWetMixer.prepare(spec);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing((size_t) samplesPerBlock);
    dryWetMixer.setWetLatency(oversampling->getLatencyInSamples());
    setLatencySamples((int) std::ceil(oversampling->getLatencyInSamples()));

    ceilingSmoother.reset(sampleRate / (double) juce::jmax(1, samplesPerBlock), 0.02);
    softnessSmoother.reset(sampleRate / (double) juce::jmax(1, samplesPerBlock), 0.02);
    ceilingSmoother.setCurrentAndTargetValue(ceilingParam->load());
    softnessSmoother.setCurrentAndTargetValue(softnessParam->load());
    lastCurveType = -1;
}

void CreationStationClipperProcessor::releaseResources()
{
    if (oversampling != nullptr)
        oversampling->reset();
}

bool CreationStationClipperProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void CreationStationClipperProcessor::updateCurveSettings()
{
    auto typeIndex = (int) typeParam->load();
    if (typeIndex == lastCurveType)
        return;

    lastCurveType = typeIndex;
}

void CreationStationClipperProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    updateCurveSettings();
    ceilingSmoother.setTargetValue(ceilingParam->load());
    softnessSmoother.setTargetValue(softnessParam->load());

    dryWetMixer.setWetMixProportion(mixParam->load() * 0.01f);
    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);

    driveGain.setGainDecibels(driveParam->load());
    juce::dsp::ProcessContextReplacing<float> context(block);
    driveGain.process(context);

    auto oversampledBlock = oversampling->processSamplesUp(block);
    auto oversampledChannels = (int) oversampledBlock.getNumChannels();
    auto oversampledSamples = (int) oversampledBlock.getNumSamples();

    auto ceilingDb = ceilingSmoother.getNextValue();
    auto ceilingLinear = juce::Decibels::decibelsToGain(ceilingDb);
    auto softness = juce::jmap(softnessSmoother.getNextValue(), 0.0f, 100.0f, 1.0f, 12.0f);
    auto curveType = static_cast<CurveType>((int) typeParam->load());

    auto preClipPeak = 0.0f;
    auto postClipPeak = 0.0f;

    for (int channel = 0; channel < oversampledChannels; ++channel)
    {
        auto* samples = oversampledBlock.getChannelPointer((size_t) channel);
        for (int sample = 0; sample < oversampledSamples; ++sample)
        {
            auto x = samples[sample];
            preClipPeak = juce::jmax(preClipPeak, std::abs(x));

            float clipped = x;
            switch (curveType)
            {
                case CurveType::soft:
                    clipped = ceilingLinear * std::tanh((x / juce::jmax(0.0001f, ceilingLinear)) * softness)
                                            / std::tanh(softness);
                    break;
                case CurveType::hard:
                    clipped = juce::jlimit(-ceilingLinear, ceilingLinear, x);
                    break;
                case CurveType::asymmetric:
                {
                    auto posCeiling = ceilingLinear;
                    auto negCeiling = ceilingLinear * 0.78f;
                    if (x >= 0.0f)
                        clipped = posCeiling * std::tanh((x / juce::jmax(0.0001f, posCeiling)) * softness)
                                                / std::tanh(softness);
                    else
                        clipped = -negCeiling * std::tanh((std::abs(x) / juce::jmax(0.0001f, negCeiling)) * (softness * 1.15f))
                                                 / std::tanh(softness * 1.15f);
                    break;
                }
            }

            samples[sample] = clipped;
            postClipPeak = juce::jmax(postClipPeak, std::abs(clipped));
        }
    }

    oversampling->processSamplesDown(block);

    outputGain.setGainDecibels(outputParam->load());
    outputGain.process(context);
    dryWetMixer.mixWetSamples(block);

    auto outputPeak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
        outputPeak = juce::jmax(outputPeak, buffer.getMagnitude(channel, 0, numSamples));
    currentOutputLevelDb.store(outputPeak > 0.0f ? juce::Decibels::gainToDecibels(outputPeak) : -100.0f);

    auto reductionDb = 0.0f;
    if (preClipPeak > 0.0f && postClipPeak > 0.0f)
        reductionDb = juce::jmax(0.0f, juce::Decibels::gainToDecibels(preClipPeak) - juce::Decibels::gainToDecibels(postClipPeak));
    currentClipReductionDb.store(reductionDb);
}

juce::AudioProcessorEditor* CreationStationClipperProcessor::createEditor()
{
    return new CreationStationClipperEditor(*this);
}

void CreationStationClipperProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationClipperProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationClipperProcessor();
}
