#include "CreationStationVocalFocusProcessor.h"
#include "CreationStationVocalFocusEditor.h"

namespace cs::plugins
{
namespace
{
constexpr auto modeId = "mode";
constexpr auto amountId = "amount";
constexpr auto lowCutId = "lowCut";
constexpr auto highCutId = "highCut";
constexpr auto bleedId = "bleed";
constexpr auto mixId = "mix";
constexpr auto outputId = "output";
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationVocalFocusProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { modeId, 1 }, "Mode",
        juce::StringArray { "Reduce Center", "Isolate Center" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { amountId, 1 }, "Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 70.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { lowCutId, 1 }, "Low Cut",
        juce::NormalisableRange<float>(60.0f, 800.0f, 1.0f, 0.35f), 140.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { highCutId, 1 }, "High Cut",
        juce::NormalisableRange<float>(1000.0f, 12000.0f, 1.0f, 0.35f), 6500.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { bleedId, 1 }, "Side Bleed",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 15.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { mixId, 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { outputId, 1 }, "Output",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return { params.begin(), params.end() };
}

CreationStationVocalFocusProcessor::CreationStationVocalFocusProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    modeParam = parameters.getRawParameterValue(modeId);
    amountParam = parameters.getRawParameterValue(amountId);
    lowCutParam = parameters.getRawParameterValue(lowCutId);
    highCutParam = parameters.getRawParameterValue(highCutId);
    bleedParam = parameters.getRawParameterValue(bleedId);
    mixParam = parameters.getRawParameterValue(mixId);
    outputParam = parameters.getRawParameterValue(outputId);
}

CreationStationVocalFocusProcessor::~CreationStationVocalFocusProcessor() = default;

void CreationStationVocalFocusProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    highPassFilter.reset();
    lowPassFilter.reset();
    lastLowCutHz = -1.0f;
    lastHighCutHz = -1.0f;
    updateFilterCoefficients(lowCutParam->load(), highCutParam->load());
}

void CreationStationVocalFocusProcessor::releaseResources()
{
    highPassFilter.reset();
    lowPassFilter.reset();
}

bool CreationStationVocalFocusProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void CreationStationVocalFocusProcessor::updateFilterCoefficients(float lowCutHz, float highCutHz)
{
    auto safeLowCut = juce::jlimit(20.0f, (float) (currentSampleRate * 0.45), lowCutHz);
    auto safeHighCut = juce::jlimit(safeLowCut + 100.0f, (float) (currentSampleRate * 0.49), highCutHz);

    if (juce::approximatelyEqual(safeLowCut, lastLowCutHz) && juce::approximatelyEqual(safeHighCut, lastHighCutHz))
        return;

    lastLowCutHz = safeLowCut;
    lastHighCutHz = safeHighCut;

    highPassFilter.setCoefficients(juce::IIRCoefficients::makeHighPass(currentSampleRate, safeLowCut));
    lowPassFilter.setCoefficients(juce::IIRCoefficients::makeLowPass(currentSampleRate, safeHighCut));
}

float CreationStationVocalFocusProcessor::processFocusedBand(float sample)
{
    return lowPassFilter.processSingleSampleRaw(highPassFilter.processSingleSampleRaw(sample));
}

void CreationStationVocalFocusProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    auto numChannels = buffer.getNumChannels();
    auto numSamples = buffer.getNumSamples();

    auto mode = static_cast<Mode>((int) modeParam->load());
    auto amount = juce::jlimit(0.0f, 1.0f, amountParam->load() * 0.01f);
    auto sideBleed = juce::jlimit(0.0f, 1.0f, bleedParam->load() * 0.01f);
    auto mix = juce::jlimit(0.0f, 1.0f, mixParam->load() * 0.01f);
    auto outputGain = juce::Decibels::decibelsToGain(outputParam->load());

    updateFilterCoefficients(lowCutParam->load(), highCutParam->load());

    if (numChannels < 2)
    {
        auto* mono = buffer.getWritePointer(0);
        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
        {
            auto dry = mono[sampleIndex];
            auto focused = processFocusedBand(dry);
            auto wet = mode == Mode::reduceCenter
                           ? dry - (focused * amount)
                           : juce::jmap(amount, dry, focused * 1.25f);
            mono[sampleIndex] = (dry * (1.0f - mix) + wet * mix) * outputGain;
        }
        return;
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        auto dryLeft = left[sampleIndex];
        auto dryRight = right[sampleIndex];

        auto mid = 0.5f * (dryLeft + dryRight);
        auto side = 0.5f * (dryLeft - dryRight);
        auto focusedMid = processFocusedBand(mid);

        float wetMid = mid;
        float wetSide = side;

        if (mode == Mode::reduceCenter)
        {
            wetMid = mid - (focusedMid * amount);
            // Let the user widen the remainder a bit so removing center vocal doesn't collapse
            // the backing track.
            wetSide = side * (1.0f + (amount * 0.35f));
        }
        else
        {
            // Pull toward the center-focused band while letting a controlled amount of side
            // content survive, so it feels more like isolation than hard mono cancellation.
            wetMid = juce::jmap(amount, mid, focusedMid * 1.35f);
            wetSide = side * sideBleed;
        }

        auto wetLeft = wetMid + wetSide;
        auto wetRight = wetMid - wetSide;

        left[sampleIndex] = (dryLeft * (1.0f - mix) + wetLeft * mix) * outputGain;
        right[sampleIndex] = (dryRight * (1.0f - mix) + wetRight * mix) * outputGain;
    }
}

juce::AudioProcessorEditor* CreationStationVocalFocusProcessor::createEditor()
{
    return new CreationStationVocalFocusEditor(*this);
}

void CreationStationVocalFocusProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void CreationStationVocalFocusProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationVocalFocusProcessor();
}
