#include "PatchRuntimePlayer.h"

#include <cmath>

namespace
{
float clamp01Runtime(float value)
{
    return juce::jlimit(0.0f, 1.0f, value);
}

float applyBrightnessFilterRuntime(float input, float& state, float brightness, double sampleRate)
{
    auto cutoffHz = juce::jmap(juce::jlimit(0.02f, 1.0f, brightness), 180.0f, 12000.0f);
    auto alpha = juce::jlimit(0.001f, 0.99f, (float) (juce::MathConstants<double>::twoPi * cutoffHz / sampleRate));
    state += alpha * (input - state);
    return state;
}

float getNumericProperty(const juce::NamedValueSet& properties, const juce::Identifier& key, float fallback)
{
    return (float) properties.getWithDefault(key, fallback);
}
}

void PatchRuntimePlayer::prepare(double newSampleRate, int newMaximumBlockSize)
{
    sampleRate = juce::jmax(1.0, newSampleRate);
    maximumBlockSize = juce::jmax(1, newMaximumBlockSize);
}

void PatchRuntimePlayer::reset()
{
}

const cw::PatchAutomationLane* PatchRuntimePlayer::findLane(const cw::PatchDocument& patch, const juce::String& targetParameter)
{
    for (const auto& lane : patch.automationLanes)
        if (lane.targetParameter == targetParameter || lane.id == targetParameter)
            return &lane;

    return nullptr;
}

const cw::PatchNode* PatchRuntimePlayer::findNode(const cw::PatchDocument& patch, const juce::String& kind)
{
    for (const auto& node : patch.nodes)
        if (node.kind == kind)
            return &node;

    return nullptr;
}

float PatchRuntimePlayer::sampleAutomation(const cw::PatchAutomationLane* lane, float t)
{
    if (lane == nullptr || lane->points.isEmpty())
        return 0.5f;

    if (lane->points.size() == 1)
        return (float) lane->points.getFirst().value;

    auto clampedT = clamp01Runtime(t);

    for (int index = 0; index < lane->points.size() - 1; ++index)
    {
        const auto& left = lane->points.getReference(index);
        const auto& right = lane->points.getReference(index + 1);

        if (clampedT >= (float) left.time && clampedT <= (float) right.time)
        {
            auto localRange = juce::jmax(0.0001f, (float) right.time - (float) left.time);
            auto localT = (clampedT - (float) left.time) / localRange;
            return juce::jmap(localT, (float) left.value, (float) right.value);
        }
    }

    return (float) lane->points.getLast().value;
}

bool PatchRuntimePlayer::renderPatchToBuffer(const cw::PatchDocument& patch,
                                             double durationSeconds,
                                             juce::AudioBuffer<float>& destination,
                                             juce::String& errorMessage) const
{
    if (patch.type != "instrument")
    {
        errorMessage = "This runtime currently renders instrument patches only.";
        return false;
    }

    auto safeDuration = juce::jmax(0.05, durationSeconds);
    auto numSamples = juce::jmax(1, juce::roundToInt(safeDuration * sampleRate));
    destination.setSize(2, numSamples, false, false, true);
    destination.clear();

    double baseFrequency = 180.0;
    float brightness = 0.72f;
    for (const auto& parameter : patch.parameters)
    {
        if (parameter.id == "baseFrequency")
            baseFrequency = parameter.defaultValue;
        else if (parameter.id == "brightness")
            brightness = (float) parameter.defaultValue;
    }

    float outputGain = (float) patch.output.gain;

    auto* pitchLane = findLane(patch, "pitchOffsetSemitones");
    auto* gainLane = findLane(patch, "outputGain");
    auto* envelopeNode = findNode(patch, "envelope");

    auto attackPosition = envelopeNode != nullptr ? getNumericProperty(envelopeNode->properties, "attackPosition", 0.12f) : 0.12f;
    auto sustainPosition = envelopeNode != nullptr ? getNumericProperty(envelopeNode->properties, "sustainPosition", 0.42f) : 0.42f;
    auto releasePosition = envelopeNode != nullptr ? getNumericProperty(envelopeNode->properties, "releasePosition", 0.82f) : 0.82f;
    auto sustainLevel = envelopeNode != nullptr ? getNumericProperty(envelopeNode->properties, "sustainLevel", 0.48f) : 0.48f;
    auto releaseStart = juce::jmax(sustainPosition + 0.01f, releasePosition);

    double totalSourceLevel = 0.0;
    for (const auto& source : patch.sources)
        totalSourceLevel += source.level;

    auto normalizer = totalSourceLevel > 0.0 ? (0.9f / (float) totalSourceLevel) : 0.0f;
    float filterState = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto t = (float) sample / (float) juce::jmax(1, numSamples - 1);
        auto pitchMotion = sampleAutomation(pitchLane, t);
        auto gainMotion = sampleAutomation(gainLane, t);
        auto pitchSemitones = juce::jmap(pitchMotion, 0.0f, 1.0f, -12.0f, 12.0f);
        auto frequency = baseFrequency * std::pow(2.0, pitchSemitones / 12.0);
        auto phase = juce::MathConstants<double>::twoPi * frequency * ((double) sample / sampleRate);

        float mixed = 0.0f;
        for (const auto& source : patch.sources)
        {
            float sourceSample = 0.0f;
            if (source.kind == "oscillator")
            {
                if (source.waveform == "sine")
                    sourceSample = (float) std::sin(phase);
                else if (source.waveform == "saw")
                    sourceSample = 2.0f * ((float) (phase / juce::MathConstants<double>::twoPi) - std::floor(0.5f + (float) (phase / juce::MathConstants<double>::twoPi)));
                else if (source.waveform == "square")
                    sourceSample = std::sin(phase) >= 0.0 ? 1.0f : -1.0f;
                else if (source.waveform == "triangle")
                    sourceSample = std::asin(std::sin(phase)) * (2.0f / juce::MathConstants<float>::pi);
            }
            else if (source.kind == "noise")
            {
                auto hashed = std::sin((float) sample * 12.9898f + 78.233f) * 43758.5453f;
                sourceSample = 2.0f * (hashed - std::floor(hashed)) - 1.0f;
            }

            mixed += sourceSample * (float) source.level;
        }

        float envelope = 0.0f;
        if (t <= attackPosition)
            envelope = juce::jmap(t, 0.0f, juce::jmax(0.001f, attackPosition), 0.0f, 1.0f);
        else if (t <= sustainPosition)
            envelope = juce::jmap(t, attackPosition, juce::jmax(attackPosition + 0.001f, sustainPosition), 1.0f, sustainLevel);
        else if (t <= releaseStart)
            envelope = sustainLevel;
        else
            envelope = juce::jmap(t, releaseStart, 1.0f, sustainLevel, 0.0f);

        auto value = normalizer * envelope * gainMotion * outputGain * mixed;
        value = applyBrightnessFilterRuntime(value, filterState, brightness, sampleRate);
        destination.setSample(0, sample, value);
        destination.setSample(1, sample, value);
    }

    return true;
}
