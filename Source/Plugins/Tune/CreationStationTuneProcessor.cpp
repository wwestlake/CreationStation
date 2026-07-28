#include "CreationStationTuneProcessor.h"
#include "CreationStationTuneEditor.h"
#include "../../Audio/SamplePackBuilder/PitchDetector.h"
#include <cmath>

namespace cs::plugins
{
namespace
{
constexpr auto modeId = "mode";
constexpr auto referenceId = "reference";
constexpr auto keyId = "key";
constexpr auto scaleId = "scale";
constexpr auto speedId = "speed";
constexpr auto strengthId = "strength";
constexpr auto mixId = "mix";
constexpr auto octaveProtectId = "octaveProtect";

juce::String allowedNoteParamId(int noteIndex)
{
    return "allowedNote" + juce::String(noteIndex);
}

int wrapPitchClass(int pitchClass)
{
    pitchClass %= 12;
    if (pitchClass < 0)
        pitchClass += 12;
    return pitchClass;
}

double midiToFrequency(double midiValue, double referenceHz)
{
    return referenceHz * std::pow(2.0, (midiValue - 69.0) / 12.0);
}

double frequencyToMidi(double frequencyHz, double referenceHz)
{
    if (frequencyHz <= 0.0 || referenceHz <= 0.0)
        return 69.0;

    return 69.0 + 12.0 * std::log2(frequencyHz / referenceHz);
}
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationTuneProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { modeId, 1 }, "Mode",
        juce::StringArray { "Tuner", "Auto", "Manual" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { referenceId, 1 }, "Reference",
        juce::NormalisableRange<float>(432.0f, 448.0f, 0.1f), 440.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { keyId, 1 }, "Key",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { scaleId, 1 }, "Scale",
        juce::StringArray { "Chromatic", "Major", "Minor", "Dorian", "Phrygian", "Lydian",
                            "Mixolydian", "Locrian", "Arabian", "Egyptian" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { speedId, 1 }, "Speed",
        juce::NormalisableRange<float>(5.0f, 180.0f, 1.0f, 0.35f), 35.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { strengthId, 1 }, "Strength",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.85f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { mixId, 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { octaveProtectId, 1 }, "Octave Protect", true));

    for (int noteIndex = 0; noteIndex < 12; ++noteIndex)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { allowedNoteParamId(noteIndex), 1 },
            "Allow " + noteNameForMidi((double) noteIndex), true));
    }

    return { params.begin(), params.end() };
}

CreationStationTuneProcessor::CreationStationTuneProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
    modeParam = parameters.getRawParameterValue(modeId);
    referenceParam = parameters.getRawParameterValue(referenceId);
    keyParam = parameters.getRawParameterValue(keyId);
    scaleParam = parameters.getRawParameterValue(scaleId);
    speedParam = parameters.getRawParameterValue(speedId);
    strengthParam = parameters.getRawParameterValue(strengthId);
    mixParam = parameters.getRawParameterValue(mixId);
    octaveProtectParam = parameters.getRawParameterValue(octaveProtectId);

    for (int noteIndex = 0; noteIndex < 12; ++noteIndex)
        allowedNoteParams[(size_t) noteIndex] = parameters.getRawParameterValue(allowedNoteParamId(noteIndex));
}

CreationStationTuneProcessor::~CreationStationTuneProcessor() = default;

void CreationStationTuneProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    dryBuffer.setSize(2, samplesPerBlock, false, false, true);
    shifter.prepare(sampleRate, samplesPerBlock, 2);
    shifter.setSmoothingTime(sampleRate, 0.035);
    detectorWindow.fill(0.0f);
    detectorWritePosition = 0;
    detectorWrapped = false;
    samplesSinceLastAnalysis = 0;
    detectedCentsDisplay.store(0.0f);
    appliedCorrectionCentsDisplay.store(0.0f);
    detectionConfidenceDisplay.store(0.0f);
}

void CreationStationTuneProcessor::releaseResources()
{
    shifter.reset();
}

bool CreationStationTuneProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CreationStationTuneProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels < 2)
        return;

    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    for (int channel = 0; channel < numChannels; ++channel)
        dryBuffer.copyFrom(channel, 0, buffer, channel, 0, numSamples);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto mono = 0.5f * (buffer.getSample(0, sample) + buffer.getSample(1, sample));
        pushDetectorSample(mono);
    }

    samplesSinceLastAnalysis += numSamples;
    if (samplesSinceLastAnalysis >= 256)
    {
        samplesSinceLastAnalysis = 0;
        analysePitchWindow();
    }

    auto mode = static_cast<Mode>((int) modeParam->load());
    currentModeDisplay.store((int) mode);

    auto desiredRatio = 1.0;
    if (mode == Mode::automatic && detectionConfidenceDisplay.load() > 0.25f)
    {
        auto appliedSemitones = appliedCorrectionCentsDisplay.load() / 100.0f;
        desiredRatio = std::pow(2.0, (double) appliedSemitones / 12.0);
    }

    shifter.setSmoothingTime(currentSampleRate, juce::jmax(0.005, (double) speedParam->load() * 0.001));
    shifter.setTargetRatio(desiredRatio);

    if (mode != Mode::automatic || mixParam->load() <= 0.0f || detectionConfidenceDisplay.load() <= 0.25f)
        return;

    shifter.processBlock(buffer);

    auto wet = juce::jlimit(0.0f, 1.0f, mixParam->load());
    auto dry = 1.0f - wet;
    for (int channel = 0; channel < numChannels; ++channel)
    {
        buffer.applyGain(channel, 0, numSamples, wet);
        buffer.addFrom(channel, 0, dryBuffer, channel, 0, numSamples, dry);
    }
}

juce::AudioProcessorEditor* CreationStationTuneProcessor::createEditor()
{
    return new CreationStationTuneEditor(*this);
}

void CreationStationTuneProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationTuneProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

void CreationStationTuneProcessor::copyPitchHistory(std::array<float, displayHistorySize>& destination, int& validPoints) const
{
    const juce::SpinLock::ScopedLockType lock(historyLock);
    destination.fill(0.0f);
    validPoints = historyCount;

    if (historyCount <= 0)
        return;

    auto start = historyCount == displayHistorySize ? historyWritePosition : 0;
    for (int index = 0; index < historyCount; ++index)
        destination[(size_t) index] = pitchHistory[(size_t) ((start + index) % displayHistorySize)];
}

juce::String CreationStationTuneProcessor::noteNameForMidi(double midiValue)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    auto rounded = (int) std::llround(midiValue);
    auto pitchClass = wrapPitchClass(rounded);
    auto octave = (int) std::floor((midiValue / 12.0) - 1.0 + 0.5);
    return juce::String(names[pitchClass]) + juce::String(octave);
}

void CreationStationTuneProcessor::DelayPitchShifter::prepare(double sampleRate, int maxBlockSize, int numChannels)
{
    juce::ignoreUnused(sampleRate);
    windowSize = juce::jlimit(256, 2048, juce::jmax(512, maxBlockSize * 2));
    baseDelay = windowSize;
    delayBufferSize = baseDelay + windowSize * 2 + maxBlockSize + 16;
    delayBuffer.setSize(numChannels, delayBufferSize, false, false, true);
    reset();
}

void CreationStationTuneProcessor::DelayPitchShifter::reset()
{
    delayBuffer.clear();
    writePosition = 0;
    phase = 0.0;
    ratio.setCurrentAndTargetValue(1.0);
}

void CreationStationTuneProcessor::DelayPitchShifter::setSmoothingTime(double sampleRate, double seconds)
{
    if (std::abs(lastSmoothingSeconds - seconds) < 1.0e-4)
        return;

    lastSmoothingSeconds = seconds;
    ratio.reset(sampleRate, seconds);
}

void CreationStationTuneProcessor::DelayPitchShifter::setTargetRatio(double newRatio)
{
    ratio.setTargetValue(juce::jlimit(0.5, 2.0, newRatio));
}

float CreationStationTuneProcessor::DelayPitchShifter::readSample(int channel, double delaySamples) const
{
    auto readPosition = (double) writePosition - delaySamples;
    while (readPosition < 0.0)
        readPosition += (double) delayBufferSize;

    while (readPosition >= (double) delayBufferSize)
        readPosition -= (double) delayBufferSize;

    auto indexA = (int) readPosition;
    auto indexB = (indexA + 1) % delayBufferSize;
    auto frac = (float) (readPosition - (double) indexA);
    auto sampleA = delayBuffer.getSample(channel, indexA);
    auto sampleB = delayBuffer.getSample(channel, indexB);
    return sampleA + (sampleB - sampleA) * frac;
}

void CreationStationTuneProcessor::DelayPitchShifter::processBlock(juce::AudioBuffer<float>& buffer)
{
    auto numChannels = juce::jmin(buffer.getNumChannels(), delayBuffer.getNumChannels());
    auto numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto currentRatio = ratio.getNextValue();
        phase += (1.0 - currentRatio) / (double) windowSize;
        phase -= std::floor(phase);

        auto phaseB = phase + 0.5;
        phaseB -= std::floor(phaseB);

        auto gainA = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * (float) phase);
        auto gainB = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * (float) phaseB);
        auto delayA = (double) baseDelay + phase * (double) windowSize;
        auto delayB = (double) baseDelay + phaseB * (double) windowSize;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto in = buffer.getSample(channel, sample);
            delayBuffer.setSample(channel, writePosition, in);
            auto shifted = readSample(channel, delayA) * gainA
                         + readSample(channel, delayB) * gainB;
            buffer.setSample(channel, sample, shifted);
        }

        writePosition = (writePosition + 1) % delayBufferSize;
    }
}

void CreationStationTuneProcessor::pushDetectorSample(float sample) noexcept
{
    detectorWindow[(size_t) detectorWritePosition] = sample;
    detectorWritePosition = (detectorWritePosition + 1) % (int) detectorWindow.size();
    if (detectorWritePosition == 0)
        detectorWrapped = true;
}

void CreationStationTuneProcessor::analysePitchWindow()
{
    std::array<float, 2048> analysis {};
    auto availableSamples = detectorWrapped ? (int) detectorWindow.size() : detectorWritePosition;
    auto analysisSamples = juce::jmin((int) analysis.size(), availableSamples);
    if (analysisSamples < 512)
        return;

    auto start = detectorWritePosition - analysisSamples;
    if (start < 0)
        start += (int) detectorWindow.size();

    for (int index = 0; index < analysisSamples; ++index)
        analysis[(size_t) index] = detectorWindow[(size_t) ((start + index) % (int) detectorWindow.size())];

    auto detection = PitchDetector::detectPitch(analysis.data(), analysisSamples, currentSampleRate, 70.0, 1200.0);
    detectionConfidenceDisplay.store(detection.confidence);

    if (! detection.detected)
    {
        appliedCorrectionCentsDisplay.store(0.0f);
        pushHistoryPoint(0.0f);
        return;
    }

    auto referenceHz = juce::jmax(1.0, (double) referenceParam->load());
    auto detectedMidi = frequencyToMidi(detection.frequencyHz, referenceHz);

    if (octaveProtectParam->load() > 0.5f)
    {
        while (detectedMidi - lastStableMidi > 6.0)
            detectedMidi -= 12.0;
        while (detectedMidi - lastStableMidi < -6.0)
            detectedMidi += 12.0;
    }

    lastStableMidi = detectedMidi;

    auto rootNote = (int) keyParam->load();
    auto scale = static_cast<Scale>((int) scaleParam->load());
    auto targetMidi = findNearestAllowedMidi(detectedMidi, rootNote, scale);
    auto centsToTarget = (float) ((targetMidi - detectedMidi) * 100.0);
    auto strength = juce::jlimit(0.0f, 1.0f, strengthParam->load());
    auto appliedCents = std::abs(centsToTarget) < 4.0f ? 0.0f : centsToTarget * strength;

    detectedMidiDisplay.store(detectedMidi);
    targetMidiDisplay.store(targetMidi);
    detectedCentsDisplay.store(centsToTarget);
    appliedCorrectionCentsDisplay.store(appliedCents);
    pushHistoryPoint(centsToTarget);
}

double CreationStationTuneProcessor::findNearestAllowedMidi(double detectedMidi, int rootNote, Scale scale) const
{
    auto rounded = (int) std::llround(detectedMidi);
    auto bestMidi = (double) rounded;
    auto bestDistance = std::numeric_limits<double>::max();

    for (int candidate = rounded - 24; candidate <= rounded + 24; ++candidate)
    {
        auto pitchClass = wrapPitchClass(candidate);
        if (! isPitchClassAllowed(pitchClass, rootNote, scale))
            continue;

        auto distance = std::abs((double) candidate - detectedMidi);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestMidi = (double) candidate;
        }
    }

    return bestMidi;
}

bool CreationStationTuneProcessor::isPitchClassAllowed(int pitchClass, int rootNote, Scale scale) const
{
    static constexpr bool chromatic[12]  = { true, true, true, true, true, true, true, true, true, true, true, true };
    static constexpr bool major[12]      = { true, false, true, false, true, true, false, true, false, true, false, true };
    static constexpr bool minor[12]      = { true, false, true, true, false, true, false, true, true, false, true, false };
    static constexpr bool dorian[12]     = { true, false, true, true, false, true, false, true, false, true, true, false };
    static constexpr bool phrygian[12]   = { true, true, false, true, false, true, false, true, true, false, true, false };
    static constexpr bool lydian[12]     = { true, false, true, false, true, false, true, true, false, true, false, true };
    static constexpr bool mixolydian[12] = { true, false, true, false, true, true, false, true, false, true, true, false };
    static constexpr bool locrian[12]    = { true, true, false, true, false, true, true, false, true, false, true, false };
    static constexpr bool arabian[12]    = { true, true, false, false, true, true, false, true, true, false, false, true };
    static constexpr bool egyptian[12]   = { true, false, true, false, false, true, false, true, false, false, true, false };

    const bool* mask = chromatic;
    switch (scale)
    {
        case Scale::major:      mask = major; break;
        case Scale::minor:      mask = minor; break;
        case Scale::dorian:     mask = dorian; break;
        case Scale::phrygian:   mask = phrygian; break;
        case Scale::lydian:     mask = lydian; break;
        case Scale::mixolydian: mask = mixolydian; break;
        case Scale::locrian:    mask = locrian; break;
        case Scale::arabian:    mask = arabian; break;
        case Scale::egyptian:   mask = egyptian; break;
        case Scale::chromatic:
        default:                mask = chromatic; break;
    }

    auto relative = wrapPitchClass(pitchClass - rootNote);
    return mask[relative] && allowedNoteParams[(size_t) pitchClass]->load() > 0.5f;
}

void CreationStationTuneProcessor::pushHistoryPoint(float centsToTarget)
{
    const juce::SpinLock::ScopedTryLockType lock(historyLock);
    if (! lock.isLocked())
        return;

    pitchHistory[(size_t) historyWritePosition] = juce::jlimit(-100.0f, 100.0f, centsToTarget);
    historyWritePosition = (historyWritePosition + 1) % displayHistorySize;
    historyCount = juce::jmin(displayHistorySize, historyCount + 1);
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationTuneProcessor();
}
