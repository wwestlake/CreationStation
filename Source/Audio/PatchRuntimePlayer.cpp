#include "PatchRuntimePlayer.h"

#include <cmath>

namespace
{
constexpr float kMinFilterCutoffHz = 40.0f;
constexpr float kMaxFilterCutoffHz = 16000.0f;

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

float cutoffToNormalized(float cutoffHz)
{
    auto safeCutoff = juce::jlimit(kMinFilterCutoffHz, kMaxFilterCutoffHz, cutoffHz);
    auto logMin = std::log(kMinFilterCutoffHz);
    auto logMax = std::log(kMaxFilterCutoffHz);
    return clamp01Runtime((std::log(safeCutoff) - logMin) / juce::jmax(0.0001f, logMax - logMin));
}

float normalizedToCutoff(float normalized)
{
    auto clamped = clamp01Runtime(normalized);
    auto logMin = std::log(kMinFilterCutoffHz);
    auto logMax = std::log(kMaxFilterCutoffHz);
    return std::exp(logMin + (logMax - logMin) * clamped);
}

float applyCurveModeRuntime(float localT, const juce::String& curveMode)
{
    auto t = clamp01Runtime(localT);
    if (curveMode == "stepped")
        return t < 1.0f ? 0.0f : 1.0f;
    if (curveMode == "smooth")
        return t * t * (3.0f - 2.0f * t);
    return t;
}

double sampleAutomationPoints(const juce::Array<cw::PatchAutomationPoint>& points,
                              const juce::String& interpolation,
                              double t,
                              double fallbackValue)
{
    if (points.isEmpty())
        return fallbackValue;
    if (points.size() == 1)
        return points.getFirst().value;

    auto clampedT = juce::jlimit(0.0, 1.0, t);
    for (int index = 0; index < points.size() - 1; ++index)
    {
        const auto& left = points.getReference(index);
        const auto& right = points.getReference(index + 1);
        if (clampedT >= left.time && clampedT <= right.time)
        {
            auto localRange = juce::jmax(0.0001, right.time - left.time);
            auto localT = (clampedT - left.time) / localRange;
            auto curve = right.curve.isNotEmpty() ? right.curve : interpolation;
            return juce::jmap((double) applyCurveModeRuntime((float) localT, curve), left.value, right.value);
        }
    }

    return points.getLast().value;
}

double sampleTargetLanesRuntime(const juce::Array<cw::PatchAutomationLane>& lanes,
                                const juce::String& targetParameter,
                                double t,
                                double fallbackValue)
{
    double sum = 0.0;
    int count = 0;
    for (const auto& lane : lanes)
    {
        if (lane.targetParameter == targetParameter)
        {
            auto interpolation = lane.interpolation.isNotEmpty() ? lane.interpolation : juce::String("linear");
            sum += sampleAutomationPoints(lane.points, interpolation, t, fallbackValue);
            ++count;
        }
    }

    return count > 0 ? (sum / (double) count) : fallbackValue;
}

juce::Array<cw::PatchAutomationPoint> parseEnvelopePointsJson(const juce::String& jsonText, const juce::String& curveMode)
{
    juce::Array<cw::PatchAutomationPoint> points;
    auto parsed = juce::JSON::parse(jsonText);
    auto* values = parsed.getArray();
    if (values == nullptr)
        return points;

    for (const auto& value : *values)
    {
        if (auto* object = value.getDynamicObject())
        {
            cw::PatchAutomationPoint point;
            point.time = (double) object->getProperty("time");
            point.value = (double) object->getProperty("value");
            point.curve = object->getProperty("curve").toString();
            if (point.curve.isEmpty())
                point.curve = curveMode;
            points.add(point);
        }
    }

    return points;
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

const cw::PatchNode* PatchRuntimePlayer::findNode(const cw::PatchDocument& patch, const juce::String& kind)
{
    for (const auto& node : patch.nodes)
        if (node.kind == kind)
            return &node;

    return nullptr;
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
    float filterCutoffHz = 3600.0f;
    float filterResonance = 0.90f;
    float filterEnvelopeAmount = 0.35f;
    float macroHardness = 0.50f;
    float macroWeight = 0.50f;
    float macroAir = 0.50f;
    float macroGrit = 0.25f;
    float macroSize = 0.50f;
    for (const auto& parameter : patch.parameters)
    {
        if (parameter.id == "baseFrequency")
            baseFrequency = parameter.defaultValue;
        else if (parameter.id == "filterCutoff")
            filterCutoffHz = (float) parameter.defaultValue;
        else if (parameter.id == "filterResonance")
            filterResonance = (float) parameter.defaultValue;
        else if (parameter.id == "filterEnvelopeAmount")
            filterEnvelopeAmount = (float) parameter.defaultValue;
        else if (parameter.id == "macroHardness")
            macroHardness = (float) parameter.defaultValue;
        else if (parameter.id == "macroWeight")
            macroWeight = (float) parameter.defaultValue;
        else if (parameter.id == "macroAir")
            macroAir = (float) parameter.defaultValue;
        else if (parameter.id == "macroGrit")
            macroGrit = (float) parameter.defaultValue;
        else if (parameter.id == "macroSize")
            macroSize = (float) parameter.defaultValue;
    }

    float outputGain = (float) patch.output.gain;

    auto* envelopeNode = findNode(patch, "envelope");
    auto* filterNode = findNode(patch, "filter");
    auto filterMode = filterNode != nullptr ? filterNode->properties.getWithDefault("mode", juce::String("lowpass")).toString()
                                            : juce::String("lowpass");
    if (filterNode != nullptr)
    {
        filterCutoffHz = getNumericProperty(filterNode->properties, "cutoffHz", filterCutoffHz);
        filterResonance = getNumericProperty(filterNode->properties, "resonance", filterResonance);
        filterEnvelopeAmount = getNumericProperty(filterNode->properties, "envelopeAmount", filterEnvelopeAmount);
    }
    juce::Array<cw::PatchAutomationPoint> envelopePoints;
    juce::String envelopeCurveMode { "linear" };
    if (envelopeNode != nullptr)
    {
        envelopeCurveMode = envelopeNode->properties.getWithDefault("curveMode", juce::String("linear")).toString();
        auto pointsJson = envelopeNode->properties.getWithDefault("pointsJson", {}).toString();
        if (pointsJson.isNotEmpty())
            envelopePoints = parseEnvelopePointsJson(pointsJson, envelopeCurveMode);

        if (envelopePoints.isEmpty())
        {
            auto attackPosition = getNumericProperty(envelopeNode->properties, "attackPosition", 0.12f);
            auto sustainPosition = getNumericProperty(envelopeNode->properties, "sustainPosition", 0.42f);
            auto releasePosition = getNumericProperty(envelopeNode->properties, "releasePosition", 0.82f);
            auto sustainLevel = getNumericProperty(envelopeNode->properties, "sustainLevel", 0.48f);
            envelopePoints.add({ 0.0, 0.0, envelopeCurveMode });
            envelopePoints.add({ attackPosition, 1.0, envelopeCurveMode });
            envelopePoints.add({ sustainPosition, sustainLevel, envelopeCurveMode });
            envelopePoints.add({ releasePosition, sustainLevel, envelopeCurveMode });
            envelopePoints.add({ 1.0, 0.0, envelopeCurveMode });
        }
    }

    if (envelopePoints.isEmpty())
    {
        envelopePoints.add({ 0.0, 0.0, envelopeCurveMode });
        envelopePoints.add({ 0.12, 1.0, envelopeCurveMode });
        envelopePoints.add({ 0.42, 0.48, envelopeCurveMode });
        envelopePoints.add({ 0.82, 0.48, envelopeCurveMode });
        envelopePoints.add({ 1.0, 0.0, envelopeCurveMode });
    }

    for (int index = 1; index < envelopePoints.size() - 1; ++index)
    {
        double timeShift = index == 1
                         ? juce::jmap((double) macroHardness, 0.0, 1.0, 0.05, -0.05)
                         : juce::jmap((double) macroSize, 0.0, 1.0, -0.04, 0.08);
        envelopePoints.getReference(index).time += timeShift;
        envelopePoints.getReference(index).value = juce::jlimit(0.0,
                                                                1.0,
                                                                envelopePoints.getReference(index).value
                                                                    + juce::jmap((double) macroWeight, 0.0, 1.0, -0.08, 0.12));
    }

    double totalSourceLevel = 0.0;
    for (const auto& source : patch.sources)
        totalSourceLevel += source.level;

    auto normalizer = totalSourceLevel > 0.0 ? (0.9f / (float) totalSourceLevel) : 0.0f;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (uint32) juce::jmax(1, maximumBlockSize);
    spec.numChannels = 1;
    filter.prepare(spec);
    filter.reset();
    filter.setType(filterMode == "highpass" ? juce::dsp::StateVariableTPTFilterType::highpass
                                            : filterMode == "bandpass" ? juce::dsp::StateVariableTPTFilterType::bandpass
                                                                       : juce::dsp::StateVariableTPTFilterType::lowpass);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto t = (float) sample / (float) juce::jmax(1, numSamples - 1);
        auto pitchMotion = sampleTargetLanesRuntime(patch.automationLanes, "pitchOffsetSemitones", t, 0.0);
        auto gainMotion = sampleTargetLanesRuntime(patch.automationLanes, "outputGain", t, 1.0);
        auto filterMotion = sampleTargetLanesRuntime(patch.automationLanes, "filterCutoff", t, 0.5);
        auto resonanceMotion = sampleTargetLanesRuntime(patch.automationLanes, "filterResonance", t, filterResonance);
        auto noiseMotion = sampleTargetLanesRuntime(patch.automationLanes, "noiseLevel", t, 0.0);
        auto baseFrequencyMotion = sampleTargetLanesRuntime(patch.automationLanes, "baseFrequency", t, baseFrequency);
        auto pitchSemitones = (float) pitchMotion
                            + juce::jmap(macroWeight, 0.0f, 1.0f, 2.0f, -2.0f) * (t - 0.5f) * 2.0f;
        auto weightedBaseFrequency = baseFrequencyMotion * (double) juce::jmap(macroWeight, 0.0f, 1.0f, 1.16f, 0.86f);
        auto frequency = weightedBaseFrequency * std::pow(2.0, pitchSemitones / 12.0);
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

        auto envelope = (float) sampleAutomationPoints(envelopePoints, envelopeCurveMode, t, 0.0);

        auto gritDrive = 1.0f + macroGrit * 5.5f;
        auto macroNoise = juce::jlimit(0.0f, 1.0f, (float) noiseMotion + macroAir * 0.18f + macroGrit * 0.12f);
        mixed += macroNoise * ((std::sin((float) sample * 12.9898f + 78.233f) * 43758.5453f) - std::floor(std::sin((float) sample * 12.9898f + 78.233f) * 43758.5453f));
        auto value = normalizer * envelope * (float) gainMotion * outputGain * mixed;
        value = std::tanh(value * gritDrive) / std::tanh(gritDrive);
        auto filterNormalized = clamp01Runtime(cutoffToNormalized(filterCutoffHz)
                                               + ((float) filterMotion - 0.5f) * 0.75f
                                               + (envelope - 0.5f) * (filterEnvelopeAmount + macroHardness * 0.30f)
                                               + macroAir * 0.18f
                                               - macroWeight * 0.10f);
        auto cutoffHz = normalizedToCutoff(filterNormalized);
        filter.setCutoffFrequency(juce::jlimit(kMinFilterCutoffHz, (float) (sampleRate * 0.45), cutoffHz));
        filter.setResonance(juce::jlimit(0.30f, 8.0f, (float) resonanceMotion + macroHardness * 0.9f - macroWeight * 0.15f));
        value = filter.processSample(0, value);
        destination.setSample(0, sample, value);
        destination.setSample(1, sample, value);
    }

    return true;
}
