#include "WorkstationAudioEngine.h"

#include <cmath>

namespace
{
float panToLeftGain(float pan) noexcept
{
    return juce::jlimit(0.0f, 1.0f, 0.5f * (1.0f - pan));
}

float panToRightGain(float pan) noexcept
{
    return juce::jlimit(0.0f, 1.0f, 0.5f * (1.0f + pan));
}
}

WorkstationAudioEngine::DemoTrackSource::DemoTrackSource(juce::String trackName, double frequencyHz)
    : name(std::move(trackName)), frequency(frequencyHz)
{
}

void WorkstationAudioEngine::DemoTrackSource::prepareToPlay(int, double newSampleRate)
{
    sampleRate = newSampleRate;
    phase = 0.0;
    levelSmoother.reset(sampleRate, 0.08);
    levelSmoother.setCurrentAndTargetValue(0.0f);
}

void WorkstationAudioEngine::DemoTrackSource::releaseResources()
{
}

void WorkstationAudioEngine::DemoTrackSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr || bufferToFill.buffer->getNumChannels() < 2)
        return;

    auto* left = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    auto* right = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);
    auto numSamples = bufferToFill.numSamples;

    auto currentGain = gain.load();
    auto currentPan = pan.load();
    auto leftGain = panToLeftGain(currentPan);
    auto rightGain = panToRightGain(currentPan);
    auto isMuted = muted.load();
    auto isPlaying = playing.load();

    float peak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto value = 0.0f;

        if (isPlaying && ! isMuted)
        {
            value = static_cast<float>(std::sin(phase)) * currentGain;
            phase += juce::MathConstants<double>::twoPi * frequency / sampleRate;

            if (phase > juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }

        left[sample] += value * leftGain;
        right[sample] += value * rightGain;
        peak = juce::jmax(peak, std::abs(value));
    }

    levelSmoother.setTargetValue(peak);
    level.store(levelSmoother.getNextValue());
}

WorkstationAudioEngine::WorkstationAudioEngine()
{
    tracks.add(new DemoTrackSource("Drums", 110.0));
    tracks.add(new DemoTrackSource("Bass", 55.0));
    tracks.add(new DemoTrackSource("Keys", 220.0));
    tracks.add(new DemoTrackSource("Lead", 330.0));
    tracks.add(new DemoTrackSource("Vox", 440.0));
    tracks.add(new DemoTrackSource("FX", 550.0));
    tracks.add(new DemoTrackSource("Perc", 660.0));
    tracks.add(new DemoTrackSource("Aux", 770.0));

    for (auto* track : tracks)
        mixerSource.addInputSource(track, false);

    audioSourcePlayer.setSource(&mixerSource);
}

void WorkstationAudioEngine::attachToDevice(juce::AudioDeviceManager& deviceManager)
{
    deviceManager.addAudioCallback(&audioSourcePlayer);
}

void WorkstationAudioEngine::detachFromDevice(juce::AudioDeviceManager& deviceManager)
{
    deviceManager.removeAudioCallback(&audioSourcePlayer);
}

void WorkstationAudioEngine::setPlaying(bool shouldPlay)
{
    playing.store(shouldPlay);

    for (auto* track : tracks)
        track->setPlaying(shouldPlay);
}

void WorkstationAudioEngine::setTrackGain(int trackIndex, float gain)
{
    if (auto* track = tracks[juce::jlimit(0, tracks.size() - 1, trackIndex)])
        track->setGain(gain);
}

void WorkstationAudioEngine::setTrackPan(int trackIndex, float pan)
{
    if (auto* track = tracks[juce::jlimit(0, tracks.size() - 1, trackIndex)])
        track->setPan(pan);
}

void WorkstationAudioEngine::setTrackMuted(int trackIndex, bool shouldMute)
{
    if (auto* track = tracks[juce::jlimit(0, tracks.size() - 1, trackIndex)])
        track->setMuted(shouldMute);
}

void WorkstationAudioEngine::setTrackSoloed(int trackIndex, bool shouldSolo)
{
    if (auto* track = tracks[juce::jlimit(0, tracks.size() - 1, trackIndex)])
        track->setSoloed(shouldSolo);
}

void WorkstationAudioEngine::setMasterGain(float gain)
{
    audioSourcePlayer.setGain(gain);
}
