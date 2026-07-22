#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "SignalGraphRuntime.h"

class WorkstationAudioEngine final : public juce::AudioIODeviceCallback
{
public:
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
    void setMetronomeEnabled(bool shouldEnable) noexcept { metronomeEnabled.store(shouldEnable); }
    bool isMetronomeEnabled() const noexcept { return metronomeEnabled.load(); }
    void setMetronomeTempo(double bpm, int numerator) noexcept;
    bool isRecording() const noexcept { return recording; }
    bool startRecordingToFile(const juce::File& file, juce::String& errorMessage);
    bool startRecordingToFiles(const juce::Array<RecordingTarget>& targets, juce::String& errorMessage);
    void stopRecording();
    juce::File getRecordingFile() const { return recordingFile; }
    juce::Array<juce::File> getRecordingFiles() const;
    bool previewAssetFile(const juce::File& file, juce::String& errorMessage);
    bool previewAssetFile(const juce::File& file, const PreviewSettings& settings, juce::String& errorMessage);
    bool previewGeneratedBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate, juce::String& errorMessage);
    bool setFoleyArrangement(const juce::ValueTree& arrangementState, const juce::File& assetsDirectory, juce::String& errorMessage);
    bool setTrackerPlaybackClips(const juce::Array<PlaybackClipTarget>& targets, juce::String& errorMessage);
    bool renderTrackerMixToBuffer(const juce::Array<PlaybackClipTarget>& targets,
                                  double durationSeconds,
                                  const RenderSettings& settings,
                                  juce::AudioBuffer<float>& outputBuffer,
                                  juce::String& errorMessage);
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
    void unloadTrackPlugin(int trackIndex);
    juce::String getTrackPluginName(int trackIndex) const;
    juce::File getTrackPluginFile(int trackIndex) const;
    bool hasTrackPlugin(int trackIndex) const noexcept;
    int getTrackPluginCount(int trackIndex) const noexcept;
    void setTrackPluginBypassed(int trackIndex, bool shouldBypass);
    bool isTrackPluginBypassed(int trackIndex) const noexcept;
    juce::AudioProcessorEditor* createTrackPluginEditor(int trackIndex);

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int totalNumInputChannels,
                                          float* const* outputChannelData,
                                          int totalNumOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    juce::ValueTree createSessionState() const;
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

    private:
        juce::AudioPluginFormatManager formatManager;
        std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
        juce::AudioBuffer<float> pluginBuffer;
        juce::MidiBuffer pluginMidiBuffer;
        juce::File pluginFile;
        std::atomic<bool> bypassed { false };
        double sampleRate = 44100.0;
        int blockSize = 512;
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
        void removeLastPlugin();
        void clear();
        int getPluginCount() const noexcept { return inserts.size(); }
        bool hasPlugin() const noexcept { return ! inserts.isEmpty(); }
        juce::String getPluginName(int slotIndex) const;
        juce::String getSummaryName() const;
        juce::File getPluginFile(int slotIndex) const;
        void setLastBypassed(bool shouldBypass) noexcept;
        bool isLastBypassed() const noexcept;
        juce::AudioProcessorEditor* createLastEditor();
        bool copyStateTo(int slotIndex, juce::MemoryBlock& destination) const;

    private:
        juce::OwnedArray<PluginInsertSource> inserts;
        double sampleRate = 44100.0;
        int blockSize = 512;
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

    private:
        WorkstationAudioEngine& owner;
        juce::CriticalSection lock;
        juce::Array<Clip> clips;
        juce::AudioBuffer<float> trackRenderBuffer;
        int64 playbackSamplePosition = 0;
        double sampleRate = 44100.0;
        int blockSize = 512;
    };

    static constexpr int visibleChannelCount = 8;
    static constexpr int echoBufferSize = 4410;

    void prepareGraph(double sampleRate, int blockSize);
    void processGraph(juce::AudioBuffer<float>& buffer);
    bool shouldRenderTrack(int trackIndex) const noexcept;
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
    ArrangementSource arrangementSource;
    PluginInsertSource masterInsertSource;
    PluginInsertSource graphVstInsertSource;
    MasterOutputSource masterOutputSource;
    juce::OwnedArray<TrackChannelSource> tracks;
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
