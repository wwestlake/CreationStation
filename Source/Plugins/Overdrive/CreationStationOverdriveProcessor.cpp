#include "CreationStationOverdriveProcessor.h"
#include "CreationStationOverdriveEditor.h"

namespace cs::plugins
{

namespace
{
constexpr auto driveId = "drive";
constexpr auto toneId = "tone";
constexpr auto outputId = "output";
constexpr auto mixId = "mix";
constexpr auto typeId = "type";
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationOverdriveProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { typeId, 1 }, "Type",
        juce::StringArray { "Soft", "Hard", "Tube" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { driveId, 1 }, "Drive",
        juce::NormalisableRange<float>(0.0f, 40.0f, 0.1f), 12.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { toneId, 1 }, "Tone",
        juce::NormalisableRange<float>(300.0f, 20000.0f, 1.0f, 0.3f), 8000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

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

CreationStationOverdriveProcessor::CreationStationOverdriveProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    typeParam = parameters.getRawParameterValue(typeId);
    driveParam = parameters.getRawParameterValue(driveId);
    toneParam = parameters.getRawParameterValue(toneId);
    outputParam = parameters.getRawParameterValue(outputId);
    mixParam = parameters.getRawParameterValue(mixId);
}

CreationStationOverdriveProcessor::~CreationStationOverdriveProcessor() = default;

void CreationStationOverdriveProcessor::updateWaveshaperFunction(int typeIndex)
{
    if (typeIndex == lastCurveType)
        return;

    lastCurveType = typeIndex;

    switch (static_cast<CurveType>(typeIndex))
    {
        case CurveType::soft:
            waveshaper.functionToUse = [](float x) { return std::tanh(x); };
            break;
        case CurveType::hard:
            waveshaper.functionToUse = [](float x) { return juce::jlimit(-1.0f, 1.0f, x); };
            break;
        case CurveType::tube:
            // Asymmetric soft clip - positive/negative halves saturate differently, producing
            // the even-harmonic-rich character associated with tube-style overdrive.
            waveshaper.functionToUse = [](float x)
            {
                return x >= 0.0f ? std::tanh(x) : std::tanh(x * 1.6f) * 0.75f;
            };
            break;
    }
}

void CreationStationOverdriveProcessor::updateToneCoefficients(float toneHz)
{
    if (juce::approximatelyEqual(toneHz, lastToneHz))
        return;

    lastToneHz = toneHz;
    auto safeHz = juce::jlimit(20.0f, (float) (currentSampleRate * 0.49), toneHz);
    *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, safeHz);
}

void CreationStationOverdriveProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
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
    toneFilter.prepare(spec);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing((size_t) samplesPerBlock);
    dryWetMixer.setWetLatency(oversampling->getLatencyInSamples());
    setLatencySamples((int) std::ceil(oversampling->getLatencyInSamples()));

    toneSmoother.reset(sampleRate / (double) juce::jmax(1, samplesPerBlock), 0.03);
    toneSmoother.setCurrentAndTargetValue(toneParam->load());

    lastCurveType = -1;
    lastToneHz = -1.0f;
    updateWaveshaperFunction((int) typeParam->load());
    updateToneCoefficients(toneSmoother.getCurrentValue());
}

void CreationStationOverdriveProcessor::releaseResources()
{
    if (oversampling != nullptr)
        oversampling->reset();
}

bool CreationStationOverdriveProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void CreationStationOverdriveProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    updateWaveshaperFunction((int) typeParam->load());
    toneSmoother.setTargetValue(toneParam->load());
    updateToneCoefficients(toneSmoother.getNextValue());

    dryWetMixer.setWetMixProportion(mixParam->load() * 0.01f);

    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);

    driveGain.setGainDecibels(driveParam->load());
    juce::dsp::ProcessContextReplacing<float> context(block);
    driveGain.process(context);

    auto oversampledBlock = oversampling->processSamplesUp(block);
    juce::dsp::ProcessContextReplacing<float> oversampledContext(oversampledBlock);
    waveshaper.process(oversampledContext);
    oversampling->processSamplesDown(block);

    toneFilter.process(context);

    outputGain.setGainDecibels(outputParam->load());
    outputGain.process(context);

    dryWetMixer.mixWetSamples(block);

    auto outputPeak = 0.0f;
    for (int channel = 0; channel < numChannels; ++channel)
        outputPeak = juce::jmax(outputPeak, buffer.getMagnitude(channel, 0, numSamples));
    currentOutputLevelDb.store(outputPeak > 0.0f ? juce::Decibels::gainToDecibels(outputPeak) : -100.0f);
}

juce::AudioProcessorEditor* CreationStationOverdriveProcessor::createEditor()
{
    return new CreationStationOverdriveEditor(*this);
}

void CreationStationOverdriveProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationOverdriveProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationOverdriveProcessor();
}
