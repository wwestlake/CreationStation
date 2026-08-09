#include "PatchLiveVoice.h"

#include <cmath>

// Design note on why runtime DSP state (filter internals, one-pole
// smoothing state) is NOT part of EntityGraph even though EntityGraph is
// the thing rebuild() publishes: EntityGraph is atomically swapped via
// std::atomic<std::shared_ptr<const EntityGraph>> so the audio thread can
// safely pick up a structural change mid-stream. If evolving per-block
// state lived inside that same const, atomically-swapped object, either
// the audio thread couldn't mutate it (it's const) or every block would
// need to publish a whole new EntityGraph just to carry updated state
// forward -- defeating the entire point of only rebuilding on structural
// change. Instead, RuntimeEntityState is a plain, non-atomic, audio-thread-
// owned array (see adoptGraphIfChanged), rebuilt only when the audio thread
// notices the published EntityGraph pointer changed since its last block.

namespace
{
constexpr float kMinFilterCutoffHz = 40.0f;
constexpr float kMaxFilterCutoffHz = 16000.0f;

float clamp01(float value) { return juce::jlimit(0.0f, 1.0f, value); }

float applyOnePoleLowPass(float input, float& state, float cutoffHz, double sampleRate)
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
    return clamp01((std::log(safeCutoff) - logMin) / juce::jmax(0.0001f, logMax - logMin));
}

float normalizedToCutoff(float normalized)
{
    auto clamped = clamp01(normalized);
    auto logMin = std::log(kMinFilterCutoffHz);
    auto logMax = std::log(kMaxFilterCutoffHz);
    return std::exp(logMin + (logMax - logMin) * clamped);
}

float applyCurveMode(float localT, const juce::String& curveMode)
{
    auto t = clamp01(localT);
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
            return juce::jmap((double) applyCurveMode((float) localT, curve), left.value, right.value);
        }
    }

    return points.getLast().value;
}

double sampleTargetLanes(const juce::Array<cw::PatchAutomationLane>& lanes,
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

// Mirrors PatchRuntimePlayer's "which id feeds this node" search, but
// returns an id string instead of a buffer -- used only at rebuild() time
// (message thread) to figure out entity wiring before it's translated into
// fast integer indices.
juce::String resolveSingleInputId(const cw::PatchDocument& patch, const juce::String& nodeId)
{
    for (auto& connection : patch.connections)
        if (connection.to == nodeId)
            return connection.from;
    return {};
}

bool hasOutgoingConnection(const cw::PatchDocument& patch, const juce::String& id)
{
    for (auto& connection : patch.connections)
        if (connection.from == id)
            return true;
    return false;
}
}

PatchLiveVoice::PatchLiveVoice()
{
    for (auto& id : liveMidiSlotNodeIds)
        id = {};
}

void PatchLiveVoice::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
    sampleRate = newSampleRate;
    maxBlockSize = juce::jmax(1, samplesPerBlockExpected);
}

void PatchLiveVoice::releaseResources()
{
}

float PatchLiveVoice::resolveMidiOr(int slot, float fallback) const noexcept
{
    if (slot == kNoSlot)
        return fallback;
    return liveMidiValues[(size_t) slot].load();
}

float PatchLiveVoice::resolveMidiOrLinear(int slot, float fallback, float rangeMin, float rangeMax) const noexcept
{
    if (slot == kNoSlot)
        return fallback;
    auto normalized = juce::jlimit(0.0f, 1.0f, liveMidiValues[(size_t) slot].load());
    return juce::jmap(normalized, 0.0f, 1.0f, rangeMin, rangeMax);
}

float PatchLiveVoice::resolveMidiOrLog(int slot, float fallback, float rangeMin, float rangeMax) const noexcept
{
    if (slot == kNoSlot)
        return fallback;
    auto normalized = juce::jlimit(0.0f, 1.0f, liveMidiValues[(size_t) slot].load());
    auto logMin = std::log(juce::jmax(0.0001f, rangeMin));
    auto logMax = std::log(juce::jmax(0.0001f, rangeMax));
    return std::exp(logMin + (logMax - logMin) * normalized);
}

void PatchLiveVoice::start(double durationSeconds)
{
    if (currentGraph.load() == nullptr)
        return;

    totalDurationSamples.store(juce::jmax<int64>(1, (int64) std::llround(juce::jmax(0.05, durationSeconds) * sampleRate)));
    elapsedSamples.store(0);
    finished.store(false);
    active.store(true);
}

void PatchLiveVoice::stop()
{
    active.store(false);
}

void PatchLiveVoice::setLiveMidiValue(const juce::String& nodeId, float value)
{
    for (int i = 0; i < liveMidiSlotCount; ++i)
    {
        if (liveMidiSlotNodeIds[(size_t) i] == nodeId)
        {
            liveMidiValues[(size_t) i].store(value);
            return;
        }
    }
    // Not in the currently-compiled graph -- e.g. not wired to anything
    // yet, or a rebuild() hasn't caught up. Nothing to update here; the
    // node's own tracked value (GraphNodeModel::midiLiveValue) is what
    // seeds its slot whenever the next rebuild() does register it.
}

int PatchLiveVoice::registerOrReuseSlot(const juce::String& midiNodeId, const PatchLiveBindingMap& liveBindings)
{
    if (midiNodeId.isEmpty())
        return kNoSlot;

    for (int i = 0; i < liveMidiSlotCount; ++i)
        if (liveMidiSlotNodeIds[(size_t) i] == midiNodeId)
            return i; // reuse -- keeps whatever value is already live-tracked there, doesn't reset it

    if (liveMidiSlotCount >= maxLiveMidiSlots)
        return kNoSlot; // extremely unlikely (64 simultaneously-bound controls) -- falls back to the baked value

    int slot = liveMidiSlotCount++;
    liveMidiSlotNodeIds[(size_t) slot] = midiNodeId;
    liveMidiValues[(size_t) slot].store(liveBindings.findCurrentValue(midiNodeId));
    return slot;
}

void PatchLiveVoice::rebuild(const cw::PatchDocument& patch, const PatchLiveBindingMap& liveBindings)
{
    // sampleRate is whatever prepareToPlay() last set it to (the live
    // device's actual rate, kept current by mixerSource propagating
    // prepareToPlay to every input source including this one) -- rebuild()
    // doesn't need its own copy passed in.
    auto graph = std::make_shared<EntityGraph>();

    double baseFrequency = 180.0;
    for (const auto& parameter : patch.parameters)
    {
        if (parameter.id == "baseFrequency") baseFrequency = parameter.defaultValue;
        else if (parameter.id == "noiseLevel") graph->noiseLevel = (float) parameter.defaultValue;
        else if (parameter.id == "macroHardness") graph->macroHardness = (float) parameter.defaultValue;
        else if (parameter.id == "macroWeight") graph->macroWeight = (float) parameter.defaultValue;
        else if (parameter.id == "macroAir") graph->macroAir = (float) parameter.defaultValue;
        else if (parameter.id == "macroGrit") graph->macroGrit = (float) parameter.defaultValue;
        else if (parameter.id == "macroSize") graph->macroSize = (float) parameter.defaultValue;
    }
    graph->outputGain = patch.output.gain;
    graph->automationLanes = patch.automationLanes;

    // Shared cross-cutting envelope curve (first Envelope node) -- same
    // resolution as PatchRuntimePlayer::renderPatchToBuffer.
    const cw::PatchNode* envelopeNode = nullptr;
    for (auto& node : patch.nodes)
        if (node.kind == "envelope") { envelopeNode = &node; break; }
    graph->hasEnvelopeNode = envelopeNode != nullptr;

    juce::String envelopeCurveMode { "linear" };
    juce::Array<cw::PatchAutomationPoint> envelopePoints;
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
                         ? juce::jmap((double) graph->macroHardness, 0.0, 1.0, 0.05, -0.05)
                         : juce::jmap((double) graph->macroSize, 0.0, 1.0, -0.04, 0.08);
        envelopePoints.getReference(index).time += timeShift;
        envelopePoints.getReference(index).value = juce::jlimit(0.0, 1.0,
            envelopePoints.getReference(index).value + juce::jmap((double) graph->macroWeight, 0.0, 1.0, -0.08, 0.12));
    }
    graph->sharedEnvelopePoints = envelopePoints;
    graph->sharedEnvelopeCurveMode = envelopeCurveMode;

    double totalSourceLevel = 0.0;
    for (auto& source : patch.sources)
        totalSourceLevel += source.level;
    graph->normalizer = totalSourceLevel > 0.0 ? (float) (0.9 / totalSourceLevel) : 0.0f;

    // --- Sources: always first, in patch order. ---
    juce::HashMap<juce::String, int> entityIndexById; // source/mix/filter/envelope id -> its final index in graph->entities
    for (int i = 0; i < patch.sources.size(); ++i)
    {
        const auto& source = patch.sources.getReference(i);
        LiveEntity entity;
        entity.kind = EntityKind::Source;
        entity.id = source.id;
        entity.sourceKind = source.kind;
        entity.waveform = source.waveform;
        entity.level = (float) source.level;
        entity.baseFrequencyHz = baseFrequency;
        if (source.frequencyParameter.isNotEmpty())
            for (auto& parameter : patch.parameters)
                if (parameter.id == source.frequencyParameter) { entity.baseFrequencyHz = parameter.defaultValue; break; }

        entity.levelMidiSlot = registerOrReuseSlot(liveBindings.findMidiNodeId(source.id, "level"), liveBindings);
        entity.frequencyMidiSlot = registerOrReuseSlot(liveBindings.findMidiNodeId(source.id, "frequency"), liveBindings);

        graph->entities.add(entity);
        entityIndexById.set(source.id, graph->entities.size() - 1);
    }

    juce::String mixId, outputId;
    for (auto& node : patch.nodes)
    {
        if (node.kind == "mix") mixId = node.id;
        else if (node.kind == "output") outputId = node.id;
    }

    // --- Mix: always exists as an entity, even with no explicit "mix" node
    // (implicit sum of every source with no outgoing wire), matching
    // PatchRuntimePlayer's fallback behaviour. Its feeds only ever
    // reference Source entities, which are always already placed above, so
    // it can always go immediately after them with no dependency ordering
    // needed. ---
    {
        LiveEntity mixEntity;
        mixEntity.kind = EntityKind::Mix;
        mixEntity.id = mixId.isNotEmpty() ? mixId : juce::String("__implicit_mix__");

        juce::StringArray sourcesWithOutgoingWire;
        for (auto& connection : patch.connections)
            for (auto& source : patch.sources)
                if (source.id == connection.from && ! sourcesWithOutgoingWire.contains(connection.from))
                    sourcesWithOutgoingWire.add(connection.from);

        if (mixId.isNotEmpty())
        {
            for (auto& connection : patch.connections)
            {
                if (connection.to != mixId)
                    continue;
                if (! entityIndexById.contains(connection.from))
                    continue; // only sources can feed mix, matching the offline renderer
                MixFeed feed;
                feed.entityIndex = entityIndexById[connection.from];
                feed.weight = (float) connection.weight;
                // connection.toPort here is "signalIn:N" (the signal wire) --
                // buildLiveBindingMap() registers weight bindings under
                // "mixWeight:N" (matching buildPatchDocument()'s own
                // mixWeight:N port id convention for the separate value
                // wire), so translate the channel index across formats
                // rather than looking up the signal port's own id.
                auto channelIndex = connection.toPort.fromFirstOccurrenceOf(":", false, false).getIntValue();
                feed.weightMidiSlot = registerOrReuseSlot(liveBindings.findMidiNodeId(mixId, "mixWeight:" + juce::String(channelIndex)), liveBindings);
                mixEntity.mixFeeds.add(feed);
            }
        }
        else
        {
            for (int i = 0; i < patch.sources.size(); ++i)
                if (! sourcesWithOutgoingWire.contains(patch.sources.getReference(i).id))
                {
                    MixFeed feed;
                    feed.entityIndex = entityIndexById[patch.sources.getReference(i).id];
                    feed.weight = 1.0f;
                    mixEntity.mixFeeds.add(feed);
                }
        }

        graph->entities.add(mixEntity);
        entityIndexById.set(mixEntity.id, graph->entities.size() - 1);
    }
    auto resolvedMixEntityId = graph->entities.getReference(graph->entities.size() - 1).id;

    // --- Filter/Envelope: real multi-instance entities, arbitrary
    // topology, resolved into dependency order (Kahn's-algorithm-style,
    // same idea as SignalLabPanel::validateGraph()'s cycle detection,
    // applied here to produce an evaluation order instead of an error
    // list). A cycle is broken by placing whatever's left with an
    // unresolved (silent) input, matching the offline renderer's
    // ready/computing recursion-guard behaviour for the same case. ---
    struct PendingNode
    {
        juce::String id;
        bool isFilter = false;
        juce::String rawInputId; // may reference a source (already placed), mix (already placed), or another pending filter/envelope
        const cw::PatchNode* node = nullptr;
    };
    juce::Array<PendingNode> pending;
    for (auto& node : patch.nodes)
    {
        if (node.kind != "filter" && node.kind != "envelope")
            continue;
        PendingNode entry;
        entry.id = node.id;
        entry.isFilter = node.kind == "filter";
        entry.rawInputId = resolveSingleInputId(patch, node.id);
        entry.node = &node;
        pending.add(entry);
    }

    bool progressed = true;
    while (! pending.isEmpty() && progressed)
    {
        progressed = false;
        for (int i = pending.size(); --i >= 0;)
        {
            auto& candidate = pending.getReference(i);
            // Placeable once its input is either unresolved (silence -- fine,
            // same as the offline renderer treating a missing wire as
            // silence), or already placed.
            bool inputReady = candidate.rawInputId.isEmpty() || entityIndexById.contains(candidate.rawInputId);
            if (! inputReady)
                continue;

            LiveEntity entity;
            entity.kind = candidate.isFilter ? EntityKind::Filter : EntityKind::Envelope;
            entity.id = candidate.id;
            entity.inputEntityIndex = candidate.rawInputId.isNotEmpty() ? entityIndexById[candidate.rawInputId] : -1;

            if (candidate.isFilter)
            {
                entity.filterMode = candidate.node->properties.getWithDefault("mode", juce::String("lowpass")).toString();
                entity.cutoffHz = getNumericProperty(candidate.node->properties, "cutoffHz", entity.cutoffHz);
                entity.resonance = getNumericProperty(candidate.node->properties, "resonance", entity.resonance);
                entity.envelopeAmount = getNumericProperty(candidate.node->properties, "envelopeAmount", entity.envelopeAmount);
                entity.cutoffMidiSlot = registerOrReuseSlot(liveBindings.findMidiNodeId(candidate.id, "cutoff"), liveBindings);
                entity.resonanceMidiSlot = registerOrReuseSlot(liveBindings.findMidiNodeId(candidate.id, "resonance"), liveBindings);
                entity.envelopeAmountMidiSlot = registerOrReuseSlot(liveBindings.findMidiNodeId(candidate.id, "envelopeAmount"), liveBindings);
            }
            else
            {
                entity.curveMode = candidate.node->properties.getWithDefault("curveMode", juce::String("linear")).toString();
                auto pointsJson = candidate.node->properties.getWithDefault("pointsJson", {}).toString();
                if (pointsJson.isNotEmpty())
                    entity.points = parseEnvelopePointsJson(pointsJson, entity.curveMode);
                if (entity.points.isEmpty())
                    entity.points = envelopePoints;
            }

            graph->entities.add(entity);
            entityIndexById.set(candidate.id, graph->entities.size() - 1);
            pending.remove(i);
            progressed = true;
        }
    }
    // Anything left in `pending` is part of a cycle (or depends, transitively,
    // on one) -- place it with a silent input rather than drop it, so it's
    // still visible/tappable even though its input can't be resolved.
    for (auto& candidate : pending)
    {
        LiveEntity entity;
        entity.kind = candidate.isFilter ? EntityKind::Filter : EntityKind::Envelope;
        entity.id = candidate.id;
        entity.inputEntityIndex = -1;
        if (candidate.isFilter)
        {
            entity.filterMode = candidate.node->properties.getWithDefault("mode", juce::String("lowpass")).toString();
            entity.cutoffHz = getNumericProperty(candidate.node->properties, "cutoffHz", entity.cutoffHz);
            entity.resonance = getNumericProperty(candidate.node->properties, "resonance", entity.resonance);
            entity.envelopeAmount = getNumericProperty(candidate.node->properties, "envelopeAmount", entity.envelopeAmount);
        }
        else
        {
            entity.curveMode = candidate.node->properties.getWithDefault("curveMode", juce::String("linear")).toString();
            entity.points = envelopePoints;
        }
        graph->entities.add(entity);
        entityIndexById.set(candidate.id, graph->entities.size() - 1);
    }

    // --- Step 3 equivalent: resolve the final output, same fallback order
    // as PatchRuntimePlayer: explicit Sink wiring, else the first Filter/
    // Envelope with no outgoing wire, else Mix. ---
    graph->finalEntityIndex = -1;
    if (outputId.isNotEmpty())
    {
        auto sinkInputId = resolveSingleInputId(patch, outputId);
        if (sinkInputId.isNotEmpty() && entityIndexById.contains(sinkInputId))
            graph->finalEntityIndex = entityIndexById[sinkInputId];
    }
    if (graph->finalEntityIndex < 0)
    {
        for (auto& entity : graph->entities)
        {
            if ((entity.kind == EntityKind::Filter || entity.kind == EntityKind::Envelope) && ! hasOutgoingConnection(patch, entity.id))
            {
                graph->finalEntityIndex = entityIndexById[entity.id];
                break;
            }
        }
    }
    if (graph->finalEntityIndex < 0)
        graph->finalEntityIndex = entityIndexById[resolvedMixEntityId];

    currentGraph.store(std::move(graph));
}

void PatchLiveVoice::adoptGraphIfChanged(const std::shared_ptr<const EntityGraph>& graph)
{
    if (graph == activeGraphForAudioThread)
        return;

    runtimeStates.clear();
    for (auto& entity : graph->entities)
    {
        auto* state = runtimeStates.add(new RuntimeEntityState());
        state->scratch.setSize(1, maxBlockSize, false, false, true);
        state->scratch.clear();

        if (entity.kind == EntityKind::Filter)
        {
            juce::dsp::ProcessSpec spec;
            spec.sampleRate = sampleRate;
            spec.maximumBlockSize = (uint32) maxBlockSize;
            spec.numChannels = 1;
            state->filterDsp.prepare(spec);
            state->filterDsp.reset();
            state->filterDsp.setType(entity.filterMode == "highpass" ? juce::dsp::StateVariableTPTFilterType::highpass
                                                                      : entity.filterMode == "bandpass" ? juce::dsp::StateVariableTPTFilterType::bandpass
                                                                                                         : juce::dsp::StateVariableTPTFilterType::lowpass);
        }
    }

    activeGraphForAudioThread = graph;
}

void PatchLiveVoice::processOneBlock(const EntityGraph& graph, juce::AudioBuffer<float>& destBuffer, int destStartSample, int numSamples)
{
    auto totalSamples = totalDurationSamples.load();
    auto startElapsed = elapsedSamples.load();
    bool hasAutomationLanes = ! graph.automationLanes.isEmpty();

    for (int localSample = 0; localSample < numSamples; ++localSample)
    {
        auto absoluteSample = startElapsed + localSample;
        auto t = (float) absoluteSample / (float) juce::jmax<int64>(1, totalSamples - 1);

        auto pitchMotion = hasAutomationLanes ? sampleTargetLanes(graph.automationLanes, "pitchOffsetSemitones", t, 0.0) : 0.0;
        auto gainMotion = hasAutomationLanes ? sampleTargetLanes(graph.automationLanes, "outputGain", t, 1.0) : 1.0;
        auto noiseMotion = hasAutomationLanes ? sampleTargetLanes(graph.automationLanes, "noiseLevel", t, graph.noiseLevel) : (double) graph.noiseLevel;
        auto hardnessMotion = (float) (hasAutomationLanes ? sampleTargetLanes(graph.automationLanes, "macroHardness", t, graph.macroHardness) : (double) graph.macroHardness);
        auto weightMotion = (float) (hasAutomationLanes ? sampleTargetLanes(graph.automationLanes, "macroWeight", t, graph.macroWeight) : (double) graph.macroWeight);
        auto airMotion = (float) (hasAutomationLanes ? sampleTargetLanes(graph.automationLanes, "macroAir", t, graph.macroAir) : (double) graph.macroAir);
        auto gritMotion = (float) (hasAutomationLanes ? sampleTargetLanes(graph.automationLanes, "macroGrit", t, graph.macroGrit) : (double) graph.macroGrit);
        auto sizeMotion = (float) (hasAutomationLanes ? sampleTargetLanes(graph.automationLanes, "macroSize", t, graph.macroSize) : (double) graph.macroSize);
        auto pitchSemitones = (float) pitchMotion + juce::jmap(weightMotion, 0.0f, 1.0f, 2.0f, -2.0f) * (t - 0.5f) * 2.0f;
        auto sharedEnvelope = graph.hasEnvelopeNode ? (float) sampleAutomationPoints(graph.sharedEnvelopePoints, graph.sharedEnvelopeCurveMode, t, 0.0) : 1.0f;

        for (int entityIndex = 0; entityIndex < graph.entities.size(); ++entityIndex)
        {
            auto& entity = graph.entities.getReference(entityIndex);
            auto& runtime = *runtimeStates[entityIndex];
            float outputValue = 0.0f;

            if (entity.kind == EntityKind::Source)
            {
                auto level = resolveMidiOr(entity.levelMidiSlot, entity.level);
                if (entity.sourceKind == "oscillator")
                {
                    auto liveBaseFrequency = (double) resolveMidiOrLog(entity.frequencyMidiSlot, (float) entity.baseFrequencyHz, 30.0f, 2400.0f);
                    auto weightedBaseFrequency = liveBaseFrequency
                                               * (double) juce::jmap(weightMotion, 0.0f, 1.0f, 1.16f, 0.86f)
                                               * (double) juce::jmap(sizeMotion, 0.0f, 1.0f, 1.04f, 0.94f);
                    auto frequency = weightedBaseFrequency * std::pow(2.0, pitchSemitones / 12.0);
                    auto phase = juce::MathConstants<double>::twoPi * frequency * ((double) absoluteSample / sampleRate);

                    float waveform = 0.0f;
                    if (entity.waveform == "sine")
                        waveform = (float) std::sin(phase);
                    else if (entity.waveform == "saw")
                        waveform = 2.0f * ((float) (phase / juce::MathConstants<double>::twoPi) - std::floor(0.5f + (float) (phase / juce::MathConstants<double>::twoPi)));
                    else if (entity.waveform == "square")
                        waveform = std::sin(phase) >= 0.0 ? 1.0f : -1.0f;
                    else if (entity.waveform == "triangle")
                        waveform = (float) (std::asin(std::sin(phase)) * (2.0 / juce::MathConstants<double>::pi));
                    outputValue = waveform * level;
                }
                else if (entity.sourceKind == "noise")
                {
                    auto hashed = std::sin((float) absoluteSample * 12.9898f + 78.233f) * 43758.5453f;
                    outputValue = (2.0f * (hashed - std::floor(hashed)) - 1.0f) * clamp01(level);
                }
            }
            else if (entity.kind == EntityKind::Mix)
            {
                auto hashed = std::sin((float) absoluteSample * 12.9898f + 78.233f) * 43758.5453f;

                float mixed = 0.0f;
                for (auto& feed : entity.mixFeeds)
                {
                    // 0..1.5 matches the manual fader's own range (MixerFaderBank) --
                    // a MIDI fader at max should reach the same boosted ceiling the
                    // on-screen slider allows, not cap at unity.
                    auto weight = resolveMidiOrLinear(feed.weightMidiSlot, feed.weight, 0.0f, 1.5f);
                    mixed += runtimeStates[feed.entityIndex]->scratch.getSample(0, localSample) * weight;
                }

                auto transient = juce::jmax(0.0f, sharedEnvelope - runtime.previousEnvelope);
                runtime.previousEnvelope = sharedEnvelope;

                auto gritDrive = 1.0f + gritMotion * 5.5f;
                auto macroNoise = juce::jlimit(0.0f, 1.0f, (float) noiseMotion + airMotion * 0.18f + gritMotion * 0.12f);
                mixed += macroNoise * (hashed - std::floor(hashed));
                auto value = graph.normalizer * (float) gainMotion * (float) graph.outputGain * mixed;
                value += transient * juce::jmap(hardnessMotion, 0.0f, 1.0f, 0.0f, 0.45f);
                value = std::tanh(value * gritDrive) / std::tanh(gritDrive);
                auto bodyCutoff = juce::jmap(weightMotion, 0.0f, 1.0f, 120.0f, 520.0f);
                auto bodyComponent = applyOnePoleLowPass(value, runtime.bodyState, bodyCutoff, sampleRate);
                value += bodyComponent * juce::jmap(weightMotion, 0.0f, 1.0f, 0.0f, 0.32f);
                value *= juce::jmap(sizeMotion, 0.0f, 1.0f, 0.98f, 1.12f + 0.08f * std::sin(t * juce::MathConstants<float>::pi));
                value = juce::jmap(airMotion, value, juce::jlimit(-1.0f, 1.0f, value));

                outputValue = value;
            }
            else if (entity.kind == EntityKind::Envelope)
            {
                auto envelopeValue = (float) sampleAutomationPoints(entity.points, entity.curveMode, t, 0.0);
                float inputValue = entity.inputEntityIndex >= 0 ? runtimeStates[entity.inputEntityIndex]->scratch.getSample(0, localSample) : 0.0f;
                outputValue = inputValue * envelopeValue;
            }
            else if (entity.kind == EntityKind::Filter)
            {
                auto filterMotion = hasAutomationLanes ? sampleTargetLanes(graph.automationLanes, "filterCutoff", t, 0.5) : 0.5;
                auto baseCutoff = resolveMidiOrLog(entity.cutoffMidiSlot, entity.cutoffHz, kMinFilterCutoffHz, kMaxFilterCutoffHz);
                auto baseResonance = resolveMidiOrLinear(entity.resonanceMidiSlot, entity.resonance, 0.30f, 8.0f);
                auto baseEnvAmount = resolveMidiOrLinear(entity.envelopeAmountMidiSlot, entity.envelopeAmount, -1.0f, 1.0f);
                auto resonanceMotion = hasAutomationLanes ? sampleTargetLanes(graph.automationLanes, "filterResonance", t, baseResonance) : (double) baseResonance;
                auto filterEnvelopeMotion = (float) (hasAutomationLanes ? sampleTargetLanes(graph.automationLanes, "filterEnvelopeAmount", t, baseEnvAmount) : (double) baseEnvAmount);

                float inputValue = entity.inputEntityIndex >= 0 ? runtimeStates[entity.inputEntityIndex]->scratch.getSample(0, localSample) : 0.0f;

                auto envelopeContribution = graph.hasEnvelopeNode ? (sharedEnvelope - 0.5f) : 0.0f;
                auto filterNormalized = clamp01(cutoffToNormalized(baseCutoff)
                                                + ((float) filterMotion - 0.5f) * 0.75f
                                                + envelopeContribution * (filterEnvelopeMotion + hardnessMotion * 0.30f)
                                                + airMotion * 0.18f
                                                - weightMotion * 0.10f
                                                - sizeMotion * 0.06f);
                auto cutoffHz = normalizedToCutoff(filterNormalized);
                runtime.filterDsp.setCutoffFrequency(juce::jlimit(kMinFilterCutoffHz, (float) (sampleRate * 0.45), cutoffHz));
                runtime.filterDsp.setResonance(juce::jlimit(0.30f, 8.0f, (float) resonanceMotion + hardnessMotion * 0.9f - weightMotion * 0.15f));
                auto filteredValue = runtime.filterDsp.processSample(0, inputValue);
                outputValue = juce::jmap(airMotion, filteredValue, juce::jlimit(-1.0f, 1.0f, inputValue));
            }

            runtime.scratch.setSample(0, localSample, outputValue);
        }
    }

    if (graph.finalEntityIndex >= 0 && graph.finalEntityIndex < runtimeStates.size())
    {
        auto& finalScratch = runtimeStates[graph.finalEntityIndex]->scratch;
        for (int channel = 0; channel < destBuffer.getNumChannels(); ++channel)
            destBuffer.addFrom(channel, destStartSample, finalScratch, 0, 0, numSamples, 0.9f);
    }

    elapsedSamples.store(startElapsed + numSamples);
}

void PatchLiveVoice::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;

    if (! active.load())
        return;

    auto graph = currentGraph.load();
    if (graph == nullptr)
        return;

    adoptGraphIfChanged(graph);

    auto totalSamples = totalDurationSamples.load();
    auto remaining = totalSamples - elapsedSamples.load();
    if (remaining <= 0)
    {
        active.store(false);
        finished.store(true);
        return;
    }

    auto numSamples = juce::jmin(bufferToFill.numSamples, (int) juce::jmin<int64>(remaining, maxBlockSize));

    processOneBlock(*graph, *bufferToFill.buffer, bufferToFill.startSample, numSamples);

    if (totalSamples - elapsedSamples.load() <= 0)
    {
        active.store(false);
        finished.store(true);
    }
}
