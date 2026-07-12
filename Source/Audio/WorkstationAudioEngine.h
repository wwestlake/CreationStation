#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "SignalGraphRuntime.h"

class WorkstationAudioEngine final
{
public:
    WorkstationAudioEngine();

    void attachToDevice(juce::AudioDeviceManager& deviceManager);
    void detachFromDevice(juce::AudioDeviceManager& deviceManager);

    void setPlaying(bool shouldPlay);
    bool isPlaying() const noexcept { return playing; }

    static constexpr int getVisibleChannelCount() noexcept { return visibleChannelCount; }
    static constexpr int getTrackCount() noexcept { return trackCount; }

    juce::String getTrackName(int trackIndex) const;
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
    void setGraphTone(float amount);
    float getGraphTone() const noexcept { return graphTone.load(); }
    void setGraphEcho(float amount);
    float getGraphEcho() const noexcept { return graphEcho.load(); }
    void setGraphWidth(float amount);
    float getGraphWidth() const noexcept { return graphWidth.load(); }

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
    void setTrackPluginBypassed(int trackIndex, bool shouldBypass);
    bool isTrackPluginBypassed(int trackIndex) const noexcept;
    juce::AudioProcessorEditor* createTrackPluginEditor(int trackIndex);

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
        juce::String getName() const { return source.name; }
        float getGain() const noexcept { return source.getGain(); }
        float getPan() const noexcept { return source.getPan(); }
        bool isMuted() const noexcept { return source.isMuted(); }
        bool isSoloed() const noexcept { return source.isSoloed(); }

        DemoTrackSource source;
        PluginInsertSource insert;
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

    static constexpr int trackCount = 32;
    static constexpr int visibleChannelCount = 8;
    static constexpr int echoBufferSize = 4410;

    void prepareGraph(double sampleRate, int blockSize);
    void processGraph(juce::AudioBuffer<float>& buffer);

    juce::AudioSourcePlayer audioSourcePlayer;
    juce::MixerAudioSource mixerSource;
    PluginInsertSource masterInsertSource;
    MasterOutputSource masterOutputSource;
    juce::OwnedArray<TrackChannelSource> tracks;
    std::array<float, 2> lowPassState {};
    std::array<float, echoBufferSize> echoHistoryLeft {};
    std::array<float, echoBufferSize> echoHistoryRight {};
    int echoWritePosition = 0;
    double graphSampleRate = 44100.0;
    SignalGraphRuntime signalGraph;
    std::atomic<bool> playing { false };
    std::atomic<float> masterGain { 0.8f };
    std::atomic<bool> graphEnabled { true };
    std::atomic<float> graphInput { 0.85f };
    std::atomic<float> graphDrive { 0.15f };
    std::atomic<float> graphTone { 0.55f };
    std::atomic<float> graphEcho { 0.08f };
    std::atomic<float> graphWidth { 0.5f };
};
