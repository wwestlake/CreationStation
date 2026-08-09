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

float applyOnePoleLowPassRuntime(float input, float& state, float cutoffHz, double sampleRate)
{
    auto clampedCutoff = juce::jlimit(20.0f, (float) (sampleRate * 0.45), cutoffHz);
    auto alpha = juce::jlimit(0.0005f, 0.99f, (float) (juce::MathConstants<double>::twoPi * clampedCutoff / sampleRate));
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

namespace
{
enum class TapStage { Source, Mix, Envelope, Filter, Output };
struct TapPlan { juce::String nodeId; TapStage stage; int sourceIndex; };
}

bool PatchRuntimePlayer::renderPatchToBuffer(const cw::PatchDocument& patch,
                                             double durationSeconds,
                                             juce::AudioBuffer<float>& destination,
                                             juce::String& errorMessage,
                                             juce::Array<TapCapture>* taps) const
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

    // Scope/Analyzer nodes show whatever's actually wired into them -- a
    // specific source, the mix bus, post-envelope, or post-filter -- instead
    // of always showing the final output regardless of where they're placed.
    // The pipeline is still fixed-order internally (no arbitrary rewiring),
    // but each of those stage boundaries is a real, distinct value now, so
    // tapping "mix" genuinely differs from tapping "envelope" or "filter".
    juce::Array<TapPlan> tapPlans;
    if (taps != nullptr)
    {
        for (auto& node : patch.nodes)
        {
            if (node.kind != "scope" && node.kind != "analyzer")
                continue;

            TapPlan plan { node.id, TapStage::Output, -1 };
            for (auto& connection : patch.connections)
            {
                if (connection.to != node.id)
                    continue;
                if (connection.from == "mix") plan.stage = TapStage::Mix;
                else if (connection.from == "envelope") plan.stage = TapStage::Envelope;
                else if (connection.from == "filter") plan.stage = TapStage::Filter;
                else if (connection.from == "output") plan.stage = TapStage::Output;
                else
                {
                    for (int sourceIndex = 0; sourceIndex < patch.sources.size(); ++sourceIndex)
                        if (patch.sources.getReference(sourceIndex).id == connection.from)
                        {
                            plan.stage = TapStage::Source;
                            plan.sourceIndex = sourceIndex;
                            break;
                        }
                }
                break;
            }

            tapPlans.add(plan);
            TapCapture capture { node.id, juce::AudioBuffer<float>(1, numSamples) };
            capture.buffer.clear();
            taps->add(std::move(capture));
        }
    }

    double baseFrequency = 180.0;
    float filterCutoffHz = 3600.0f;
    float filterResonance = 0.90f;
    float filterEnvelopeAmount = 0.35f;
    float noiseLevel = 0.10f;
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
        else if (parameter.id == "noiseLevel")
            noiseLevel = (float) parameter.defaultValue;
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
    // buildPatchDocument only ever writes a filter/envelope node into the
    // document if the user actually placed one on the canvas -- so their
    // presence here is the real signal, not just extra config to read.
    // "No Filter node placed" now genuinely means no filtering happens.
    bool hasFilterNode = filterNode != nullptr;
    bool hasEnvelopeNode = envelopeNode != nullptr;
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

    // sampleTargetLanesRuntime already degenerates to "return fallbackValue"
    // when there are no automation lanes targeting a parameter -- but under
    // the Debug config's /Od /RTC1 (MSVC rejects /O2 combined with /RTC1
    // outright, so this file can't just be flagged to optimize), even that
    // no-op body costs real time as an uninlined, RTC-instrumented call.
    // Timeline/automation nodes aren't wired into buildPatchDocument yet,
    // so patch.automationLanes is empty for every graph that exists today
    // -- skip the call entirely in that case (15 calls/sample, ~3.6M calls
    // for a 5s buffer) rather than paying for a no-op every sample. Behavior
    // is identical either way; this only matters once lanes are non-empty.
    bool hasAutomationLanes = ! patch.automationLanes.isEmpty();

    // Each oscillator source can name its own frequency parameter
    // (buildPatchDocument gives every node instance a uniquely-id'd one) so
    // multiple Sine/Saw/etc. nodes stay independent -- resolve each source's
    // base frequency once here rather than per-sample.
    juce::Array<double> sourceBaseFrequencyHz;
    for (const auto& source : patch.sources)
    {
        double freq = baseFrequency;
        if (source.frequencyParameter.isNotEmpty())
            for (const auto& parameter : patch.parameters)
                if (parameter.id == source.frequencyParameter)
                {
                    freq = parameter.defaultValue;
                    break;
                }
        sourceBaseFrequencyHz.add(freq);
    }

    // A source's mix weight comes from the connection actually wiring it
    // into a Mixer channel (buildPatchDocument resolves that from the
    // channel's real weight port). No matching connection -- the common
    // case of a source feeding straight into the bus with no Mixer in
    // between -- just passes through at unity weight, same as before.
    juce::Array<float> sourceMixWeight;
    for (const auto& source : patch.sources)
    {
        float weight = 1.0f;
        for (const auto& connection : patch.connections)
            if (connection.from == source.id)
            {
                weight = (float) connection.weight;
                break;
            }
        sourceMixWeight.add(weight);
    }

    juce::Array<float> sourceTapValue;
    sourceTapValue.insertMultiple(0, 0.0f, patch.sources.size());

    float bodyState = 0.0f;
    float previousEnvelope = 0.0f;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto t = (float) sample / (float) juce::jmax(1, numSamples - 1);
        auto pitchMotion = hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "pitchOffsetSemitones", t, 0.0) : 0.0;
        auto gainMotion = hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "outputGain", t, 1.0) : 1.0;
        auto filterMotion = hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "filterCutoff", t, 0.5) : 0.5;
        auto resonanceMotion = hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "filterResonance", t, filterResonance) : (double) filterResonance;
        auto filterEnvelopeMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "filterEnvelopeAmount", t, filterEnvelopeAmount) : (double) filterEnvelopeAmount);
        auto noiseMotion = hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "noiseLevel", t, noiseLevel) : (double) noiseLevel;
        auto hardnessMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroHardness", t, macroHardness) : (double) macroHardness);
        auto weightMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroWeight", t, macroWeight) : (double) macroWeight);
        auto airMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroAir", t, macroAir) : (double) macroAir);
        auto gritMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroGrit", t, macroGrit) : (double) macroGrit);
        auto sizeMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroSize", t, macroSize) : (double) macroSize);
        auto pitchSemitones = (float) pitchMotion
                            + juce::jmap(weightMotion, 0.0f, 1.0f, 2.0f, -2.0f) * (t - 0.5f) * 2.0f;

        // Same pseudo-random hash used both by the noise source and by the
        // macro grit/air texture layer below -- compute std::sin once and
        // reuse it instead of recomputing the identical value up to three
        // times per sample.
        auto hashed = std::sin((float) sample * 12.9898f + 78.233f) * 43758.5453f;
        auto hashNoise = 2.0f * (hashed - std::floor(hashed)) - 1.0f;

        float mixed = 0.0f;
        for (int sourceIndex = 0; sourceIndex < patch.sources.size(); ++sourceIndex)
        {
            const auto& source = patch.sources.getReference(sourceIndex);
            float sourceSample = 0.0f;
            if (source.kind == "oscillator")
            {
                // Each source carries its own level and (via
                // sourceBaseFrequencyHz, resolved once before this loop from
                // its own uniquely-id'd parameter) its own base frequency --
                // that's what keeps multiple instances of the same waveform
                // independent instead of all sharing one pitch/level.
                auto weightedBaseFrequency = sourceBaseFrequencyHz[sourceIndex]
                                           * (double) juce::jmap(weightMotion, 0.0f, 1.0f, 1.16f, 0.86f)
                                           * (double) juce::jmap(sizeMotion, 0.0f, 1.0f, 1.04f, 0.94f);
                auto frequency = weightedBaseFrequency * std::pow(2.0, pitchSemitones / 12.0);
                auto phase = juce::MathConstants<double>::twoPi * frequency * ((double) sample / sampleRate);

                auto animatedLevel = (float) source.level * sourceMixWeight[sourceIndex];
                if (source.waveform == "sine")
                    sourceSample = (float) std::sin(phase);
                else if (source.waveform == "saw")
                    sourceSample = 2.0f * ((float) (phase / juce::MathConstants<double>::twoPi) - std::floor(0.5f + (float) (phase / juce::MathConstants<double>::twoPi)));
                else if (source.waveform == "square")
                    sourceSample = std::sin(phase) >= 0.0 ? 1.0f : -1.0f;
                else if (source.waveform == "triangle")
                    sourceSample = std::asin(std::sin(phase)) * (2.0f / juce::MathConstants<float>::pi);

                sourceTapValue.set(sourceIndex, sourceSample * animatedLevel);
                mixed += sourceSample * animatedLevel;
                continue;
            }
            else if (source.kind == "noise")
            {
                sourceSample = hashNoise;
                auto noiseContribution = sourceSample * (float) juce::jlimit(0.0, 1.0, source.level) * sourceMixWeight[sourceIndex];
                sourceTapValue.set(sourceIndex, noiseContribution);
                mixed += noiseContribution;
                continue;
            }
        }

        // No Envelope node placed -- play at flat level for the duration
        // (no shaping, no attack transient) instead of computing a curve
        // that was never really configured by anything on the canvas.
        auto envelope = hasEnvelopeNode ? (float) sampleAutomationPoints(envelopePoints, envelopeCurveMode, t, 0.0) : 1.0f;
        auto transient = juce::jmax(0.0f, envelope - previousEnvelope);
        previousEnvelope = envelope;

        auto gritDrive = 1.0f + gritMotion * 5.5f;
        auto macroNoise = juce::jlimit(0.0f, 1.0f, (float) noiseMotion + airMotion * 0.18f + gritMotion * 0.12f);
        mixed += macroNoise * (hashed - std::floor(hashed));
        auto mixStageValue = mixed; // what the Mixer node's output actually is, before any envelope/filter shaping
        auto value = normalizer * envelope * (float) gainMotion * outputGain * mixed;
        value += transient * juce::jmap(hardnessMotion, 0.0f, 1.0f, 0.0f, 0.45f);
        value = std::tanh(value * gritDrive) / std::tanh(gritDrive);
        auto bodyCutoff = juce::jmap(weightMotion, 0.0f, 1.0f, 120.0f, 520.0f);
        auto bodyComponent = applyOnePoleLowPassRuntime(value, bodyState, bodyCutoff, sampleRate);
        value += bodyComponent * juce::jmap(weightMotion, 0.0f, 1.0f, 0.0f, 0.32f);
        value *= juce::jmap(sizeMotion, 0.0f, 1.0f, 0.98f, 1.12f + 0.08f * std::sin(t * juce::MathConstants<float>::pi));
        auto envelopeStageValue = value; // output of the Envelope stage, before filtering

        // No Filter node placed -- pass the signal through unfiltered instead
        // of always running a filter with fallback default settings.
        float filteredValue = value;
        if (hasFilterNode)
        {
            auto envelopeContribution = hasEnvelopeNode ? (envelope - 0.5f) : 0.0f;
            auto filterNormalized = clamp01Runtime(cutoffToNormalized(filterCutoffHz)
                                                   + ((float) filterMotion - 0.5f) * 0.75f
                                                   + envelopeContribution * (filterEnvelopeMotion + hardnessMotion * 0.30f)
                                                   + airMotion * 0.18f
                                                   - weightMotion * 0.10f
                                                   - sizeMotion * 0.06f);
            auto cutoffHz = normalizedToCutoff(filterNormalized);
            filter.setCutoffFrequency(juce::jlimit(kMinFilterCutoffHz, (float) (sampleRate * 0.45), cutoffHz));
            filter.setResonance(juce::jlimit(0.30f, 8.0f, (float) resonanceMotion + hardnessMotion * 0.9f - weightMotion * 0.15f));
            filteredValue = filter.processSample(0, value);
        }
        value = juce::jmap(airMotion, filteredValue, juce::jlimit(-1.0f, 1.0f, value));
        destination.setSample(0, sample, value);
        destination.setSample(1, sample, value);

        if (taps != nullptr)
        {
            for (int tapIndex = 0; tapIndex < tapPlans.size(); ++tapIndex)
            {
                auto& plan = tapPlans.getReference(tapIndex);
                float tapValue = value; // TapStage::Output (and the safe default)
                if (plan.stage == TapStage::Source && plan.sourceIndex >= 0 && plan.sourceIndex < sourceTapValue.size())
                    tapValue = sourceTapValue.getUnchecked(plan.sourceIndex);
                else if (plan.stage == TapStage::Mix)
                    tapValue = mixStageValue;
                else if (plan.stage == TapStage::Envelope)
                    tapValue = envelopeStageValue;
                else if (plan.stage == TapStage::Filter)
                    tapValue = filteredValue;
                taps->getReference(tapIndex).buffer.setSample(0, sample, tapValue);
            }
        }
    }

    return true;
}
