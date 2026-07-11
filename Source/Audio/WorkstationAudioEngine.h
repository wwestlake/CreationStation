#pragma once

#include <JuceHeader.h>
#include <atomic>

class WorkstationAudioEngine final
{
public:
    WorkstationAudioEngine();

    void attachToDevice(juce::AudioDeviceManager& deviceManager);
    void detachFromDevice(juce::AudioDeviceManager& deviceManager);

    void setPlaying(bool shouldPlay);
    bool isPlaying() const noexcept { return playing; }

    void setTrackGain(int trackIndex, float gain);
    void setTrackPan(int trackIndex, float pan);
    void setTrackMuted(int trackIndex, bool shouldMute);
    void setTrackSoloed(int trackIndex, bool shouldSolo);
    void setMasterGain(float gain);

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

    static constexpr int trackCount = 4;

    juce::AudioSourcePlayer audioSourcePlayer;
    juce::MixerAudioSource mixerSource;
    juce::OwnedArray<DemoTrackSource> tracks;
    std::atomic<bool> playing { false };
};
