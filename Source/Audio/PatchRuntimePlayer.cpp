#include "PatchRuntimePlayer.h"

#include <cmath>
#include <functional>

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

    double baseFrequency = 180.0;
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

    // Filter/Envelope are real multi-instance entities now (Phase 3, built
    // further down) -- each reads its own mode/cutoff/resonance/curve
    // directly from its own patch.nodes entry. The first Envelope node
    // found here is only used as a shared cross-cutting modulation curve
    // (macro transient-boost, filter cutoff-tracks-envelope) -- see the
    // comment on computeMix() below for why that's a deliberate
    // simplification rather than true per-connection modulation routing.
    auto* envelopeNode = findNode(patch, "envelope");
    bool hasEnvelopeNode = envelopeNode != nullptr;
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

    // --- Step 1: every source computes its own buffer, independently. ---
    // Sources have no inputs, so this can always run first regardless of how
    // the rest of the graph is wired.
    juce::OwnedArray<juce::AudioBuffer<float>> sourceBuffers;
    for (int sourceIndex = 0; sourceIndex < patch.sources.size(); ++sourceIndex)
    {
        auto* buffer = sourceBuffers.add(new juce::AudioBuffer<float>(1, numSamples));
        const auto& source = patch.sources.getReference(sourceIndex);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            auto t = (float) sample / (float) juce::jmax(1, numSamples - 1);
            auto pitchMotion = hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "pitchOffsetSemitones", t, 0.0) : 0.0;
            auto weightMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroWeight", t, macroWeight) : (double) macroWeight);
            auto sizeMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroSize", t, macroSize) : (double) macroSize);
            auto pitchSemitones = (float) pitchMotion + juce::jmap(weightMotion, 0.0f, 1.0f, 2.0f, -2.0f) * (t - 0.5f) * 2.0f;

            float sourceSample = 0.0f;
            if (source.kind == "oscillator")
            {
                // Each source carries its own level and its own base
                // frequency (sourceBaseFrequencyHz, resolved above from its
                // own uniquely-id'd parameter) -- that's what keeps multiple
                // instances of the same waveform independent.
                auto weightedBaseFrequency = sourceBaseFrequencyHz[sourceIndex]
                                           * (double) juce::jmap(weightMotion, 0.0f, 1.0f, 1.16f, 0.86f)
                                           * (double) juce::jmap(sizeMotion, 0.0f, 1.0f, 1.04f, 0.94f);
                auto frequency = weightedBaseFrequency * std::pow(2.0, pitchSemitones / 12.0);
                auto phase = juce::MathConstants<double>::twoPi * frequency * ((double) sample / sampleRate);

                float waveform = 0.0f;
                if (source.waveform == "sine")
                    waveform = (float) std::sin(phase);
                else if (source.waveform == "saw")
                    waveform = 2.0f * ((float) (phase / juce::MathConstants<double>::twoPi) - std::floor(0.5f + (float) (phase / juce::MathConstants<double>::twoPi)));
                else if (source.waveform == "square")
                    waveform = std::sin(phase) >= 0.0 ? 1.0f : -1.0f;
                else if (source.waveform == "triangle")
                    waveform = std::asin(std::sin(phase)) * (2.0f / juce::MathConstants<float>::pi);
                sourceSample = waveform * (float) source.level;
            }
            else if (source.kind == "noise")
            {
                auto hashed = std::sin((float) sample * 12.9898f + 78.233f) * 43758.5453f;
                sourceSample = (2.0f * (hashed - std::floor(hashed)) - 1.0f) * (float) juce::jlimit(0.0, 1.0, source.level);
            }
            buffer->setSample(0, sample, sourceSample);
        }
    }

    // --- Step 2: figure out the shape of the rest of the graph. ---
    // Mix and Output are still singleton (at most one id each). Filter and
    // Envelope are real multi-instance now (Phase 3, matching oscillators) --
    // each one is its own entity with its own settings, processing whatever
    // single buffer is actually wired into it, wherever that is in the
    // graph, instead of one fixed slot each.
    juce::String mixId, outputId;
    for (auto& node : patch.nodes)
    {
        if (node.kind == "mix") mixId = node.id;
        else if (node.kind == "output") outputId = node.id;
    }

    struct FilterEntity
    {
        juce::String id, mode;
        float cutoffHz = 3600.0f, resonance = 0.90f, envelopeAmount = 0.35f;
        juce::AudioBuffer<float> buffer;
        bool ready = false, computing = false;
    };
    struct EnvelopeEntity
    {
        juce::String id, curveMode;
        juce::Array<cw::PatchAutomationPoint> points;
        juce::AudioBuffer<float> buffer;
        bool ready = false, computing = false;
    };

    juce::OwnedArray<FilterEntity> filterEntities;
    juce::OwnedArray<EnvelopeEntity> envelopeEntities;
    for (auto& node : patch.nodes)
    {
        if (node.kind == "filter")
        {
            auto* entity = filterEntities.add(new FilterEntity());
            entity->id = node.id;
            entity->mode = node.properties.getWithDefault("mode", juce::String("lowpass")).toString();
            entity->cutoffHz = getNumericProperty(node.properties, "cutoffHz", entity->cutoffHz);
            entity->resonance = getNumericProperty(node.properties, "resonance", entity->resonance);
            entity->envelopeAmount = getNumericProperty(node.properties, "envelopeAmount", entity->envelopeAmount);
            entity->buffer.setSize(1, numSamples);
            entity->buffer.clear();
        }
        else if (node.kind == "envelope")
        {
            auto* entity = envelopeEntities.add(new EnvelopeEntity());
            entity->id = node.id;
            entity->curveMode = node.properties.getWithDefault("curveMode", juce::String("linear")).toString();
            auto pointsJson = node.properties.getWithDefault("pointsJson", {}).toString();
            if (pointsJson.isNotEmpty())
                entity->points = parseEnvelopePointsJson(pointsJson, entity->curveMode);
            if (entity->points.isEmpty())
                entity->points = envelopePoints; // the default shape already resolved above
            entity->buffer.setSize(1, numSamples);
            entity->buffer.clear();
        }
    }

    auto findFilterEntity = [&](const juce::String& id) -> FilterEntity*
    {
        for (auto* entity : filterEntities) if (entity->id == id) return entity;
        return nullptr;
    };
    auto findEnvelopeEntity = [&](const juce::String& id) -> EnvelopeEntity*
    {
        for (auto* entity : envelopeEntities) if (entity->id == id) return entity;
        return nullptr;
    };
    auto hasOutgoingConnection = [&](const juce::String& id)
    {
        for (auto& connection : patch.connections)
            if (connection.from == id)
                return true;
        return false;
    };

    juce::StringArray sourcesWithOutgoingWire;
    for (auto& connection : patch.connections)
        for (auto& source : patch.sources)
            if (source.id == connection.from && ! sourcesWithOutgoingWire.contains(connection.from))
                sourcesWithOutgoingWire.add(connection.from);

    juce::AudioBuffer<float> mixBuffer(1, numSamples);
    bool mixReady = false, mixComputing = false;

    std::function<const juce::AudioBuffer<float>*()> computeMix;
    std::function<const juce::AudioBuffer<float>*(FilterEntity&)> computeFilter;
    std::function<const juce::AudioBuffer<float>*(EnvelopeEntity&)> computeEnvelope;

    // What feeds a plain single-input node (Filter, Envelope, Output, Scope,
    // Analyzer) -- a specific source, the Mix bus, or another Filter/
    // Envelope's own output if it's wired to feed something downstream of
    // itself. Returns null if nothing is actually wired in.
    auto resolveSingleInput = [&](const juce::String& nodeId) -> const juce::AudioBuffer<float>*
    {
        for (auto& connection : patch.connections)
        {
            if (connection.to != nodeId)
                continue;
            for (int i = 0; i < patch.sources.size(); ++i)
                if (patch.sources.getReference(i).id == connection.from)
                    return sourceBuffers[i];
            if (! mixId.isEmpty() && connection.from == mixId)
                return computeMix();
            if (auto* filterEntity = findFilterEntity(connection.from))
                return computeFilter(*filterEntity);
            if (auto* envelopeEntity = findEnvelopeEntity(connection.from))
                return computeEnvelope(*envelopeEntity);
        }
        return nullptr;
    };

    // Mix: weighted sum of whatever's actually wired into it (or, if no
    // Mix node exists at all, every source that isn't explicitly wired
    // elsewhere -- the same tolerant "just place oscillators, no wiring
    // needed" behavior this always had). Also where the always-on macro
    // character shaping (hardness/weight/air/grit/size, saturation, body
    // resonance) happens -- that isn't tied to a placeable node. The
    // transient-boost component of that shaping, and each Filter's cutoff-
    // tracks-envelope modulation below, both reference "the first Envelope
    // node's curve" as a shared cross-cutting modulation source (envelopePoints/
    // envelopeCurveMode/hasEnvelopeNode, resolved earlier) -- a deliberate
    // simplification for graphs with multiple Envelopes, since true per-
    // connection modulation routing is future work, not this phase's job.
    computeMix = [&]() -> const juce::AudioBuffer<float>*
    {
        if (mixReady) return &mixBuffer;
        if (mixComputing) return nullptr;
        mixComputing = true;
        mixBuffer.clear();

        juce::Array<int> feedIndices;
        juce::Array<float> feedWeights;
        if (! mixId.isEmpty())
        {
            for (auto& connection : patch.connections)
            {
                if (connection.to != mixId) continue;
                for (int i = 0; i < patch.sources.size(); ++i)
                    if (patch.sources.getReference(i).id == connection.from)
                    {
                        feedIndices.add(i);
                        feedWeights.add((float) connection.weight);
                    }
            }
        }
        else
        {
            for (int i = 0; i < patch.sources.size(); ++i)
                if (! sourcesWithOutgoingWire.contains(patch.sources.getReference(i).id))
                {
                    feedIndices.add(i);
                    feedWeights.add(1.0f);
                }
        }

        float bodyState = 0.0f;
        float previousEnvelope = 0.0f;
        for (int sample = 0; sample < numSamples; ++sample)
        {
            auto t = (float) sample / (float) juce::jmax(1, numSamples - 1);
            auto gainMotion = hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "outputGain", t, 1.0) : 1.0;
            auto noiseMotion = hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "noiseLevel", t, noiseLevel) : (double) noiseLevel;
            auto hardnessMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroHardness", t, macroHardness) : (double) macroHardness);
            auto weightMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroWeight", t, macroWeight) : (double) macroWeight);
            auto airMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroAir", t, macroAir) : (double) macroAir);
            auto gritMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroGrit", t, macroGrit) : (double) macroGrit);
            auto sizeMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroSize", t, macroSize) : (double) macroSize);

            auto hashed = std::sin((float) sample * 12.9898f + 78.233f) * 43758.5453f;

            float mixed = 0.0f;
            for (int k = 0; k < feedIndices.size(); ++k)
                mixed += sourceBuffers[feedIndices[k]]->getSample(0, sample) * feedWeights[k];

            // "envelope" here is only the shared curve used for the
            // transient-boost color term below (rate-of-change, not
            // amplitude) -- actual amplitude shaping is no longer applied
            // here. Envelope is a real, independently-wired entity now
            // (computeEnvelope() below); applying it again here on top of
            // whatever real Envelope entities already multiplied in would
            // double it up.
            auto envelope = hasEnvelopeNode ? (float) sampleAutomationPoints(envelopePoints, envelopeCurveMode, t, 0.0) : 1.0f;
            auto transient = juce::jmax(0.0f, envelope - previousEnvelope);
            previousEnvelope = envelope;

            auto gritDrive = 1.0f + gritMotion * 5.5f;
            auto macroNoise = juce::jlimit(0.0f, 1.0f, (float) noiseMotion + airMotion * 0.18f + gritMotion * 0.12f);
            mixed += macroNoise * (hashed - std::floor(hashed));
            auto value = normalizer * (float) gainMotion * outputGain * mixed;
            value += transient * juce::jmap(hardnessMotion, 0.0f, 1.0f, 0.0f, 0.45f);
            value = std::tanh(value * gritDrive) / std::tanh(gritDrive);
            auto bodyCutoff = juce::jmap(weightMotion, 0.0f, 1.0f, 120.0f, 520.0f);
            auto bodyComponent = applyOnePoleLowPassRuntime(value, bodyState, bodyCutoff, sampleRate);
            value += bodyComponent * juce::jmap(weightMotion, 0.0f, 1.0f, 0.0f, 0.32f);
            value *= juce::jmap(sizeMotion, 0.0f, 1.0f, 0.98f, 1.12f + 0.08f * std::sin(t * juce::MathConstants<float>::pi));
            value = juce::jmap(airMotion, value, juce::jlimit(-1.0f, 1.0f, value));

            mixBuffer.setSample(0, sample, value);
        }

        mixReady = true;
        mixComputing = false;
        return &mixBuffer;
    };

    // Envelope: multiplies whatever single buffer is wired into it by its
    // own curve. A real, independently-positionable entity now -- multiple
    // Envelopes can each shape a different point in the graph.
    computeEnvelope = [&](EnvelopeEntity& entity) -> const juce::AudioBuffer<float>*
    {
        if (entity.ready) return &entity.buffer;
        if (entity.computing) return nullptr;
        entity.computing = true;

        auto* inputBuffer = resolveSingleInput(entity.id);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            auto t = (float) sample / (float) juce::jmax(1, numSamples - 1);
            auto envelopeValue = (float) sampleAutomationPoints(entity.points, entity.curveMode, t, 0.0);
            float inputValue = inputBuffer != nullptr ? inputBuffer->getSample(0, sample) : 0.0f;
            entity.buffer.setSample(0, sample, inputValue * envelopeValue);
        }

        entity.ready = true;
        entity.computing = false;
        return &entity.buffer;
    };

    // Filter: processes whatever single buffer is actually wired into it --
    // Mix's output in the common case, but just as validly a raw source or
    // an Envelope's output wired directly, or nothing (silence) if it's
    // placed but unwired.
    computeFilter = [&](FilterEntity& entity) -> const juce::AudioBuffer<float>*
    {
        if (entity.ready) return &entity.buffer;
        if (entity.computing) return nullptr;
        entity.computing = true;

        auto* inputBuffer = resolveSingleInput(entity.id);

        juce::dsp::StateVariableTPTFilter<float> filterDsp;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = (uint32) juce::jmax(1, maximumBlockSize);
        spec.numChannels = 1;
        filterDsp.prepare(spec);
        filterDsp.reset();
        filterDsp.setType(entity.mode == "highpass" ? juce::dsp::StateVariableTPTFilterType::highpass
                                                    : entity.mode == "bandpass" ? juce::dsp::StateVariableTPTFilterType::bandpass
                                                                                : juce::dsp::StateVariableTPTFilterType::lowpass);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            auto t = (float) sample / (float) juce::jmax(1, numSamples - 1);
            auto filterMotion = hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "filterCutoff", t, 0.5) : 0.5;
            auto resonanceMotion = hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "filterResonance", t, entity.resonance) : (double) entity.resonance;
            auto filterEnvelopeMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "filterEnvelopeAmount", t, entity.envelopeAmount) : (double) entity.envelopeAmount);
            auto hardnessMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroHardness", t, macroHardness) : (double) macroHardness);
            auto weightMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroWeight", t, macroWeight) : (double) macroWeight);
            auto airMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroAir", t, macroAir) : (double) macroAir);
            auto sizeMotion = (float) (hasAutomationLanes ? sampleTargetLanesRuntime(patch.automationLanes, "macroSize", t, macroSize) : (double) macroSize);
            auto envelope = hasEnvelopeNode ? (float) sampleAutomationPoints(envelopePoints, envelopeCurveMode, t, 0.0) : 1.0f;

            float inputValue = inputBuffer != nullptr ? inputBuffer->getSample(0, sample) : 0.0f;

            auto envelopeContribution = hasEnvelopeNode ? (envelope - 0.5f) : 0.0f;
            auto filterNormalized = clamp01Runtime(cutoffToNormalized(entity.cutoffHz)
                                                   + ((float) filterMotion - 0.5f) * 0.75f
                                                   + envelopeContribution * (filterEnvelopeMotion + hardnessMotion * 0.30f)
                                                   + airMotion * 0.18f
                                                   - weightMotion * 0.10f
                                                   - sizeMotion * 0.06f);
            auto cutoffHz = normalizedToCutoff(filterNormalized);
            filterDsp.setCutoffFrequency(juce::jlimit(kMinFilterCutoffHz, (float) (sampleRate * 0.45), cutoffHz));
            filterDsp.setResonance(juce::jlimit(0.30f, 8.0f, (float) resonanceMotion + hardnessMotion * 0.9f - weightMotion * 0.15f));
            auto filteredValue = filterDsp.processSample(0, inputValue);
            auto outputValue = juce::jmap(airMotion, filteredValue, juce::jlimit(-1.0f, 1.0f, inputValue));

            entity.buffer.setSample(0, sample, outputValue);
        }

        entity.ready = true;
        entity.computing = false;
        return &entity.buffer;
    };

    // --- Step 3: resolve the final output. ---
    // Explicit wiring into the Sink is honored first. If nothing is wired
    // there yet (e.g. a freshly-placed, not-yet-connected graph), fall back
    // to whatever the natural end of the chain is -- any Filter or Envelope
    // with no outgoing wire of its own, else Mix -- so a minimal "just
    // place a couple oscillators" graph is still audible without forcing
    // the user to wire all the way to Sink first.
    const juce::AudioBuffer<float>* finalBuffer = nullptr;
    if (! outputId.isEmpty())
        finalBuffer = resolveSingleInput(outputId);
    if (finalBuffer == nullptr)
    {
        for (auto* entity : filterEntities)
            if (! hasOutgoingConnection(entity->id)) { finalBuffer = computeFilter(*entity); break; }
    }
    if (finalBuffer == nullptr)
    {
        for (auto* entity : envelopeEntities)
            if (! hasOutgoingConnection(entity->id)) { finalBuffer = computeEnvelope(*entity); break; }
    }
    if (finalBuffer == nullptr)
        finalBuffer = computeMix();

    if (finalBuffer != nullptr)
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            auto value = finalBuffer->getSample(0, sample);
            destination.setSample(0, sample, value);
            destination.setSample(1, sample, value);
        }
    }

    // Scope/Analyzer taps: whatever's actually wired into each one --
    // a specific source, the mix bus, post-filter, or nothing.
    if (taps != nullptr)
    {
        for (auto& node : patch.nodes)
        {
            if (node.kind != "scope" && node.kind != "analyzer")
                continue;
            TapCapture capture { node.id, juce::AudioBuffer<float>(1, numSamples) };
            capture.buffer.clear();
            if (auto* tapSource = resolveSingleInput(node.id))
                capture.buffer.copyFrom(0, 0, *tapSource, 0, 0, numSamples);
            taps->add(std::move(capture));
        }
    }

    return true;
}
