#pragma once

#include <JuceHeader.h>
#include <atomic>

class RouterAudioEngine final : private juce::AudioIODeviceCallback
{
public:
    RouterAudioEngine();
    ~RouterAudioEngine() override;

    void setRouteSourceIndex(int newIndex);
    int getRouteSourceIndex() const noexcept { return routeSourceIndex.load(); }

    void setOutputMuted(bool shouldMute) noexcept { outputMuted.store(shouldMute); }
    bool isOutputMuted() const noexcept { return outputMuted.load(); }

    juce::StringArray getInputDeviceNames() const;
    juce::StringArray getOutputDeviceNames() const;
    juce::String getCurrentInputDeviceName() const;
    juce::String getCurrentOutputDeviceName() const;

    bool setInputDeviceByName(const juce::String& name);
    bool setOutputDeviceByName(const juce::String& name);

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    juce::AudioDeviceManager deviceManager;
    std::atomic<int> routeSourceIndex { 0 };
    std::atomic<bool> outputMuted { false };
    double sampleRate = 44100.0;
    int blockSize = 512;
};
