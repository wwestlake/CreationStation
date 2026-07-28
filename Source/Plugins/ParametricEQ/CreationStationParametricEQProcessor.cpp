#include "CreationStationParametricEQProcessor.h"
#include "CreationStationParametricEQEditor.h"

namespace cs::plugins
{
namespace
{
constexpr auto masterBypassId = "masterBypass";
constexpr auto outputTrimId = "outputTrim";
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationParametricEQProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::StringArray typeChoices { "Bell", "Low Shelf", "High Shelf", "Low-Pass", "High-Pass" };

    // A sensible starting curve (low shelf / bell / bell / high shelf, spread across the
    // spectrum) even though every band can be freely retyped/repositioned afterward.
    constexpr int defaultTypeIndex[numBands] = { 1, 0, 0, 2 };
    constexpr float defaultFrequency[numBands] = { 100.0f, 500.0f, 2000.0f, 8000.0f };

    for (int band = 0; band < numBands; ++band)
    {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { typeParamId(band), 1 }, "Band " + juce::String(band + 1) + " Type",
            typeChoices, defaultTypeIndex[band]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { freqParamId(band), 1 }, "Band " + juce::String(band + 1) + " Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), defaultFrequency[band],
            juce::AudioParameterFloatAttributes().withLabel("Hz")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { gainParamId(band), 1 }, "Band " + juce::String(band + 1) + " Gain",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { qParamId(band), 1 }, "Band " + juce::String(band + 1) + " Q",
            juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.4f), 0.7071f));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { bypassParamId(band), 1 }, "Band " + juce::String(band + 1) + " Bypass", false));
    }

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { masterBypassId, 1 }, "Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { outputTrimId, 1 }, "Output Trim",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return { params.begin(), params.end() };
}

CreationStationParametricEQProcessor::CreationStationParametricEQProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

CreationStationParametricEQProcessor::~CreationStationParametricEQProcessor()
{
}

void CreationStationParametricEQProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    analyzerBuffer.setSize(1, analyzerFftSize, false, false, true);
    analyzerBuffer.clear();
    analyzerWritePosition = 0;
    analyzerBufferWrapped = false;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getMainBusNumOutputChannels();

    for (auto& filter : filters)
        filter.prepare(spec);

    // updateBandCoefficients() calls getNextValue() once per processBlock() call, not once per
    // sample, so the smoother's rate needs to be the block rate, not the sample rate, for the ramp
    // time below to correspond to real time regardless of the host's buffer size.
    auto blockRate = sampleRate / (double) juce::jmax(1, samplesPerBlock);
    constexpr double rampSeconds = 0.03;

    for (int band = 0; band < numBands; ++band)
    {
        auto& smoother = smoothers[(size_t) band];
        smoother.freq.reset(blockRate, rampSeconds);
        smoother.gain.reset(blockRate, rampSeconds);
        smoother.q.reset(blockRate, rampSeconds);

        smoother.freq.setCurrentAndTargetValue(apvts.getRawParameterValue(freqParamId(band))->load());
        smoother.gain.setCurrentAndTargetValue(apvts.getRawParameterValue(gainParamId(band))->load());
        smoother.q.setCurrentAndTargetValue(apvts.getRawParameterValue(qParamId(band))->load());
    }

    // Force every band's coefficients to rebuild against the new sample rate.
    cachedState.fill({});
    for (int band = 0; band < numBands; ++band)
        updateBandCoefficients(band);
}

void CreationStationParametricEQProcessor::releaseResources()
{
    for (auto& filter : filters)
        filter.reset();
}

void CreationStationParametricEQProcessor::pushAnalyzerSamples(const juce::AudioBuffer<float>& buffer)
{
    const juce::SpinLock::ScopedTryLockType lock(analyzerLock);
    if (! lock.isLocked() || analyzerBuffer.getNumSamples() <= 0 || buffer.getNumSamples() <= 0)
        return;

    const auto numChannels = juce::jmax(1, juce::jmin(buffer.getNumChannels(), getTotalNumOutputChannels()));
    auto* destination = analyzerBuffer.getWritePointer(0);
    const auto totalSamples = analyzerBuffer.getNumSamples();

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float mixedSample = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
            mixedSample += buffer.getSample(channel, sample);

        destination[analyzerWritePosition] = mixedSample / (float) numChannels;
        analyzerWritePosition = (analyzerWritePosition + 1) % totalSamples;
        if (analyzerWritePosition == 0)
            analyzerBufferWrapped = true;
    }
}

void CreationStationParametricEQProcessor::updateBandCoefficients(int band)
{
    auto type = (int) apvts.getRawParameterValue(typeParamId(band))->load();

    auto& smoother = smoothers[(size_t) band];
    smoother.freq.setTargetValue(apvts.getRawParameterValue(freqParamId(band))->load());
    smoother.gain.setTargetValue(apvts.getRawParameterValue(gainParamId(band))->load());
    smoother.q.setTargetValue(apvts.getRawParameterValue(qParamId(band))->load());

    auto freq = smoother.freq.getNextValue();
    auto gainDb = smoother.gain.getNextValue();
    auto q = smoother.q.getNextValue();

    auto& cache = cachedState[(size_t) band];
    if (cache.type == type && cache.freq == freq && cache.gain == gainDb && cache.q == q)
        return;

    cache = { type, freq, gainDb, q };

    // Keep the cutoff comfortably below Nyquist regardless of what the parameter itself allows,
    // since the coefficient factories misbehave right at/above sampleRate/2.
    auto safeFreq = juce::jlimit(20.0f, (float) (currentSampleRate * 0.49), freq);
    auto gainFactor = juce::Decibels::decibelsToGain(gainDb);

    juce::dsp::IIR::Coefficients<float>::Ptr coefficients;

    switch (static_cast<BandType>(type))
    {
        case BandType::bell:
            coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, safeFreq, q, gainFactor);
            break;
        case BandType::lowShelf:
            coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(currentSampleRate, safeFreq, q, gainFactor);
            break;
        case BandType::highShelf:
            coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(currentSampleRate, safeFreq, q, gainFactor);
            break;
        case BandType::lowPass:
            coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, safeFreq, q);
            break;
        case BandType::highPass:
            coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, safeFreq, q);
            break;
    }

    currentCoefficients[(size_t) band] = coefficients;

    // ProcessorDuplicator constructs each per-channel IIR::Filter at prepare() time with its own
    // copy of the *pointer* that was in .state then - reassigning .state afterward (as opposed to
    // mutating the Coefficients object it already points to) never reaches those already-built
    // filters, so they'd stay stuck on prepare()'s original (silent, all-zero) default forever.
    *filters[(size_t) band].state = *coefficients;
}

void CreationStationParametricEQProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    if (apvts.getRawParameterValue(masterBypassId)->load() > 0.5f)
        return;

    for (int band = 0; band < numBands; ++band)
        updateBandCoefficients(band);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    for (int band = 0; band < numBands; ++band)
        if (apvts.getRawParameterValue(bypassParamId(band))->load() <= 0.5f)
            filters[(size_t) band].process(context);

    buffer.applyGain(juce::Decibels::decibelsToGain(apvts.getRawParameterValue(outputTrimId)->load()));
    pushAnalyzerSamples(buffer);
}

juce::AudioProcessorEditor* CreationStationParametricEQProcessor::createEditor()
{
    return new CreationStationParametricEQEditor(*this);
}

void CreationStationParametricEQProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationParametricEQProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::dsp::IIR::Coefficients<float>::Ptr CreationStationParametricEQProcessor::getBandCoefficients(int band) const
{
    if (! juce::isPositiveAndBelow(band, numBands))
        return nullptr;

    if (apvts.getRawParameterValue(bypassParamId(band))->load() > 0.5f)
        return nullptr;

    return currentCoefficients[(size_t) band];
}

double CreationStationParametricEQProcessor::getBandFrequency(int band) const
{
    if (! juce::isPositiveAndBelow(band, numBands))
        return 1000.0;

    return (double) apvts.getRawParameterValue(freqParamId(band))->load();
}

float CreationStationParametricEQProcessor::getBandGainDb(int band) const
{
    if (! juce::isPositiveAndBelow(band, numBands))
        return 0.0f;

    return apvts.getRawParameterValue(gainParamId(band))->load();
}

CreationStationParametricEQProcessor::BandType CreationStationParametricEQProcessor::getBandType(int band) const
{
    if (! juce::isPositiveAndBelow(band, numBands))
        return BandType::bell;

    return static_cast<BandType>((int) apvts.getRawParameterValue(typeParamId(band))->load());
}

bool CreationStationParametricEQProcessor::copyAnalyzerBuffer(juce::AudioBuffer<float>& destination) const
{
    const juce::SpinLock::ScopedTryLockType lock(analyzerLock);
    if (! lock.isLocked() || analyzerBuffer.getNumSamples() <= 0)
        return false;

    destination.setSize(1, analyzerBuffer.getNumSamples(), false, false, true);
    auto* source = analyzerBuffer.getReadPointer(0);
    auto* writePointer = destination.getWritePointer(0);
    const auto totalSamples = analyzerBuffer.getNumSamples();

    if (! analyzerBufferWrapped)
    {
        juce::FloatVectorOperations::copy(writePointer, source, totalSamples);
        if (analyzerWritePosition < totalSamples)
            juce::FloatVectorOperations::clear(writePointer + analyzerWritePosition, totalSamples - analyzerWritePosition);
        return analyzerWritePosition > 0;
    }

    auto tailSamples = totalSamples - analyzerWritePosition;
    juce::FloatVectorOperations::copy(writePointer, source + analyzerWritePosition, tailSamples);
    juce::FloatVectorOperations::copy(writePointer + tailSamples, source, analyzerWritePosition);
    return true;
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationParametricEQProcessor();
}
