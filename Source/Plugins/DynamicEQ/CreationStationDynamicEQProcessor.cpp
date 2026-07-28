#include "CreationStationDynamicEQProcessor.h"
#include "CreationStationDynamicEQEditor.h"

namespace cs::plugins
{
namespace
{
constexpr auto attackId = "attack";
constexpr auto releaseId = "release";
constexpr auto mixId = "mix";
constexpr auto outputTrimId = "outputTrim";
constexpr auto masterBypassId = "masterBypass";
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationDynamicEQProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    constexpr int defaultType[numBands] = { 1, 0, 2 };
    constexpr float defaultFreq[numBands] = { 120.0f, 1500.0f, 7200.0f };
    constexpr float defaultQ[numBands] = { 0.75f, 1.2f, 0.75f };

    juce::StringArray typeChoices { "Bell", "Low Shelf", "High Shelf" };
    juce::StringArray modeChoices { "Cut", "Boost" };

    for (int band = 0; band < numBands; ++band)
    {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { typeParamId(band), 1 }, "Band " + juce::String(band + 1) + " Type",
            typeChoices, defaultType[band]));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { modeParamId(band), 1 }, "Band " + juce::String(band + 1) + " Mode",
            modeChoices, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { freqParamId(band), 1 }, "Band " + juce::String(band + 1) + " Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), defaultFreq[band],
            juce::AudioParameterFloatAttributes().withLabel("Hz")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { gainParamId(band), 1 }, "Band " + juce::String(band + 1) + " Gain",
            juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { rangeParamId(band), 1 }, "Band " + juce::String(band + 1) + " Range",
            juce::NormalisableRange<float>(0.0f, 18.0f, 0.1f), 6.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { thresholdParamId(band), 1 }, "Band " + juce::String(band + 1) + " Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -24.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { qParamId(band), 1 }, "Band " + juce::String(band + 1) + " Q",
            juce::NormalisableRange<float>(0.2f, 8.0f, 0.01f, 0.4f), defaultQ[band],
            juce::AudioParameterFloatAttributes().withLabel("Q")));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { bypassParamId(band), 1 }, "Band " + juce::String(band + 1) + " Bypass", false));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { attackId, 1 }, "Attack",
        juce::NormalisableRange<float>(1.0f, 200.0f, 0.1f, 0.35f), 18.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { releaseId, 1 }, "Release",
        juce::NormalisableRange<float>(10.0f, 1200.0f, 1.0f, 0.35f), 180.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { mixId, 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { outputTrimId, 1 }, "Output Trim",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { masterBypassId, 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

CreationStationDynamicEQProcessor::CreationStationDynamicEQProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

CreationStationDynamicEQProcessor::~CreationStationDynamicEQProcessor() = default;

void CreationStationDynamicEQProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    analyzerBuffer.setSize(1, analyzerFftSize, false, false, true);
    analyzerBuffer.clear();
    analyzerWritePosition = 0;
    analyzerBufferWrapped = false;
    dryBuffer.setSize(2, samplesPerBlock, false, false, true);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getMainBusNumOutputChannels();

    dryWetMixer.prepare(spec);
    outputGain.prepare(spec);
    currentAttackMs = -1.0f;
    currentReleaseMs = -1.0f;
    updateBallistics();

    for (int band = 0; band < numBands; ++band)
    {
        bands[(size_t) band].filter.prepare(spec);
        bands[(size_t) band].detector.reset();
        bands[(size_t) band].detector.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        bands[(size_t) band].currentDynamicGainDb = 0.0f;
        bands[(size_t) band].detectorEnvelopeDb = -100.0f;
        bands[(size_t) band].lastFreq = -1.0f;
        bands[(size_t) band].lastQ = -1.0f;
        updateDetectorFilter(band);
        updateBandCoefficients(band);
    }
}

void CreationStationDynamicEQProcessor::releaseResources()
{
    for (auto& band : bands)
    {
        band.filter.reset();
        band.detector.reset();
    }
}

bool CreationStationDynamicEQProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void CreationStationDynamicEQProcessor::updateBallistics()
{
    auto attackMs = apvts.getRawParameterValue(attackId)->load();
    auto releaseMs = apvts.getRawParameterValue(releaseId)->load();

    if (juce::approximatelyEqual(attackMs, currentAttackMs) && juce::approximatelyEqual(releaseMs, currentReleaseMs))
        return;

    currentAttackMs = attackMs;
    currentReleaseMs = releaseMs;

    auto attackSeconds = juce::jmax(0.001f, attackMs * 0.001f);
    auto releaseSeconds = juce::jmax(0.001f, releaseMs * 0.001f);
    auto attackAlpha = std::exp(-1.0f / (attackSeconds * (float) currentSampleRate));
    auto releaseAlpha = std::exp(-1.0f / (releaseSeconds * (float) currentSampleRate));

    for (auto& band : bands)
    {
        band.attackAlpha = attackAlpha;
        band.releaseAlpha = releaseAlpha;
    }
}

void CreationStationDynamicEQProcessor::updateDetectorFilter(int bandIndex)
{
    if (! juce::isPositiveAndBelow(bandIndex, numBands))
        return;

    auto& band = bands[(size_t) bandIndex];
    auto freq = apvts.getRawParameterValue(freqParamId(bandIndex))->load();
    auto q = apvts.getRawParameterValue(qParamId(bandIndex))->load();

    if (juce::approximatelyEqual(freq, band.lastFreq) && juce::approximatelyEqual(q, band.lastQ))
        return;

    band.lastFreq = freq;
    band.lastQ = q;
    band.detector.setCutoffFrequency(juce::jlimit(20.0f, (float) (currentSampleRate * 0.45), freq));
    band.detector.setResonance(q);
}

void CreationStationDynamicEQProcessor::updateBandCoefficients(int bandIndex)
{
    auto type = (int) apvts.getRawParameterValue(typeParamId(bandIndex))->load();
    auto freq = apvts.getRawParameterValue(freqParamId(bandIndex))->load();
    auto q = apvts.getRawParameterValue(qParamId(bandIndex))->load();
    auto baseGainDb = apvts.getRawParameterValue(gainParamId(bandIndex))->load();
    auto totalGainDb = baseGainDb + bands[(size_t) bandIndex].currentDynamicGainDb;
    auto safeFreq = juce::jlimit(20.0f, (float) (currentSampleRate * 0.49), freq);
    auto gainFactor = juce::Decibels::decibelsToGain(totalGainDb);

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
    }

    bands[(size_t) bandIndex].coefficients = coefficients;
    *bands[(size_t) bandIndex].filter.state = *coefficients;
}

void CreationStationDynamicEQProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    if (apvts.getRawParameterValue(masterBypassId)->load() > 0.5f)
        return;

    updateBallistics();
    outputGain.setGainDecibels(apvts.getRawParameterValue(outputTrimId)->load());
    dryWetMixer.setWetMixProportion(apvts.getRawParameterValue(mixId)->load() * 0.01f);

    juce::dsp::AudioBlock<float> block(buffer);
    dryWetMixer.pushDrySamples(block);

    auto numSamples = buffer.getNumSamples();
    for (int bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        auto& band = bands[(size_t) bandIndex];
        updateDetectorFilter(bandIndex);

        auto thresholdDb = apvts.getRawParameterValue(thresholdParamId(bandIndex))->load();
        auto rangeDb = apvts.getRawParameterValue(rangeParamId(bandIndex))->load();
        auto mode = (int) apvts.getRawParameterValue(modeParamId(bandIndex))->load();
        auto bypassed = apvts.getRawParameterValue(bypassParamId(bandIndex))->load() > 0.5f;

        if (bypassed)
        {
            band.currentDynamicGainDb *= 0.85f;
            updateBandCoefficients(bandIndex);
            continue;
        }

        auto peakDetector = 0.0f;
        auto channelCount = juce::jmax(1, buffer.getNumChannels());
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float mono = 0.0f;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                mono += buffer.getSample(channel, sample);
            mono /= static_cast<float>(channelCount);

            auto filtered = band.detector.processSample(0, mono);
            peakDetector = juce::jmax(peakDetector, std::abs(filtered));
        }

        auto detectorDb = peakDetector > 0.0f ? juce::Decibels::gainToDecibels(peakDetector) : -100.0f;
        band.detectorEnvelopeDb = detectorDb > band.detectorEnvelopeDb
                                ? band.attackAlpha * band.detectorEnvelopeDb + (1.0f - band.attackAlpha) * detectorDb
                                : band.releaseAlpha * band.detectorEnvelopeDb + (1.0f - band.releaseAlpha) * detectorDb;

        float targetDynamicGainDb = 0.0f;
        if (mode == (int) BandMode::cut)
        {
            auto overDb = juce::jmax(0.0f, band.detectorEnvelopeDb - thresholdDb);
            auto proportion = juce::jlimit(0.0f, 1.0f, overDb / 24.0f);
            targetDynamicGainDb = -rangeDb * proportion;
        }
        else
        {
            auto underDb = juce::jmax(0.0f, thresholdDb - band.detectorEnvelopeDb);
            auto proportion = juce::jlimit(0.0f, 1.0f, underDb / 24.0f);
            targetDynamicGainDb = rangeDb * proportion;
        }

        auto coeff = std::abs(targetDynamicGainDb) > std::abs(band.currentDynamicGainDb) ? band.attackAlpha : band.releaseAlpha;
        band.currentDynamicGainDb = coeff * band.currentDynamicGainDb + (1.0f - coeff) * targetDynamicGainDb;
        updateBandCoefficients(bandIndex);
    }

    juce::dsp::ProcessContextReplacing<float> context(block);
    for (int bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        if (apvts.getRawParameterValue(bypassParamId(bandIndex))->load() <= 0.5f)
            bands[(size_t) bandIndex].filter.process(context);
    }

    outputGain.process(context);
    dryWetMixer.mixWetSamples(block);
    pushAnalyzerSamples(buffer);
}

void CreationStationDynamicEQProcessor::pushAnalyzerSamples(const juce::AudioBuffer<float>& buffer)
{
    const juce::SpinLock::ScopedTryLockType lock(analyzerLock);
    if (! lock.isLocked() || analyzerBuffer.getNumSamples() <= 0 || buffer.getNumSamples() <= 0)
        return;

    auto* destination = analyzerBuffer.getWritePointer(0);
    auto totalSamples = analyzerBuffer.getNumSamples();
    auto numChannels = juce::jmax(1, buffer.getNumChannels());

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float mixed = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
            mixed += buffer.getSample(channel, sample);

        destination[analyzerWritePosition] = mixed / (float) numChannels;
        analyzerWritePosition = (analyzerWritePosition + 1) % totalSamples;
        if (analyzerWritePosition == 0)
            analyzerBufferWrapped = true;
    }
}

juce::AudioProcessorEditor* CreationStationDynamicEQProcessor::createEditor()
{
    return new CreationStationDynamicEQEditor(*this);
}

void CreationStationDynamicEQProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationDynamicEQProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::dsp::IIR::Coefficients<float>::Ptr CreationStationDynamicEQProcessor::getBandCoefficients(int band) const
{
    if (! juce::isPositiveAndBelow(band, numBands))
        return nullptr;

    if (apvts.getRawParameterValue(bypassParamId(band))->load() > 0.5f)
        return nullptr;

    return bands[(size_t) band].coefficients;
}

double CreationStationDynamicEQProcessor::getBandFrequency(int band) const
{
    return juce::isPositiveAndBelow(band, numBands) ? apvts.getRawParameterValue(freqParamId(band))->load() : 1000.0;
}

float CreationStationDynamicEQProcessor::getBandGainDb(int band) const
{
    if (! juce::isPositiveAndBelow(band, numBands))
        return 0.0f;

    return apvts.getRawParameterValue(gainParamId(band))->load() + bands[(size_t) band].currentDynamicGainDb;
}

CreationStationDynamicEQProcessor::BandType CreationStationDynamicEQProcessor::getBandType(int band) const
{
    if (! juce::isPositiveAndBelow(band, numBands))
        return BandType::bell;

    return static_cast<BandType>((int) apvts.getRawParameterValue(typeParamId(band))->load());
}

bool CreationStationDynamicEQProcessor::copyAnalyzerBuffer(juce::AudioBuffer<float>& destination) const
{
    const juce::SpinLock::ScopedTryLockType lock(analyzerLock);
    if (! lock.isLocked() || analyzerBuffer.getNumSamples() <= 0)
        return false;

    destination.setSize(1, analyzerBuffer.getNumSamples(), false, false, true);
    auto* source = analyzerBuffer.getReadPointer(0);
    auto* writePointer = destination.getWritePointer(0);
    auto totalSamples = analyzerBuffer.getNumSamples();

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

float CreationStationDynamicEQProcessor::getBandDynamicGainDb(int band) const
{
    return juce::isPositiveAndBelow(band, numBands) ? bands[(size_t) band].currentDynamicGainDb : 0.0f;
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationDynamicEQProcessor();
}
