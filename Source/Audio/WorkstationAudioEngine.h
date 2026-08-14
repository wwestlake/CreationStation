#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>
#include "SignalGraphRuntime.h"
#include "PatchLiveVoice.h"
#include "../Timeline/TimelineModel.h"

class WorkstationAudioEngine final : public juce::AudioIODeviceCallback,
                                     private juce::MidiInputCallback
{
public:
    enum class MetronomeMode
    {
        off = 0,
        playOrRecord = 1,
        always = 2
    };

    struct InputSourceDescriptor
    {
        int channelIndex = -1;
        juce::String id;
        juce::String name;
    };

    struct PreviewSettings
    {
        double startNormalized = 0.0;
        double endNormalized = 1.0;
        float gainDecibels = 0.0f;
        float fadeInNormalized = 0.0f;
        float fadeOutNormalized = 0.0f;
        bool reverse = false;
        bool normalize = false;
    };

    struct RecordingTarget
    {
        int trackIndex = -1;
        juce::File file;
    };

    struct PlaybackClipTarget
    {
        int trackIndex = -1;
        juce::File file;
        double startSeconds = 0.0;
        double sourceStartSeconds = 0.0;
        double durationSeconds = 0.0;
    };

    // A MIDI clip scheduled for real-time playback: notes are delivered live, sample-accurately,
    // through the same injection path as a live MIDI keyboard - not offline-rendered. This is the
    // safe alternative to bouncing through the instrument plugin ahead of time (which crashed at
    // least one real-world plugin under sustained non-real-time processBlock load).
    struct MidiPlaybackClip
    {
        int trackIndex = -1;
        double startSeconds = 0.0;
        double durationSeconds = 0.0;
        std::vector<cs::MidiNoteEvent> notes;
    };

    struct RenderSettings
    {
        double sampleRate = 48000.0;
        int blockSize = 512;
        bool normalizePeak = false;
        float peakTargetDecibels = -1.0f;
    };

    WorkstationAudioEngine();

    void attachToDevice(juce::AudioDeviceManager& deviceManager);
    void detachFromDevice(juce::AudioDeviceManager& deviceManager);

    void setPlaying(bool shouldPlay);
    bool isPlaying() const noexcept { return playing; }
    void setPlaybackPositionSeconds(double seconds);
    void setMetronomeEnabled(bool shouldEnable) noexcept;
    void setMetronomeMode(MetronomeMode mode) noexcept;
    MetronomeMode getMetronomeMode() const noexcept { return metronomeMode.load(); }
    bool isMetronomeEnabled() const noexcept { return metronomeEnabled.load(); }
    void setMetronomeTempo(double bpm, int numerator) noexcept;
    bool isRecording() const noexcept { return recording; }
    double getSampleRate() const noexcept { return arrangementSource.getSampleRate(); }
    bool startRecordingToFile(const juce::File& file, juce::String& errorMessage);
    bool startRecordingToFiles(const juce::Array<RecordingTarget>& targets, juce::String& errorMessage);
    void stopRecording();
    juce::File getRecordingFile() const { return recordingFile; }
    juce::Array<juce::File> getRecordingFiles() const;
    bool previewAssetFile(const juce::File& file, juce::String& errorMessage);
    bool previewAssetFile(const juce::File& file, const PreviewSettings& settings, juce::String& errorMessage);
    bool previewGeneratedBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate, juce::String& errorMessage);
    bool setTrackerPlaybackClips(const juce::Array<PlaybackClipTarget>& targets, juce::String& errorMessage);
    void setTrackerMidiClips(const juce::Array<MidiPlaybackClip>& clips);

    // Message-thread-safe: queues an immediate note on/off for one specific track's instrument,
    // for UI-driven audition (e.g. clicking a note in the piano roll) - bypasses MIDI channel
    // filtering entirely, since this always targets the exact track the user is editing.
    void auditionNoteOn(int trackIndex, int pitch, int velocity);
    void auditionNoteOff(int trackIndex, int pitch);

    // Message-thread-safe: sends an All Notes Off / All Sound Off to every track's instrument.
    // Stopping the transport mid-note otherwise abandons that note's real-time-scheduled note-off
    // (it was scheduled for a future audio block that will now never arrive), leaving a stuck
    // voice sounding indefinitely in the plugin - this is the actual host-side fix for that.
    void requestAllNotesOff();

    struct MidiLearnResult
    {
        juce::String deviceId;
        int channel = 1;
        int number = 0;
        bool isController = false;
    };

    // What kind of physical control the caller is expecting to learn -- lets armMidiLearn ignore
    // a candidate that's obviously the wrong shape for what's being bound, instead of grabbing
    // whatever arrives first. Concretely: a motorized fader's touch sensor fires a Note the
    // instant you touch it, *before* any of the actual pitch-wheel position data -- learning a
    // Fader Control node with no filtering would capture that touch-note instead of the fader
    // itself. More kinds (e.g. a relative encoder) can be added here as more MIDI Control node
    // types are added; Any preserves today's behavior for the transport-button learn path.
    enum class MidiLearnKind
    {
        Any,        // accept the first Note, CC, or pitch-wheel candidate (transport buttons)
        Continuous, // accept CC or pitch-wheel, reject a plain Note (Fader Control nodes)
        Discrete    // accept Note or CC, reject pitch-wheel (Button Control nodes)
    };

    // Message-thread-safe: arms a one-shot capture of the next note-on or active CC message,
    // optionally restricted to one device (empty = accept from any enabled device) and to one
    // kind of control - this is the backend for "right-click a control, choose Learn, wiggle the
    // hardware" binding setup.
    void armMidiLearn(const juce::String& deviceIdFilter = {}, MidiLearnKind expectedKind = MidiLearnKind::Any);
    void cancelMidiLearn();
    bool isMidiLearnArmed() const noexcept;
    // Message-thread-safe: returns true and fills result if a capture has landed since arming.
    bool takeMidiLearnResult(MidiLearnResult& result);
    // Message-thread-safe (called from any MIDI callback thread): if learn is armed and this
    // candidate passes the current device filter, captures it and disarms. Shared with
    // XTouchControlSurface, which receives control-surface devices (BCR2000/X-Touch) that this
    // engine deliberately does NOT register its own MIDI callback for - those still need to be
    // learnable even though they're excluded from live instrument routing.
    bool offerMidiLearnCandidate(const juce::String& deviceId, int channel, int number, bool isController);

    // Live dispatch for controls that were already learned (as opposed to the one-shot capture
    // above, which is only for the moment of learning a new binding). A UI panel with placed,
    // learned MIDI-bound controls (e.g. Signal Lab's Fader/Button Control nodes) polls
    // takeLiveMidiControlChanges() at UI-timer rate to find out what moved since the last poll.
    struct LiveMidiControlChange
    {
        juce::String deviceId;
        int channel = 1;
        int number = 0;
        bool isController = false;
        float value = 0.0f; // 0..1 normalized: CC/127, or 1.0 (note-on) / 0.0 (note-off)
    };
    // Message-thread-safe (called from any MIDI callback thread): records the latest value for
    // this (deviceId, channel, number, isController) tuple. Deliberately coarse - last-value-wins
    // per tuple between polls, not a queue - callers are UI-rate consumers, not audio-rate.
    void reportLiveMidiControlValue(const juce::String& deviceId, int channel, int number, bool isController, float normalizedValue);
    // Message-thread-safe: returns true and fills outChanges if anything has moved since the last
    // call, then clears the pending set.
    bool takeLiveMidiControlChanges(juce::Array<LiveMidiControlChange>& outChanges);

    // Signal Lab live playback -- thin wrappers over patchLiveVoice, mirroring the
    // setTrackGain-style shape used for the rest of this engine's live-adjustable state. See
    // PatchLiveVoice.h for the real design rationale (why parameter changes never rebuild, why
    // structural changes do).
    void rebuildSignalLabLiveGraph(const cw::PatchDocument& patch, const PatchLiveBindingMap& liveBindings) { patchLiveVoice.rebuild(patch, liveBindings); }
    void startSignalLabLivePlayback(double durationSeconds) { patchLiveVoice.start(durationSeconds); }
    void stopSignalLabLivePlayback() { patchLiveVoice.stop(); }
    bool isSignalLabLivePlaybackActive() const noexcept { return patchLiveVoice.isActive(); }
    bool takeSignalLabLivePlaybackFinishedFlag() noexcept { return patchLiveVoice.takeFinishedFlag(); }
    void setSignalLabLiveMidiValue(const juce::String& nodeId, float value) { patchLiveVoice.setLiveMidiValue(nodeId, value); }
    int copySignalLabLiveScopeSamples(const juce::String& nodeId, juce::AudioBuffer<float>& dest, int numSamples) { return patchLiveVoice.copyRecentScopeSamples(nodeId, dest, numSamples); }
    void updateSignalLabLiveScopeTaps(const juce::Array<juce::String>& tapNodeIds) { patchLiveVoice.updateScopeTaps(tapNodeIds); }
    double getSignalLabLiveSampleRate() const noexcept { return patchLiveVoice.getSampleRate(); }

    // A single captured MIDI event during live recording, timestamped as an absolute sample
    // position on the engine's running audio clock (not clip-relative) - paired up into notes and
    // converted to clip-relative beats once recording stops, on the message thread.
    struct RecordedMidiEvent
    {
        int trackIndex = -1;
        juce::MidiMessage message;
        int64 samplePosition = 0;
    };

    // Message-thread-safe: arms/disarms live MIDI capture. While active, incoming live keyboard
    // input on any track that is both record-armed and a MIDI track gets timestamped and queued.
    void startMidiRecording();
    void stopMidiRecording();
    bool isMidiRecording() const noexcept { return midiRecordingActive.load(); }
    // Message-thread-safe: drains and returns everything captured since the last call.
    std::vector<RecordedMidiEvent> takeRecordedMidiEvents();
    bool renderTrackerMixToBuffer(const juce::Array<PlaybackClipTarget>& targets,
                                  double durationSeconds,
                                  const RenderSettings& settings,
                                  juce::AudioBuffer<float>& outputBuffer,
                                  juce::String& errorMessage);
    void reapplyHostedPluginStates();
    void stopAssetPreview();
    bool isPreviewingAsset() const noexcept;

    static constexpr int getVisibleChannelCount() noexcept { return visibleChannelCount; }
    int getTrackCount() const noexcept { return tracks.size(); }
    int addTrack(const juce::String& trackName = {});
    bool removeTrack(int trackIndex);
    juce::Array<InputSourceDescriptor> getInputSources() const;
    int getTrackInputChannel(int trackIndex) const;
    void setTrackInputChannel(int trackIndex, int inputChannel);
    void setTrackRecordingArmed(int trackIndex, bool armed);
    bool isTrackRecordingArmed(int trackIndex) const;
    void setTrackMonitoringEnabled(int trackIndex, bool enabled);
    bool isTrackMonitoringEnabled(int trackIndex) const;
    void setTrackStereoEnabled(int trackIndex, bool enabled);
    bool isTrackStereoEnabled(int trackIndex) const;

    // 0 = Omni (all channels), 1-16 = a specific MIDI channel. Governs which channel of live
    // MIDI keyboard/controller input (not the X-Touch control surface, which is handled
    // separately) gets routed into this track's instrument plugin.
    void setTrackMidiInputChannel(int trackIndex, int channel);
    int getTrackMidiInputChannel(int trackIndex) const;

    // Empty = any enabled MIDI input device feeds this track (the original behaviour). A
    // non-empty juce::MidiDeviceInfo::identifier restricts this track to that one physical
    // device, independent of the channel filter above - this is what lets "Yamaha -> Track 3"
    // and "nanoKONTROL -> Track 5" coexist without one bleeding into the other.
    void setTrackMidiInputDeviceId(int trackIndex, const juce::String& deviceId);
    juce::String getTrackMidiInputDeviceId(int trackIndex) const;

    // Tells the engine this track is a MIDI track, so it stops metering/monitoring/recording
    // raw audio input on it (a MIDI track has no analog input signal of its own).
    void setTrackIsMidiKind(int trackIndex, bool isMidi);

    // Automation tracks never render audio/MIDI of their own - they only push values into
    // another track's controls once per audio block. This excludes the track from the normal
    // audio-producing render path entirely (no DemoTrackSource playback, no insert-chain audio).
    void setTrackIsAutomationKind(int trackIndex, bool isAutomation);
    // Publishes an immutable snapshot of an automation lane's target + curve for the audio
    // thread to read lock-free, once per block. Called from the message thread whenever the
    // lane's points or target change (point edits, target reassignment, project load).
    void setTrackAutomationData(int trackIndex, const cs::AutomationTarget& target, const std::vector<cs::AutomationPoint>& points);
    // Suspends (true) or resumes (false) this automation lane's per-block application to its
    // target while a manual recording gesture (fader ride) owns that target's value right now.
    void setTrackAutomationWriteActive(int trackIndex, bool active);

    // Folder-track bus routing: -1 (default) routes trackIndex's audio straight to master, same
    // as always; >= 0 routes it into that track's own buffer instead (which must in turn be a
    // TrackKind::folder track - enforced at the TimelineModel level, not here). Triggers a
    // message-thread-only recompute of the cached render order the audio thread reads each block.
    void setTrackParentIndex(int trackIndex, int parentTrackIndex);
    // Reorders the engine's own track array to match a TimelineModel::moveTrackRange call - same
    // startIndex/length/destinationIndex, so the two stay index-parity with each other (everything
    // else - gain, pan, insert chain, automation data - travels with the moved TrackChannelSource
    // object itself; only parentTrackIndex needs a subsequent syncTrackViews() refresh, since it's
    // an index that the reorder itself invalidates).
    void moveTrackRange(int startIndex, int length, int destinationIndex);

    // Keeps a track's live audio path running for as long as its plugin editor window is open -
    // otherwise clicking a plugin's own on-screen keyboard/pads triggers a note inside the plugin
    // that never gets flushed to audio, since nothing about a pure UI click signals the host that
    // MIDI activity happened (unlike live keyboard input or scheduled clip notes, which do).
    void setTrackHasOpenEditor(int trackIndex, bool hasOpenEditor);

    juce::String getTrackName(int trackIndex) const;
    void setTrackName(int trackIndex, const juce::String& name);
    float getTrackLevel(int trackIndex) const;
    float consumeTrackRecordingPeak(int trackIndex);
    float getTrackGain(int trackIndex) const;
    float getTrackPan(int trackIndex) const;
    bool isTrackMuted(int trackIndex) const;
    bool isTrackSoloed(int trackIndex) const;

    void setTrackGain(int trackIndex, float gain);
    void setTrackPan(int trackIndex, float pan);
    void setTrackMuted(int trackIndex, bool shouldMute);
    void setTrackSoloed(int trackIndex, bool shouldSolo);
    void setMasterGain(float gain);
    float getMasterGain() const noexcept { return masterGain.load(); }
    void setGraphEnabled(bool shouldEnable);
    bool isGraphEnabled() const noexcept { return graphEnabled.load(); }
    void setGraphDrive(float amount);
    float getGraphDrive() const noexcept { return graphDrive.load(); }
    void setGraphInput(float amount);
    float getGraphInput() const noexcept { return graphInput.load(); }
    void setGraphSourceFrequency(float hz);
    float getGraphSourceFrequency() const noexcept { return graphSourceFrequency.load(); }
    void setGraphTone(float amount);
    float getGraphTone() const noexcept { return graphTone.load(); }
    void setGraphEcho(float amount);
    float getGraphEcho() const noexcept { return graphEcho.load(); }
    void setGraphWidth(float amount);
    float getGraphWidth() const noexcept { return graphWidth.load(); }
    bool loadGraphVstPlugin(const juce::File& file, juce::String& errorMessage);
    void unloadGraphVstPlugin();
    juce::String getGraphVstPluginName() const;
    juce::File getGraphVstPluginFile() const;
    bool hasGraphVstPlugin() const noexcept;
    void setGraphVstEnabled(bool shouldEnable);
    bool isGraphVstEnabled() const noexcept { return graphVstEnabled.load(); }
    void setGraphVstMix(float amount);
    float getGraphVstMix() const noexcept { return graphVstMix.load(); }
    juce::AudioProcessorEditor* createGraphVstPluginEditor();

    bool loadMasterPlugin(const juce::File& file, juce::String& errorMessage);
    void unloadMasterPlugin();
    juce::String getMasterPluginName() const;
    juce::File getMasterPluginFile() const;
    bool hasMasterPlugin() const noexcept;
    void setMasterPluginBypassed(bool shouldBypass);
    bool isMasterPluginBypassed() const noexcept;
    juce::AudioProcessorEditor* createMasterPluginEditor();

    bool loadTrackPlugin(int trackIndex, const juce::File& file, juce::String& errorMessage);
    bool insertTrackPlugin(int trackIndex, int slotIndex, const juce::File& file, juce::String& errorMessage);
    void unloadTrackPlugin(int trackIndex);
    void unloadTrackPlugin(int trackIndex, int slotIndex);
    bool moveTrackPlugin(int trackIndex, int fromSlotIndex, int toSlotIndex);
    juce::String getTrackPluginName(int trackIndex) const;
    juce::StringArray getTrackPluginNames(int trackIndex) const;
    juce::Array<bool> getTrackPluginBypassStates(int trackIndex) const;
    juce::File getTrackPluginFile(int trackIndex) const;
    bool hasTrackPlugin(int trackIndex) const noexcept;
    int getTrackPluginCount(int trackIndex) const noexcept;
    void setTrackPluginBypassed(int trackIndex, bool shouldBypass);
    void setTrackPluginBypassed(int trackIndex, int slotIndex, bool shouldBypass);
    bool isTrackPluginBypassed(int trackIndex) const noexcept;
    bool isTrackPluginBypassed(int trackIndex, int slotIndex) const noexcept;
    juce::AudioProcessorEditor* createTrackPluginEditor(int trackIndex);
    juce::AudioProcessorEditor* createTrackPluginEditor(int trackIndex, int slotIndex);
    juce::File getTrackInstrumentPluginFile(int trackIndex) const;
    bool reapplyTrackPluginState(int trackIndex, int slotIndex);

    // Plugin parameter enumeration/control, used so an Automation Track can target a parameter
    // inside any hosted plugin - Creation Station's own or any third-party VST3 - not just the
    // built-in track volume/pan controls. Count/Name/Value are message-thread queries (used to
    // populate the automation target picker); the realtime setter is the one meant to be driven
    // from the per-block automation pass and is safe to call from the audio thread.
    int getTrackPluginParameterCount(int trackIndex, int slotIndex) const;
    juce::String getTrackPluginParameterName(int trackIndex, int slotIndex, int paramIndex) const;
    float getTrackPluginParameterValue(int trackIndex, int slotIndex, int paramIndex) const;
    void setTrackPluginParameterValueRealtime(int trackIndex, int slotIndex, int paramIndex, float normalizedValue);
    void setTrackPluginBypassedRealtime(int trackIndex, int slotIndex, bool shouldBypass);

    // Offline-renders a MIDI clip's notes/CC through the given instrument plugin into a
    // temporary WAV file, so it can be scheduled for playback the same way as a recorded
    // audio clip. Uses a fresh plugin instance - never touches the live, real-time track chain.
    bool renderMidiClipToFile(const juce::File& instrumentPluginFile,
                              const std::vector<cs::MidiNoteEvent>& notes,
                              const std::vector<cs::MidiCCEvent>& ccEvents,
                              double tempoBpm,
                              double durationSeconds,
                              juce::File& outputFile,
                              juce::String& errorMessage) const;

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int totalNumInputChannels,
                                          float* const* outputChannelData,
                                          int totalNumOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    juce::ValueTree createSessionState() const;
    juce::String createHostedPluginStateSignature() const;
    bool restoreSessionState(const juce::ValueTree& sessionState, juce::String& errorMessage);

private:
    struct DemoTrackSource final : public juce::AudioSource
    {
        DemoTrackSource(juce::String trackName, double frequencyHz);

        void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
        void releaseResources() override;
        void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

        void setGain(float newGain) noexcept { gain.store(newGain); }
        void setPan(float newPan) noexcept { pan.store(newPan); }
        void setMuted(bool shouldMute) noexcept { muted.store(shouldMute); }
        void setSoloed(bool shouldSolo) noexcept { soloed.store(shouldSolo); }
        void setPlaying(bool shouldPlay) noexcept { playing.store(shouldPlay); }
        float getLevel() const noexcept { return level.load(); }
        float getGain() const noexcept { return gain.load(); }
        float getPan() const noexcept { return pan.load(); }
        bool isMuted() const noexcept { return muted.load(); }
        bool isSoloed() const noexcept { return soloed.load(); }

        juce::String name;

    private:
        juce::SmoothedValue<float> levelSmoother;
        std::atomic<float> gain { 0.75f };
        std::atomic<float> pan { 0.0f };
        std::atomic<bool> muted { false };
        std::atomic<bool> soloed { false };
        std::atomic<bool> playing { true };
        double sampleRate = 44100.0;
        double phase = 0.0;
        double frequency = 440.0;
        std::atomic<float> level { 0.0f };
    };

    struct PluginInsertSource final : public juce::AudioSource
    {
        PluginInsertSource();

        void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
        void releaseResources() override;
        void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

        bool loadPlugin(const juce::File& file, juce::String& errorMessage);
        bool loadPlugin(const juce::File& file,
                        const juce::MemoryBlock* savedState,
                        juce::String& errorMessage);
        void unloadPlugin();
        juce::String getPluginName() const;
        juce::File getPluginFile() const;
        bool hasPlugin() const noexcept { return pluginInstance != nullptr; }
        void setBypassed(bool shouldBypass) noexcept { bypassed.store(shouldBypass); }
        bool isBypassed() const noexcept { return bypassed.load(); }
        juce::AudioProcessorEditor* createEditor();
        bool copyStateTo(juce::MemoryBlock& destination) const;
        bool restoreStateFrom(const juce::MemoryBlock& source);
        bool reapplyCachedState();
        void addExternalMidi(const juce::MidiBuffer& midi) { pendingExternalMidi.addEvents(midi, 0, -1, 0); }

        // Generic JUCE parameter interface - works identically for Creation Station's own APVTS
        // plugins and any hosted third-party VST3. Count/Name/Value are message-thread queries;
        // setParameterValueRealtime is real-time-safe (try-lock, no allocation) and is the one
        // call site meant to be driven from the engine's per-block automation pass.
        int getParameterCount() const;
        juce::String getParameterName(int paramIndex) const;
        float getParameterValue(int paramIndex) const;
        bool setParameterValueRealtime(int paramIndex, float normalizedValue);

    private:
        juce::AudioPluginFormatManager formatManager;
        std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
        juce::AudioBuffer<float> pluginBuffer;
        juce::MidiBuffer pluginMidiBuffer;
        juce::MidiBuffer pendingExternalMidi;
        juce::File pluginFile;
        std::atomic<bool> bypassed { false };
        double sampleRate = 44100.0;
        int blockSize = 512;
        int pluginInputChannels = 2;
        int pluginOutputChannels = 2;
        juce::MemoryBlock cachedState;

        // Guards pluginInstance's lifetime (not its internal processing) against the audio thread.
        // loadPlugin()/unloadPlugin() run on the message thread and can swap or destroy the
        // instance at any moment; without this, getNextAudioBlock() (audio thread) can pass its
        // null check and then dereference a pointer that gets reset out from under it a moment
        // later - an unsynchronized use-after-free that crashed on plugin removal.
        juce::CriticalSection pluginInstanceLock;
    };

    struct PluginInsertChain final : public juce::AudioSource
    {
        void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
        void releaseResources() override;
        void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

        bool addPlugin(const juce::File& file, juce::String& errorMessage);
        bool addPlugin(const juce::File& file,
                       const juce::MemoryBlock* savedState,
                       bool bypassed,
                       juce::String& errorMessage);
        bool insertPlugin(int slotIndex,
                          const juce::File& file,
                          const juce::MemoryBlock* savedState,
                          bool bypassed,
                          juce::String& errorMessage);
        void removeLastPlugin();
        void removePlugin(int slotIndex);
        bool movePlugin(int fromSlotIndex, int toSlotIndex);
        void clear();
        int getPluginCount() const noexcept { return inserts.size(); }
        bool hasPlugin() const noexcept { return ! inserts.isEmpty(); }
        juce::String getPluginName(int slotIndex) const;
        juce::StringArray getPluginNames() const;
        juce::Array<bool> getBypassStates() const;
        juce::String getSummaryName() const;
        juce::File getPluginFile(int slotIndex) const;
        void setLastBypassed(bool shouldBypass) noexcept;
        void setBypassed(int slotIndex, bool shouldBypass) noexcept;
        bool isLastBypassed() const noexcept;
        bool isBypassed(int slotIndex) const noexcept;
        juce::AudioProcessorEditor* createLastEditor();
        juce::AudioProcessorEditor* createEditor(int slotIndex);
        bool copyStateTo(int slotIndex, juce::MemoryBlock& destination) const;
        void reapplyCachedStates();
        bool reapplyCachedState(int slotIndex);
        void pushLiveMidiToFirstSlot(const juce::MidiBuffer& midi);
        int getParameterCount(int slotIndex) const;
        juce::String getParameterName(int slotIndex, int paramIndex) const;
        float getParameterValue(int slotIndex, int paramIndex) const;
        bool setParameterValueRealtime(int slotIndex, int paramIndex, float normalizedValue);

    private:
        juce::OwnedArray<PluginInsertSource> inserts;
        double sampleRate = 44100.0;
        int blockSize = 512;
    };

    // Immutable snapshot of one automation track's target + curve, published by the message
    // thread and read lock-free by the audio thread once per block. A fresh snapshot is
    // published (not mutated in place) on every edit, so the audio thread never observes a
    // half-updated curve.
    struct AutomationTrackData
    {
        cs::AutomationTarget target;
        std::vector<cs::AutomationPoint> points;
    };

    // Immutable snapshot of the track hierarchy's audio-routing shape, published by the message
    // thread (rebuildTrackRoutingCache()) whenever a track's parent changes or the track count
    // changes, and read lock-free by the audio thread once per block - the render order itself is
    // never computed on the audio thread. renderOrder lists every track index children-before-
    // parents, so a folder's buffer already contains its children's contributions by the time the
    // folder's own turn comes up. isBusDestination[i] is true if some other track's parent is i -
    // such a track must fully process every block (insert chain/gain/pan/routing) even with no
    // clips of its own, since its buffer may already hold audio summed in from its children.
    struct TrackRoutingInfo
    {
        std::vector<int> renderOrder;
        std::vector<bool> isBusDestination;
    };

    struct TrackChannelSource final : public juce::AudioSource
    {
        TrackChannelSource(juce::String trackName, double frequencyHz);

        void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
        void releaseResources() override;
        void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

        void setGain(float newGain) noexcept { source.setGain(newGain); }
        void setPan(float newPan) noexcept { source.setPan(newPan); }
        void setMuted(bool shouldMute) noexcept { source.setMuted(shouldMute); }
        void setSoloed(bool shouldSolo) noexcept { source.setSoloed(shouldSolo); }
        void setPlaying(bool shouldPlay) noexcept { source.setPlaying(shouldPlay); }
        void setName(const juce::String& newName) { source.name = newName; }
        juce::String getName() const { return source.name; }
        float getLevel() const noexcept { return source.getLevel(); }
        float getGain() const noexcept { return source.getGain(); }
        float getPan() const noexcept { return source.getPan(); }
        bool isMuted() const noexcept { return source.isMuted(); }
        bool isSoloed() const noexcept { return source.isSoloed(); }
        void setInputChannel(int channel) noexcept { inputChannel.store(channel); }
        int getInputChannel() const noexcept { return inputChannel.load(); }
        void setMidiInputChannel(int channel) noexcept { midiInputChannel.store(channel); }
        int getMidiInputChannel() const noexcept { return midiInputChannel.load(); }

        // Which physical MIDI input device this track listens to - empty means "any enabled
        // device" (the original behaviour, before per-device routing existed). Set rarely from
        // the message thread (Settings), read once per audio block, so a short lock is fine.
        void setMidiInputDeviceId(const juce::String& deviceId)
        {
            const juce::ScopedLock lock(midiInputDeviceIdLock);
            midiInputDeviceId = deviceId;
        }

        juce::String getMidiInputDeviceId() const
        {
            const juce::ScopedLock lock(midiInputDeviceIdLock);
            return midiInputDeviceId;
        }
        void setIsMidiKind(bool shouldBeMidi) noexcept { isMidiKind.store(shouldBeMidi); }
        bool getIsMidiKind() const noexcept { return isMidiKind.load(); }
        void setIsAutomationKind(bool shouldBeAutomation) noexcept { isAutomationKind.store(shouldBeAutomation); }
        bool getIsAutomationKind() const noexcept { return isAutomationKind.load(); }
        // -1 = routes straight to master (today's only behaviour). >= 0 = this track's fully
        // processed output feeds that track's buffer instead - see ArrangementSource. Mirrors
        // cs::TimelineTrack::parentTrackIndex, pushed down whenever it changes.
        void setParentTrackIndex(int newParentTrackIndex) noexcept { parentTrackIndex.store(newParentTrackIndex); }
        int getParentTrackIndex() const noexcept { return parentTrackIndex.load(); }
        void setAutomationData(std::shared_ptr<const AutomationTrackData> data) { automationData.store(std::move(data)); }
        std::shared_ptr<const AutomationTrackData> getAutomationData() const { return automationData.load(); }
        // While true, applyAutomationForBlock skips this lane entirely, ceding its target's
        // control fully to whatever is manually setting it right now (a fader being ridden) -
        // otherwise the audio-thread automation pass and a live manual drag fight over the same
        // value every block. Set by MainComponent based on the lane's Touch/Latch/Write mode.
        void setAutomationWriteActive(bool active) noexcept { automationWriteActive.store(active); }
        bool getAutomationWriteActive() const noexcept { return automationWriteActive.load(); }
        void setHasOpenEditor(bool hasEditor) noexcept { hasOpenEditor.store(hasEditor); }
        bool getHasOpenEditor() const noexcept { return hasOpenEditor.load(); }
        void setRecordingArmed(bool shouldArm) noexcept { recordingArmed.store(shouldArm); }
        bool isRecordingArmed() const noexcept { return recordingArmed.load(); }
        void setMonitoringEnabled(bool shouldMonitor) noexcept { monitoringEnabled.store(shouldMonitor); }
        bool isMonitoringEnabled() const noexcept { return monitoringEnabled.load(); }
        void setStereoEnabled(bool shouldUseStereo) noexcept { stereoEnabled.store(shouldUseStereo); }
        bool isStereoEnabled() const noexcept { return stereoEnabled.load(); }
        void setInputLevel(float newLevel) noexcept { inputLevel.store(newLevel); }
        float getInputLevel() const noexcept { return inputLevel.load(); }
        void pushRecordingPeak(float peak) noexcept { recordingPeak.store(juce::jmax(recordingPeak.load(), peak)); }
        float consumeRecordingPeak() noexcept { return recordingPeak.exchange(0.0f); }

        DemoTrackSource source;
        PluginInsertChain insertChain;
        std::atomic<int> inputChannel { -1 };
        std::atomic<int> midiInputChannel { 0 };
        juce::String midiInputDeviceId;
        mutable juce::CriticalSection midiInputDeviceIdLock;
        std::atomic<bool> isMidiKind { false };
        std::atomic<bool> isAutomationKind { false };
        std::atomic<int> parentTrackIndex { -1 };
        std::atomic<std::shared_ptr<const AutomationTrackData>> automationData;
        std::atomic<bool> automationWriteActive { false };
        std::atomic<bool> hasOpenEditor { false };
        std::atomic<bool> recordingArmed { false };
        std::atomic<bool> monitoringEnabled { false };
        std::atomic<bool> stereoEnabled { false };
        std::atomic<float> inputLevel { 0.0f };
        std::atomic<float> recordingPeak { 0.0f };
    };

    struct MasterOutputSource final : public juce::AudioSource
    {
        MasterOutputSource(WorkstationAudioEngine& owner, juce::AudioSource& inputSource, PluginInsertSource& insertSource);

        void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
        void releaseResources() override;
        void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    private:
        WorkstationAudioEngine& owner;
        juce::AudioSource& source;
        PluginInsertSource& insert;
    };

    struct AssetPreviewSource final : public juce::AudioSource
    {
        AssetPreviewSource();

        void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
        void releaseResources() override;
        void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

        bool loadFile(const juce::File& file, const PreviewSettings& settings, juce::String& errorMessage);
        bool loadBuffer(const juce::AudioBuffer<float>& buffer, double sourceSampleRate, juce::String& errorMessage);
        void stop();
        bool isPreviewing() const noexcept { return previewing.load(); }
        juce::File getPreviewFile() const { return previewFile; }

    private:
        juce::AudioFormatManager formatManager;
        juce::AudioBuffer<float> previewBuffer;
        juce::File previewFile;
        std::atomic<bool> previewing { false };
        int playbackPosition = 0;
        double sampleRate = 44100.0;
        int blockSize = 512;
    };

    struct ArrangementSource final : public juce::AudioSource
    {
        struct Clip
        {
            juce::AudioBuffer<float> buffer;
            int64 startSample = 0;
            int trackIndex = -1;
        };

        explicit ArrangementSource(WorkstationAudioEngine& owner);

        void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
        void releaseResources() override;
        void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
        void resetPlayback() noexcept;
        void setPlaybackPositionSeconds(double seconds) noexcept;
        void setClips(juce::Array<Clip> newClips);
        int64 getPlaybackSamplePosition() const noexcept { return playbackSamplePosition; }
        double getSampleRate() const noexcept { return sampleRate; }

    private:
        WorkstationAudioEngine& owner;
        juce::CriticalSection lock;
        juce::Array<Clip> clips;
        std::vector<juce::AudioBuffer<float>> perTrackBuffers; // one per real track, persists for the whole block so children can sum into a parent's buffer before it's finalized
        int64 playbackSamplePosition = 0;
        double sampleRate = 44100.0;
        int blockSize = 512;
    };

    static constexpr int visibleChannelCount = 8;
    static constexpr int echoBufferSize = 4410;

    void prepareGraph(double sampleRate, int blockSize);
    void processGraph(juce::AudioBuffer<float>& buffer);
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    bool shouldRenderTrack(int trackIndex) const noexcept;
    // True if trackIndex itself is soloed, or any track anywhere in its descendant chain
    // (child, grandchild, ...) is soloed - without this, soloing a track nested inside a
    // non-soloed folder would get silenced by its own parent's solo gate before ever reaching
    // master, muting the very track you just soloed.
    bool isTrackOrDescendantSoloed(int trackIndex) const noexcept;
    // Message-thread only: recomputes the children-before-parents render order (post-order walk
    // from each root) plus which tracks are bus destinations, and publishes it for the audio
    // thread. Called whenever a track's parent changes or the track count changes.
    void rebuildTrackRoutingCache();
    std::shared_ptr<const TrackRoutingInfo> getCachedTrackRouting() const { return cachedTrackRouting.load(); }
    // Evaluates every automation-kind track's curve at the given block-start transport position
    // and applies the result directly to its target track's gain/pan atomics or plugin parameter.
    // Must run before any track's own rendering in the same audio callback so the value takes
    // effect for that block (block-accurate, not one block delayed).
    void applyAutomationForBlock(double blockStartSeconds);
    bool anyTrackNeedsLiveMonitoring() const noexcept;
    void writeRecording(int trackIndex, const float* leftSource, const float* rightSource, int numSamples);
    void clearTracks();
    void rebuildInputSources(int totalNumInputChannels);
    void rebuildInputSources(juce::AudioIODevice& device);
    void processInputRouting(const float* const* inputChannelData,
                             int totalNumInputChannels,
                             float* const* outputChannelData,
                             int totalNumOutputChannels,
                             int numSamples);
    void renderMetronome(float* const* outputChannelData,
                         int totalNumOutputChannels,
                         int numSamples);

    juce::AudioBuffer<float> callbackRenderBuffer;
    juce::AudioBuffer<float> callbackRecordBuffer;
    juce::Array<InputSourceDescriptor> inputSources;
    juce::MixerAudioSource mixerSource;
    AssetPreviewSource assetPreviewSource;
    PatchLiveVoice patchLiveVoice;
    ArrangementSource arrangementSource;
    PluginInsertSource masterInsertSource;
    PluginInsertSource graphVstInsertSource;
    MasterOutputSource masterOutputSource;
    juce::OwnedArray<TrackChannelSource> tracks;
    std::atomic<std::shared_ptr<const TrackRoutingInfo>> cachedTrackRouting;
    std::vector<MidiPlaybackClip> scheduledMidiClips;
    juce::CriticalSection scheduledMidiClipsLock;
    struct AuditionRequest { int trackIndex = -1; int pitch = 60; int velocity = 100; bool noteOn = true; };
    std::vector<AuditionRequest> pendingAuditionRequests;
    juce::CriticalSection auditionRequestsLock;
    std::atomic<bool> allNotesOffRequested { false };
    std::atomic<bool> midiRecordingActive { false };
    std::vector<RecordedMidiEvent> recordedMidiEvents;
    juce::CriticalSection recordedMidiEventsLock;

    struct MidiLearnState
    {
        bool armed = false;
        juce::String deviceIdFilter;
        MidiLearnKind expectedKind = MidiLearnKind::Any;
        bool hasResult = false;
        MidiLearnResult result;
    };
    mutable juce::CriticalSection midiLearnLock;
    MidiLearnState midiLearnState;

    struct LiveMidiControlState
    {
        juce::Array<LiveMidiControlChange> pendingChanges;
    };
    mutable juce::CriticalSection liveMidiControlLock;
    LiveMidiControlState liveMidiControlState;
    // Kept open for a few seconds after the last delivered note, not just the exact block a
    // note-on/off fell in - otherwise a drum hit would be truncated to one audio block (~10-20ms)
    // instead of being allowed to ring out. Only touched from the audio thread.
    int64 liveAudioTailSamplesRemaining = 0;

    // One collector per physical MIDI input device, keyed by juce::MidiDeviceInfo::identifier -
    // replaces a single shared collector so incoming messages keep their device identity, which
    // per-track device routing (setTrackMidiInputDeviceId) needs to filter by. Devices are added
    // in attachToDevice()/on first message from an unrecognised device; only ever mutated from
    // the message thread, so the audio thread only ever needs a ScopedTryLock to iterate safely.
    struct MidiDeviceCollector
    {
        juce::String deviceId;
        std::unique_ptr<juce::MidiMessageCollector> collector;
    };
    std::vector<MidiDeviceCollector> midiDeviceCollectors;
    juce::CriticalSection midiDeviceCollectorsLock;
    juce::MidiMessageCollector& getOrCreateMidiDeviceCollector(const juce::String& deviceId);

    juce::AudioDeviceManager* attachedDeviceManager = nullptr;
    static bool isControlSurfaceMidiDevice(const juce::String& deviceName);
    std::array<float, 2> lowPassState {};
    std::array<float, echoBufferSize> echoHistoryLeft {};
    std::array<float, echoBufferSize> echoHistoryRight {};
    int echoWritePosition = 0;
    double graphSampleRate = 44100.0;
    int graphBlockSize = 512;
    SignalGraphRuntime signalGraph;
    std::atomic<bool> playing { false };
    std::atomic<bool> recording { false };
    std::atomic<bool> metronomeEnabled { false };
    std::atomic<MetronomeMode> metronomeMode { MetronomeMode::off };
    std::atomic<double> metronomeBpm { 120.0 };
    std::atomic<int> metronomeBeatsPerMeasure { 4 };
    int64 metronomeSampleCounter = 0;
    std::atomic<float> masterGain { 0.8f };
    std::atomic<bool> graphEnabled { true };
    std::atomic<float> graphInput { 0.0f };
    std::atomic<float> graphSourceFrequency { 220.0f };
    std::atomic<float> graphDrive { 0.15f };
    std::atomic<float> graphTone { 0.55f };
    std::atomic<float> graphEcho { 0.08f };
    std::atomic<float> graphWidth { 0.5f };
    std::atomic<bool> graphVstEnabled { true };
    std::atomic<float> graphVstMix { 0.5f };
    juce::TimeSliceThread recordingThread { "CreationStationRecorder" };
    mutable juce::CriticalSection recordingLock;
    struct TrackRecordingWriter
    {
        int trackIndex = -1;
        int numChannels = 1;
        juce::File file;
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    };

    std::vector<TrackRecordingWriter> recordingWriters;
    juce::File recordingFile;
};
