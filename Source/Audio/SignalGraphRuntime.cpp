#include "SignalGraphRuntime.h"

#include <cmath>

namespace
{
float clampUnit(float value) noexcept
{
    return juce::jlimit(0.0f, 1.0f, value);
}

float mapToneToCutoff(float tone) noexcept
{
    return juce::jmap(clampUnit(tone), 260.0f, 12000.0f);
}
}

SignalGraphRuntime::SignalGraphRuntime()
{
    formatManager.registerBasicFormats();
}

void SignalGraphRuntime::prepare(double newSampleRate, int newBlockSize)
{
    sampleRate = newSampleRate;
    blockSize = newBlockSize;
    oscillatorPhase = 0.0;
    lowPassState = {};
    echoHistoryLeft.fill(0.0f);
    echoHistoryRight.fill(0.0f);
    echoWritePosition = 0;

    fileTransport.prepareToPlay(blockSize, sampleRate);
}

void SignalGraphRuntime::reset()
{
    oscillatorPhase = 0.0;
    lowPassState = {};
    echoHistoryLeft.fill(0.0f);
    echoHistoryRight.fill(0.0f);
    echoWritePosition = 0;
}

bool SignalGraphRuntime::loadFileSource(const juce::File& file, juce::String& errorMessage)
{
    clearFileSource();

    if (! file.existsAsFile())
    {
        errorMessage = "The selected file does not exist.";
        return false;
    }

    auto* reader = formatManager.createReaderFor(file);
    if (reader == nullptr)
    {
        errorMessage = "That file could not be opened as audio.";
        return false;
    }

    fileReaderSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
    fileTransport.setSource(fileReaderSource.get(), 0, nullptr, reader->sampleRate);
    fileTransport.prepareToPlay(blockSize, sampleRate);
    fileTransport.start();
    fileSourceEnabled.store(true);
    return true;
}

void SignalGraphRuntime::clearFileSource()
{
    fileTransport.stop();
    fileTransport.setSource(nullptr);
    fileReaderSource.reset();
    fileSourceEnabled.store(false);
}

float SignalGraphRuntime::renderSourceSample()
{
    auto level = clampUnit(sourceLevel.load());
    auto frequency = juce::jlimit(10.0f, 2000.0f, sourceFrequency.load());
    auto waveform = static_cast<SourceWaveform>(sourceWaveform.load());

    auto sample = 0.0f;
    auto phaseIncrement = juce::MathConstants<double>::twoPi * frequency / juce::jmax(1.0, sampleRate);

    switch (waveform)
    {
        case SourceWaveform::sine:
            sample = static_cast<float>(std::sin(oscillatorPhase));
            break;
        case SourceWaveform::saw:
            sample = static_cast<float>((oscillatorPhase / juce::MathConstants<double>::twoPi) * 2.0 - 1.0);
            break;
        case SourceWaveform::square:
            sample = std::sin(oscillatorPhase) >= 0.0 ? 1.0f : -1.0f;
            break;
        case SourceWaveform::triangle:
        {
            auto normalized = static_cast<float>(oscillatorPhase / juce::MathConstants<double>::twoPi);
            sample = 2.0f * std::abs(2.0f * (normalized - std::floor(normalized + 0.5f))) - 1.0f;
            break;
        }
    }

    oscillatorPhase += phaseIncrement;
    if (oscillatorPhase >= juce::MathConstants<double>::twoPi)
        oscillatorPhase -= juce::MathConstants<double>::twoPi;

    return sample * level * 0.2f;
}

void SignalGraphRuntime::applyDriveAndTone(juce::AudioBuffer<float>& buffer)
{
    auto toneAmount = clampUnit(tone.load());
    auto driveAmount = clampUnit(drive.load());
    auto cutoff = mapToneToCutoff(toneAmount);
    auto safeSampleRate = juce::jmax(1.0, sampleRate);
    auto alpha = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float) safeSampleRate);

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto dryLeft = left[sample];
        auto dryRight = right[sample];

        auto mono = 0.5f * (dryLeft + dryRight);
        auto driven = std::tanh(mono * (1.0f + driveAmount * 8.0f));

        auto filterInputLeft = dryLeft + driven * driveAmount;
        auto filterInputRight = dryRight + driven * driveAmount;
        lowPassState[0] = alpha * lowPassState[0] + (1.0f - alpha) * filterInputLeft;
        lowPassState[1] = alpha * lowPassState[1] + (1.0f - alpha) * filterInputRight;

        left[sample] = lowPassState[0];
        right[sample] = lowPassState[1];
    }
}

void SignalGraphRuntime::applyDelayAndWidth(juce::AudioBuffer<float>& buffer)
{
    auto echoAmount = clampUnit(echo.load());
    auto widthAmount = clampUnit(width.load());
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto currentLeft = left[sample];
        auto currentRight = right[sample];
        auto cross = (currentLeft + currentRight) * 0.5f;
        auto widthLeft = cross + (currentLeft - cross) * widthAmount;
        auto widthRight = cross + (currentRight - cross) * widthAmount;

        auto delayedLeft = echoHistoryLeft[(size_t) echoWritePosition];
        auto delayedRight = echoHistoryRight[(size_t) echoWritePosition];
        echoHistoryLeft[(size_t) echoWritePosition] = widthLeft;
        echoHistoryRight[(size_t) echoWritePosition] = widthRight;
        echoWritePosition = (echoWritePosition + 1) % (int) echoHistoryLeft.size();

        auto dryMix = 1.0f - echoAmount;
        auto wetMix = echoAmount;
        left[sample] = juce::jlimit(-1.0f, 1.0f, (dryMix * widthLeft) + (wetMix * delayedLeft));
        right[sample] = juce::jlimit(-1.0f, 1.0f, (dryMix * widthRight) + (wetMix * delayedRight));
    }
}

void SignalGraphRuntime::render(juce::AudioBuffer<float>& buffer)
{
    if (! enabled.load())
        return;

    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    auto sourceGain = clampUnit(sourceLevel.load());
    auto fileEnabled = fileSourceEnabled.load() && fileReaderSource != nullptr;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto sourceSample = renderSourceSample() * sourceGain;
        buffer.addSample(0, sample, sourceSample);
        buffer.addSample(1, sample, sourceSample);
    }

    if (fileEnabled)
    {
        fileRenderBuffer.setSize(juce::jmax(2, buffer.getNumChannels()), buffer.getNumSamples(), false, false, true);
        fileRenderBuffer.clear();

        juce::AudioSourceChannelInfo info(&fileRenderBuffer, 0, buffer.getNumSamples());
        fileTransport.getNextAudioBlock(info);

        for (int channel = 0; channel < juce::jmin(buffer.getNumChannels(), fileRenderBuffer.getNumChannels()); ++channel)
            buffer.addFrom(channel, 0, fileRenderBuffer, channel, 0, buffer.getNumSamples());
    }

    applyDriveAndTone(buffer);
    applyDelayAndWidth(buffer);

    buffer.applyGain(masterGain.load());
}
