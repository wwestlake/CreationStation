#pragma once

#include <JuceHeader.h>
#include "../Patch/PatchModel.h"
#include <array>
#include <atomic>

// Live, block-based counterpart to PatchRuntimePlayer. PatchRuntimePlayer
// computes one fixed-duration buffer offline, all at once, and hands back a
// recording -- that's correct for Preview/Render-to-Project/export, but it
// means a live-bound value (a MIDI Control node, an automation lane) only
// ever takes effect on the *next* full render, never while sound is
// actively playing.
//
// PatchLiveVoice instead holds persistent per-entity DSP state (filter
// internal state, running elapsed-sample position, the mix bus's one-pole
// body-resonance state) that survives across getNextAudioBlock() calls, and
// reads live values fresh every block via std::atomic instead of a value
// baked in at rebuild time -- the same pattern
// WorkstationAudioEngine::TrackChannelSource already uses for a track's
// live volume fader (std::atomic<float> gain, set from the UI thread, read
// fresh every getNextAudioBlock() call).
//
// Structural topology -- which nodes exist, how they're wired -- is only
// ever rebuilt by rebuild(), called from the message thread on a real graph
// edit. Never touched per block. A value change (a MIDI knob moving) never
// calls rebuild() -- it writes straight through to an atomic slot via
// setLiveMidiValue().

// cw::PatchDocument is the portable, resolved patch format (save/load,
// future export) -- buildPatchDocument() already bakes any wired value
// (MIDI or variable) into a plain scalar, and deliberately doesn't know
// anything about live hardware. To make a parameter live, PatchLiveVoice
// needs a second, sibling piece of information alongside the document:
// which specific MIDI Control node (if any) should keep driving each
// parameter after rebuild. SignalLabPanel builds one of these alongside
// each buildPatchDocument() call (see buildLiveBindingMap()), using the
// same real node/port ids buildPatchDocument() already emits.
struct PatchLiveBindingMap
{
    struct Entry
    {
        juce::String targetNodeId; // matches a PatchSource::id or PatchNode::id
        juce::String targetPort;   // "level" | "cutoff" | "resonance" | "envelopeAmount" | "mixWeight:N"
        juce::String midiNodeId;   // the midiFader/midiButton node id driving it
    };
    juce::Array<Entry> entries;

    // Current value of every placed midiFader/midiButton node (regardless
    // of whether it's wired to anything) -- used to seed a newly-allocated
    // live slot at rebuild() time so a structural edit never resets an
    // already-tracked value back to zero.
    struct MidiNodeValue
    {
        juce::String nodeId;
        float value = 0.0f;
    };
    juce::Array<MidiNodeValue> midiNodeValues;

    juce::String findMidiNodeId(const juce::String& targetNodeId, const juce::String& targetPort) const
    {
        for (auto& entry : entries)
            if (entry.targetNodeId == targetNodeId && entry.targetPort == targetPort)
                return entry.midiNodeId;
        return {};
    }

    float findCurrentValue(const juce::String& nodeId) const
    {
        for (auto& entry : midiNodeValues)
            if (entry.nodeId == nodeId)
                return entry.value;
        return 0.0f;
    }
};

class PatchLiveVoice final : public juce::AudioSource
{
public:
    PatchLiveVoice();

    void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    // Message-thread only: (re)builds the topology snapshot published to the
    // audio thread. Safe to call while a playthrough is active -- the new
    // snapshot is built off to the side and atomically published; the audio
    // thread finishes whatever block it's mid-processing against the old
    // one, then adopts the new one on its next call (which may reallocate
    // that thread's own runtime DSP state to match -- see RuntimeEntityState
    // below -- a real topology edit while live may cost one small
    // allocation/reset, same as adding a track or plugin already isn't
    // glitch-free in a real DAW; steady-state parameter tweaks never reach
    // this path). Playback position is preserved across a rebuild (stored
    // outside the topology snapshot), so a structural edit while live
    // doesn't restart the sound from zero.
    void rebuild(const cw::PatchDocument& patch, const PatchLiveBindingMap& liveBindings);

    // Message-thread: starts exactly one playthrough of durationSeconds
    // from the beginning. No-op if rebuild() hasn't been called yet.
    void start(double durationSeconds);
    // Message-thread: stops immediately.
    void stop();
    bool isActive() const noexcept { return active.load(); }
    // Message-thread: true, and consumed, the first time this is checked
    // after a playthrough reached its natural end on its own (as opposed to
    // being stopped explicitly) -- SignalLabPanel polls this to know when
    // to schedule a Repeat re-trigger. Edge-triggered: returns true only
    // once per completed playthrough.
    bool takeFinishedFlag() noexcept { return finished.exchange(false); }

    // Message-thread: live value for whichever MIDI Control node in the
    // *current* compiled graph has this nodeId. No-op if that node isn't
    // present in the current graph (e.g. it was removed, or a rebuild
    // hasn't caught up yet -- harmless, nothing reads a stale slot since
    // slots are keyed to the graph that owns them).
    void setLiveMidiValue(const juce::String& nodeId, float value);

private:
    static constexpr int maxLiveMidiSlots = 64;
    static constexpr int kNoSlot = -1;

    // One atomic slot per MIDI Control node placed in the graph, resolved
    // by nodeId at rebuild() time. kNoSlot means "not live-bound", i.e. use
    // the value baked in at rebuild time same as any other unwired
    // parameter. The slot table itself (which index belongs to which
    // nodeId) is message-thread-only -- only rebuild()/setLiveMidiValue()
    // touch it, both always called from the message thread. The audio
    // thread only ever reads liveMidiValues[i] by index, which is safe as a
    // plain atomic load.
    std::array<std::atomic<float>, maxLiveMidiSlots> liveMidiValues {};
    std::array<juce::String, maxLiveMidiSlots> liveMidiSlotNodeIds;
    int liveMidiSlotCount = 0;
    int registerOrReuseSlot(const juce::String& midiNodeId, const PatchLiveBindingMap& liveBindings);

    enum class EntityKind { Source, Mix, Filter, Envelope };

    struct MixFeed
    {
        int entityIndex = -1;
        float weight = 1.0f;
        int weightMidiSlot = kNoSlot;
    };

    // Immutable per-entity CONFIGURATION only -- no DSP objects, no mutable
    // running state. This is what gets atomically published; see
    // RuntimeEntityState for the mutable counterpart the audio thread owns.
    struct LiveEntity
    {
        EntityKind kind = EntityKind::Source;
        juce::String id;
        int inputEntityIndex = -1; // Filter/Envelope only; -1 = silence

        // Source only
        juce::String sourceKind; // "oscillator" | "noise"
        juce::String waveform;
        float level = 0.0f;
        double baseFrequencyHz = 180.0;
        int levelMidiSlot = kNoSlot;
        int frequencyMidiSlot = kNoSlot; // oscillator only (meaningless for noise); log-mapped 30..2400 Hz

        // Mix only
        juce::Array<MixFeed> mixFeeds;

        // Filter only
        juce::String filterMode;
        float cutoffHz = 3600.0f, resonance = 0.90f, envelopeAmount = 0.35f;
        int cutoffMidiSlot = kNoSlot, resonanceMidiSlot = kNoSlot, envelopeAmountMidiSlot = kNoSlot;

        // Envelope only
        juce::String curveMode;
        juce::Array<cw::PatchAutomationPoint> points;
    };

    // Everything rebuild() produces, published to the audio thread as one
    // atomically-swapped immutable unit (same pattern as
    // WorkstationAudioEngine::cachedTrackRouting).
    struct EntityGraph
    {
        juce::Array<LiveEntity> entities; // topological order: sources, then mix, then filters/envelopes (dependency order)
        int finalEntityIndex = -1;        // which entity's buffer is the final output; -1 = silence
        juce::Array<cw::PatchAutomationLane> automationLanes;
        bool hasEnvelopeNode = false; // shared cross-cutting modulation source, see PatchRuntimePlayer.cpp's computeMix comment -- same deliberate simplification here
        juce::Array<cw::PatchAutomationPoint> sharedEnvelopePoints;
        juce::String sharedEnvelopeCurveMode { "linear" };
        double outputGain = 0.9;
        float noiseLevel = 0.10f, macroHardness = 0.50f, macroWeight = 0.50f, macroAir = 0.50f, macroGrit = 0.25f, macroSize = 0.50f;
        float normalizer = 0.0f;
    };

    std::atomic<std::shared_ptr<const EntityGraph>> currentGraph { nullptr };

    // Mutable, per-entity, audio-thread-OWNED state -- never touched by the
    // message thread. Parallel array to (the audio thread's currently
    // adopted) EntityGraph::entities, by index. Persists block to block;
    // only reallocated when adoptGraphIfChanged notices the published graph
    // pointer changed.
    struct RuntimeEntityState
    {
        juce::AudioBuffer<float> scratch; // 1 channel, block-sized
        juce::dsp::StateVariableTPTFilter<float> filterDsp; // Filter entities only
        float bodyState = 0.0f;           // Mix entity only: persistent one-pole lowpass state
        float previousEnvelope = 0.0f;    // Mix entity only: persistent, for the transient (rate-of-change) term
    };
    juce::OwnedArray<RuntimeEntityState> runtimeStates; // audio-thread only
    std::shared_ptr<const EntityGraph> activeGraphForAudioThread; // audio-thread only: last graph this thread adopted

    void adoptGraphIfChanged(const std::shared_ptr<const EntityGraph>& graph); // audio thread only
    void processOneBlock(const EntityGraph& graph, juce::AudioBuffer<float>& destBuffer, int destStartSample, int numSamples); // audio thread only
    // A learned MIDI Control node's live value is always a raw 0..1
    // normalized number (see WorkstationAudioEngine::LiveMidiControlChange
    // -- CC/127, or 1.0/0.0 for note on/off), regardless of what it's wired
    // to. resolveMidiOr() is only correct for a parameter whose own natural
    // range already IS 0..1 (Level). Everything else needs to be mapped
    // from that raw 0..1 into the parameter's actual range -- linear for
    // most, log for anything pitch/frequency-like where a linear sweep
    // would spend most of its travel in an unusably narrow band.
    float resolveMidiOr(int slot, float fallback) const noexcept;
    float resolveMidiOrLinear(int slot, float fallback, float rangeMin, float rangeMax) const noexcept;
    float resolveMidiOrLog(int slot, float fallback, float rangeMin, float rangeMax) const noexcept;
    // Level specifically: a fader is felt in dB, not linear amplitude -- see
    // the matching comment on normalizedLevelToAmplitude() in
    // SignalLabPanel.cpp, kept in sync deliberately rather than shared.
    float resolveMidiOrLevelDb(int slot, float fallback) const noexcept;

    double sampleRate = 48000.0;
    int maxBlockSize = 512;
    // Both written by start() (message thread) and read/advanced every
    // block by the audio thread -- must be atomic, unlike the rest of this
    // class's "which thread owns which field" split.
    std::atomic<int64> totalDurationSamples { 0 }; // playback runs [0, totalDurationSamples)
    std::atomic<int64> elapsedSamples { 0 };
    std::atomic<bool> active { false };
    std::atomic<bool> finished { false };
};
