#include "RouterAudioEngine.h"

RouterAudioEngine::RouterAudioEngine()
{
    deviceManager.initialise(2, 2, nullptr, true, {}, nullptr);
    deviceManager.addAudioCallback(this);
}

RouterAudioEngine::~RouterAudioEngine()
{
    deviceManager.removeAudioCallback(this);
}

void RouterAudioEngine::setRouteSourceIndex(int newIndex)
{
    routeSourceIndex.store(juce::jmax(0, newIndex));
}

juce::StringArray RouterAudioEngine::getInputDeviceNames() const
{
    juce::StringArray names;

    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        names = type->getDeviceNames(true);

    return names;
}

juce::StringArray RouterAudioEngine::getOutputDeviceNames() const
{
    juce::StringArray names;

    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        names = type->getDeviceNames(false);

    return names;
}

juce::String RouterAudioEngine::getCurrentInputDeviceName() const
{
    return deviceManager.getAudioDeviceSetup().inputDeviceName;
}

juce::String RouterAudioEngine::getCurrentOutputDeviceName() const
{
    return deviceManager.getAudioDeviceSetup().outputDeviceName;
}

bool RouterAudioEngine::setInputDeviceByName(const juce::String& name)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = name;
    return deviceManager.setAudioDeviceSetup(setup, true).isEmpty();
}

bool RouterAudioEngine::setOutputDeviceByName(const juce::String& name)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.outputDeviceName = name;
    return deviceManager.setAudioDeviceSetup(setup, true).isEmpty();
}

void RouterAudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    sampleRate = device->getCurrentSampleRate();
    blockSize = device->getCurrentBufferSizeSamples();
    juce::ignoreUnused(sampleRate, blockSize);
}

void RouterAudioEngine::audioDeviceStopped()
{
}

void RouterAudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                         int numInputChannels,
                                                         float* const* outputChannelData,
                                                         int numOutputChannels,
                                                         int numSamples,
                                                         const juce::AudioIODeviceCallbackContext&)
{
    for (int channel = 0; channel < numOutputChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);

    if (outputMuted.load())
        return;

    const auto sourceIndex = routeSourceIndex.load();
    const auto* inputLeft = numInputChannels > 0 ? inputChannelData[0] : nullptr;
    const auto* inputRight = numInputChannels > 1 ? inputChannelData[1] : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float left = 0.0f;
        float right = 0.0f;

        switch (sourceIndex)
        {
            case 1: // Blue Yeti
            case 2: // Blue Snowball
                left = right = inputLeft != nullptr ? inputLeft[sample] : 0.0f;
                break;

            case 3: // Behringer split
                left = inputLeft != nullptr ? inputLeft[sample] : 0.0f;
                right = inputRight != nullptr ? inputRight[sample] : left;
                break;

            case 4: // System Audio
                left = inputLeft != nullptr ? inputLeft[sample] : 0.0f;
                right = inputRight != nullptr ? inputRight[sample] : left;
                break;

            default: // Reaper monitor or fallback
                left = inputLeft != nullptr ? inputLeft[sample] : 0.0f;
                right = inputRight != nullptr ? inputRight[sample] : left;
                break;
        }

        if (numOutputChannels > 0 && outputChannelData[0] != nullptr)
            outputChannelData[0][sample] = left;
        if (numOutputChannels > 1 && outputChannelData[1] != nullptr)
            outputChannelData[1][sample] = right;
    }
}
