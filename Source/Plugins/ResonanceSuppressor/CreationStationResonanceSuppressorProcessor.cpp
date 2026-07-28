#include "CreationStationResonanceSuppressorProcessor.h"
#include "CreationStationResonanceSuppressorEditor.h"

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

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationResonanceSuppressorProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    constexpr float defaultFreq[numBands] = { 250.0f, 900.0f, 3200.0f, 7600.0f };
    constexpr float defaultDepth[numBands] = { 4.0f, 5.0f, 6.0f, 5.0f };
    constexpr float defaultThreshold[numBands] = { -20.0f, -22.0f, -24.0f, -26.0f };
    constexpr float defaultQ[numBands] = { 4.0f, 5.0f, 6.0f, 7.0f };

    for (int band = 0; band < numBands; ++band)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { freqParamId(band), 1 }, "Band " + juce::String(band + 1) + " Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), defaultFreq[band],
            juce::AudioParameterFloatAttributes().withLabel("Hz")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { depthParamId(band), 1 }, "Band " + juce::String(band + 1) + " Depth",
            juce::NormalisableRange<float>(0.0f, 18.0f, 0.1f), defaultDepth[band],
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { thresholdParamId(band), 1 }, "Band " + juce::String(band + 1) + " Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), defaultThreshold[band],
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { qParamId(band), 1 }, "Band " + juce::String(band + 1) + " Q",
            juce::NormalisableRange<float>(1.0f, 18.0f, 0.01f, 0.35f), defaultQ[band],
            juce::AudioParameterFloatAttributes().withLabel("Q")));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { bypassParamId(band), 1 }, "Band " + juce::String(band + 1) + " Bypass", false));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { attackId, 1 }, "Attack",
        juce::NormalisableRange<float>(1.0f, 120.0f, 0.1f, 0.35f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { releaseId, 1 }, "Release",
        juce::NormalisableRange<float>(10.0f, 1600.0f, 1.0f, 0.35f), 180.0f,
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

CreationStationResonanceSuppressorProcessor::CreationStationResonanceSuppressorProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

CreationStationResonanceSuppressorProcessor::~CreationStationResonanceSuppressorProcessor() = default;

void CreationStationResonanceSuppressorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
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

    dryWetMixer.prepare(spec);
    outputGain.prepare(spec);
    currentAttackMs = -1.0f;
    currentReleaseMs = -1.0f;
    updateBallistics();

    for (int band = 0; band < numBands; ++band)
    {
        auto& runtime = bands[(size_t) band];
        runtime.filter.prepare(spec);
        runtime.detector.reset();
        runtime.detector.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        runtime.currentDynamicGainDb = 0.0f;
        runtime.detectorEnvelopeDb = -100.0f;
        runtime.lastFreq = -1.0f;
        runtime.lastQ = -1.0f;
        updateDetectorFilter(band);
        updateBandCoefficients(band);
    }
}

void CreationStationResonanceSuppressorProcessor::releaseResources()
{
    for (auto& band : bands)
    {
        band.filter.reset();
        band.detector.reset();
    }
}

bool CreationStationResonanceSuppressorProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void CreationStationResonanceSuppressorProcessor::updateBallistics()
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

void CreationStationResonanceSuppressorProcessor::updateDetectorFilter(int bandIndex)
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

void CreationStationResonanceSuppressorProcessor::updateBandCoefficients(int bandIndex)
{
    auto freq = apvts.getRawParameterValue(freqParamId(bandIndex))->load();
    auto q = apvts.getRawParameterValue(qParamId(bandIndex))->load();
    auto totalGainDb = bands[(size_t) bandIndex].currentDynamicGainDb;
    auto safeFreq = juce::jlimit(20.0f, (float) (currentSampleRate * 0.49), freq);
    auto gainFactor = juce::Decibels::decibelsToGain(totalGainDb);

    auto coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, safeFreq, q, gainFactor);
    bands[(size_t) bandIndex].coefficients = coefficients;
    *bands[(size_t) bandIndex].filter.state = *coefficients;
}

void CreationStationResonanceSuppressorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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
    auto channelCount = juce::jmax(1, buffer.getNumChannels());

    for (int bandIndex = 0; bandIndex < numBands; ++bandIndex)
    {
        auto& band = bands[(size_t) bandIndex];
        updateDetectorFilter(bandIndex);

        auto thresholdDb = apvts.getRawParameterValue(thresholdParamId(bandIndex))->load();
        auto depthDb = apvts.getRawParameterValue(depthParamId(bandIndex))->load();
        auto bypassed = apvts.getRawParameterValue(bypassParamId(bandIndex))->load() > 0.5f;

        if (bypassed)
        {
            band.currentDynamicGainDb *= 0.82f;
            updateBandCoefficients(bandIndex);
            continue;
        }

        auto peakDetector = 0.0f;
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

        auto overDb = juce::jmax(0.0f, band.detectorEnvelopeDb - thresholdDb);
        auto proportion = juce::jlimit(0.0f, 1.0f, overDb / 24.0f);
        auto targetDynamicGainDb = -depthDb * proportion;

        auto coeff = std::abs(targetDynamicGainDb) > std::abs(band.currentDynamicGainDb) ? band.attackAlpha : band.releaseAlpha;
        band.currentDynamicGainDb = coeff * band.currentDynamicGainDb + (1.0f - coeff) * targetDynamicGainDb;
        updateBandCoefficients(bandIndex);
    }

    juce::dsp::ProcessContextReplacing<float> context(block);
    for (int bandIndex = 0; bandIndex < numBands; ++bandIndex)
        if (apvts.getRawParameterValue(bypassParamId(bandIndex))->load() <= 0.5f)
            bands[(size_t) bandIndex].filter.process(context);

    outputGain.process(context);
    dryWetMixer.mixWetSamples(block);
    pushAnalyzerSamples(buffer);
}

void CreationStationResonanceSuppressorProcessor::pushAnalyzerSamples(const juce::AudioBuffer<float>& buffer)
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

juce::AudioProcessorEditor* CreationStationResonanceSuppressorProcessor::createEditor()
{
    return new CreationStationResonanceSuppressorEditor(*this);
}

void CreationStationResonanceSuppressorProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationResonanceSuppressorProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::dsp::IIR::Coefficients<float>::Ptr CreationStationResonanceSuppressorProcessor::getBandCoefficients(int band) const
{
    if (! juce::isPositiveAndBelow(band, numBands))
        return nullptr;

    if (apvts.getRawParameterValue(bypassParamId(band))->load() > 0.5f)
        return nullptr;

    return bands[(size_t) band].coefficients;
}

double CreationStationResonanceSuppressorProcessor::getBandFrequency(int band) const
{
    return juce::isPositiveAndBelow(band, numBands) ? apvts.getRawParameterValue(freqParamId(band))->load() : 1000.0;
}

float CreationStationResonanceSuppressorProcessor::getBandDepthDb(int band) const
{
    return juce::isPositiveAndBelow(band, numBands) ? apvts.getRawParameterValue(depthParamId(band))->load() : 0.0f;
}

float CreationStationResonanceSuppressorProcessor::getBandDynamicGainDb(int band) const
{
    return juce::isPositiveAndBelow(band, numBands) ? bands[(size_t) band].currentDynamicGainDb : 0.0f;
}

bool CreationStationResonanceSuppressorProcessor::copyAnalyzerBuffer(juce::AudioBuffer<float>& destination) const
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
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationResonanceSuppressorProcessor();
}
