#include "WorkstationAudioEngine.h"

#include <cmath>
#include <array>
#include <algorithm>

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

juce::String createDefaultTrackName(int trackIndex)
{
    return "Tk-" + juce::String(trackIndex + 1).paddedLeft('0', 4);
}

constexpr double foleySecondsPerBeat = 0.5;
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
    playing.store(false);
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
            value = static_cast<float>(std::sin(phase)) * currentGain * 0.03f;
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

WorkstationAudioEngine::PluginInsertSource::PluginInsertSource()
{
    formatManager.addDefaultFormats();
}

void WorkstationAudioEngine::PluginInsertSource::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
    sampleRate = newSampleRate;
    blockSize = samplesPerBlockExpected;

    if (pluginInstance != nullptr)
    {
        pluginInstance->setPlayConfigDetails(2, 2, sampleRate, blockSize);
        pluginInstance->prepareToPlay(sampleRate, blockSize);
    }
}

void WorkstationAudioEngine::PluginInsertSource::releaseResources()
{
    if (pluginInstance != nullptr)
        pluginInstance->releaseResources();
}

bool WorkstationAudioEngine::PluginInsertSource::loadPlugin(const juce::File& file, juce::String& errorMessage)
{
    return loadPlugin(file, nullptr, errorMessage);
}

bool WorkstationAudioEngine::PluginInsertSource::loadPlugin(const juce::File& file,
                                                            const juce::MemoryBlock* savedState,
                                                            juce::String& errorMessage)
{
    unloadPlugin();

    juce::OwnedArray<juce::PluginDescription> pluginDescriptions;

    for (auto* format : formatManager.getFormats())
    {
        if (format == nullptr)
            continue;

        if (format->fileMightContainThisPluginType(file.getFullPathName()))
            format->findAllTypesForFile(pluginDescriptions, file.getFullPathName());
    }

    if (pluginDescriptions.isEmpty())
    {
        errorMessage = "No compatible VST plugin format was found for that file.";
        return false;
    }

    for (auto* pluginDescription : pluginDescriptions)
    {
        if (pluginDescription == nullptr)
            continue;

        juce::String creationError;
        auto instance = formatManager.createPluginInstance(*pluginDescription, sampleRate, blockSize, creationError);

        if (instance != nullptr)
        {
            instance->setPlayConfigDetails(2, 2, sampleRate, blockSize);
            instance->prepareToPlay(sampleRate, blockSize);

            if (savedState != nullptr && savedState->getSize() > 0)
                instance->setStateInformation(savedState->getData(), (int) savedState->getSize());

            pluginInstance = std::move(instance);
            pluginFile = file;
            bypassed.store(false);
            return true;
        }

        if (creationError.isNotEmpty())
            errorMessage = creationError;
    }

    if (errorMessage.isEmpty())
        errorMessage = "JUCE could not create a plugin instance from that file.";

    return false;
}

void WorkstationAudioEngine::PluginInsertSource::unloadPlugin()
{
    if (pluginInstance != nullptr)
    {
        pluginInstance->releaseResources();
        pluginInstance.reset();
    }

    pluginFile = {};
    bypassed.store(false);
}

juce::String WorkstationAudioEngine::PluginInsertSource::getPluginName() const
{
    return pluginInstance != nullptr ? pluginInstance->getName() : juce::String();
}

juce::File WorkstationAudioEngine::PluginInsertSource::getPluginFile() const
{
    return pluginFile;
}

juce::AudioProcessorEditor* WorkstationAudioEngine::PluginInsertSource::createEditor()
{
    return pluginInstance != nullptr ? pluginInstance->createEditorIfNeeded() : nullptr;
}

bool WorkstationAudioEngine::PluginInsertSource::copyStateTo(juce::MemoryBlock& destination) const
{
    destination.reset();

    if (pluginInstance == nullptr)
        return false;

    pluginInstance->getStateInformation(destination);
    return destination.getSize() > 0;
}

bool WorkstationAudioEngine::PluginInsertSource::restoreStateFrom(const juce::MemoryBlock& source)
{
    if (pluginInstance == nullptr || source.getSize() == 0)
        return false;

    pluginInstance->setStateInformation(source.getData(), (int) source.getSize());
    return true;
}

void WorkstationAudioEngine::PluginInsertSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;

    if (pluginInstance == nullptr)
        return;

    pluginBuffer.setSize(juce::jmax(2, bufferToFill.buffer->getNumChannels()),
                         bufferToFill.numSamples,
                         false,
                         false,
                         true);

    for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
        pluginBuffer.copyFrom(channel, 0, *bufferToFill.buffer, channel, bufferToFill.startSample, bufferToFill.numSamples);

    for (int channel = bufferToFill.buffer->getNumChannels(); channel < pluginBuffer.getNumChannels(); ++channel)
        pluginBuffer.clear(channel, 0, bufferToFill.numSamples);

    pluginMidiBuffer.clear();
    if (bypassed.load())
        pluginInstance->processBlockBypassed(pluginBuffer, pluginMidiBuffer);
    else
        pluginInstance->processBlock(pluginBuffer, pluginMidiBuffer);

    for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
        bufferToFill.buffer->copyFrom(channel, bufferToFill.startSample, pluginBuffer, channel, 0, bufferToFill.numSamples);
}

WorkstationAudioEngine::TrackChannelSource::TrackChannelSource(juce::String trackName, double frequencyHz)
    : source(std::move(trackName), frequencyHz)
{
}

void WorkstationAudioEngine::TrackChannelSource::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
    source.prepareToPlay(samplesPerBlockExpected, newSampleRate);
    insert.prepareToPlay(samplesPerBlockExpected, newSampleRate);
}

void WorkstationAudioEngine::TrackChannelSource::releaseResources()
{
    insert.releaseResources();
    source.releaseResources();
}

void WorkstationAudioEngine::TrackChannelSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;

    bufferToFill.clearActiveBufferRegion();
    source.getNextAudioBlock(bufferToFill);
    insert.getNextAudioBlock(bufferToFill);
}

WorkstationAudioEngine::MasterOutputSource::MasterOutputSource(WorkstationAudioEngine& ownerRef,
                                                               juce::AudioSource& inputSource,
                                                               PluginInsertSource& insertSource)
    : owner(ownerRef), source(inputSource), insert(insertSource)
{
}

void WorkstationAudioEngine::MasterOutputSource::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
    source.prepareToPlay(samplesPerBlockExpected, newSampleRate);
    owner.arrangementSource.prepareToPlay(samplesPerBlockExpected, newSampleRate);
    insert.prepareToPlay(samplesPerBlockExpected, newSampleRate);
    owner.prepareGraph(newSampleRate, samplesPerBlockExpected);
}

void WorkstationAudioEngine::MasterOutputSource::releaseResources()
{
    owner.arrangementSource.releaseResources();
    insert.releaseResources();
    source.releaseResources();
}

void WorkstationAudioEngine::MasterOutputSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;

    if (! owner.playing.load() && ! owner.assetPreviewSource.isPreviewing())
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    bufferToFill.clearActiveBufferRegion();
    source.getNextAudioBlock(bufferToFill);

    if (owner.playing.load())
        owner.arrangementSource.getNextAudioBlock(bufferToFill);

    insert.getNextAudioBlock(bufferToFill);
    owner.processGraph(*bufferToFill.buffer);
    bufferToFill.buffer->applyGain(bufferToFill.startSample, bufferToFill.numSamples, owner.masterGain.load());
}

WorkstationAudioEngine::AssetPreviewSource::AssetPreviewSource()
{
    formatManager.registerBasicFormats();
}

void WorkstationAudioEngine::AssetPreviewSource::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
    blockSize = samplesPerBlockExpected;
    sampleRate = newSampleRate;
}

void WorkstationAudioEngine::AssetPreviewSource::releaseResources()
{
}

bool WorkstationAudioEngine::AssetPreviewSource::loadFile(const juce::File& file,
                                                          const PreviewSettings& settings,
                                                          juce::String& errorMessage)
{
    stop();

    if (! file.existsAsFile())
    {
        errorMessage = "That project sound does not exist.";
        return false;
    }

    auto* reader = formatManager.createReaderFor(file);
    if (reader == nullptr)
    {
        errorMessage = "That file could not be opened for preview.";
        return false;
    }

    auto totalSamples = (int) reader->lengthInSamples;
    if (totalSamples <= 0)
    {
        errorMessage = "That sound file is empty.";
        delete reader;
        return false;
    }

    auto safeStart = juce::jlimit(0.0, 1.0, settings.startNormalized);
    auto safeEnd = juce::jlimit(safeStart + 0.001, 1.0, settings.endNormalized);
    auto startSample = juce::jlimit(0, totalSamples - 1, juce::roundToInt((double) totalSamples * safeStart));
    auto endSample = juce::jlimit(startSample + 1, totalSamples, juce::roundToInt((double) totalSamples * safeEnd));
    auto sliceSamples = juce::jmax(1, endSample - startSample);
    auto numChannels = juce::jmax(1, (int) reader->numChannels);

    previewBuffer.setSize(juce::jmax(2, numChannels), sliceSamples, false, false, true);
    previewBuffer.clear();
    reader->read(&previewBuffer, 0, sliceSamples, startSample, true, true);
    delete reader;

    if (settings.normalize)
    {
        auto peak = previewBuffer.getMagnitude(0, sliceSamples);
        if (peak > 0.0f)
            previewBuffer.applyGain(0.95f / peak);
    }

    auto gain = juce::Decibels::decibelsToGain(settings.gainDecibels);
    previewBuffer.applyGain(gain);

    auto fadeInSamples = juce::jlimit(0, sliceSamples, juce::roundToInt((float) sliceSamples * settings.fadeInNormalized));
    auto fadeOutSamples = juce::jlimit(0, sliceSamples, juce::roundToInt((float) sliceSamples * settings.fadeOutNormalized));

    for (int channel = 0; channel < previewBuffer.getNumChannels(); ++channel)
    {
        if (fadeInSamples > 0)
            previewBuffer.applyGainRamp(channel, 0, fadeInSamples, 0.0f, 1.0f);

        if (fadeOutSamples > 0)
            previewBuffer.applyGainRamp(channel, sliceSamples - fadeOutSamples, fadeOutSamples, 1.0f, 0.0f);
    }

    if (settings.reverse)
    {
        for (int channel = 0; channel < previewBuffer.getNumChannels(); ++channel)
        {
            auto* data = previewBuffer.getWritePointer(channel);
            std::reverse(data, data + sliceSamples);
        }
    }

    playbackPosition = 0;
    previewFile = file;
    previewing.store(true);
    return true;
}

bool WorkstationAudioEngine::AssetPreviewSource::loadBuffer(const juce::AudioBuffer<float>& buffer,
                                                            double,
                                                            juce::String& errorMessage)
{
    stop();

    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
    {
        errorMessage = "That generated signal is empty.";
        return false;
    }

    previewBuffer.setSize(juce::jmax(2, buffer.getNumChannels()), buffer.getNumSamples(), false, false, true);
    previewBuffer.clear();

    for (int channel = 0; channel < previewBuffer.getNumChannels(); ++channel)
    {
        auto sourceChannel = juce::jmin(channel, buffer.getNumChannels() - 1);
        previewBuffer.copyFrom(channel, 0, buffer, sourceChannel, 0, buffer.getNumSamples());
    }

    playbackPosition = 0;
    previewFile = {};
    previewing.store(true);
    return true;
}

void WorkstationAudioEngine::AssetPreviewSource::stop()
{
    previewBuffer.setSize(0, 0);
    previewFile = {};
    playbackPosition = 0;
    previewing.store(false);
}

void WorkstationAudioEngine::AssetPreviewSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;

    if (! previewing.load())
        return;

    auto remainingSamples = previewBuffer.getNumSamples() - playbackPosition;
    if (remainingSamples <= 0)
    {
        stop();
        return;
    }

    auto samplesToCopy = juce::jmin(bufferToFill.numSamples, remainingSamples);
    for (int channel = 0; channel < juce::jmin(bufferToFill.buffer->getNumChannels(), previewBuffer.getNumChannels()); ++channel)
        bufferToFill.buffer->addFrom(channel, bufferToFill.startSample, previewBuffer, channel, playbackPosition, samplesToCopy, 0.9f);

    playbackPosition += samplesToCopy;

    if (playbackPosition >= previewBuffer.getNumSamples())
        stop();
}

WorkstationAudioEngine::ArrangementSource::ArrangementSource(WorkstationAudioEngine& sourceOwner)
    : owner(sourceOwner)
{
}

void WorkstationAudioEngine::ArrangementSource::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
    blockSize = samplesPerBlockExpected;
    sampleRate = newSampleRate;
    resetPlayback();
}

void WorkstationAudioEngine::ArrangementSource::releaseResources()
{
}

void WorkstationAudioEngine::ArrangementSource::resetPlayback() noexcept
{
    playbackSamplePosition = 0;
}

void WorkstationAudioEngine::ArrangementSource::setPlaybackPositionSeconds(double seconds) noexcept
{
    playbackSamplePosition = juce::jmax<int64>(0, (int64) std::llround(seconds * sampleRate));
}

void WorkstationAudioEngine::ArrangementSource::setClips(juce::Array<Clip> newClips)
{
    const juce::ScopedLock scopedLock(lock);
    clips = std::move(newClips);
    playbackSamplePosition = 0;
}

void WorkstationAudioEngine::ArrangementSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;

    const juce::ScopedLock scopedLock(lock);

    const auto blockStart = playbackSamplePosition;
    const auto blockEnd = playbackSamplePosition + bufferToFill.numSamples;
    const auto outputChannels = bufferToFill.buffer->getNumChannels();

    for (int trackIndex = -1; trackIndex < owner.getTrackCount(); ++trackIndex)
    {
        if (trackIndex >= 0 && ! owner.shouldRenderTrack(trackIndex))
            continue;

        trackRenderBuffer.setSize(juce::jmax(2, outputChannels), bufferToFill.numSamples, false, false, true);
        trackRenderBuffer.clear();

        auto renderedAnyClip = false;

        for (const auto& clip : clips)
        {
            if (clip.trackIndex != trackIndex)
                continue;

            const auto clipStart = clip.startSample;
            const auto clipEnd = clip.startSample + clip.buffer.getNumSamples();

            if (clipEnd <= blockStart || clipStart >= blockEnd)
                continue;

            const auto overlapStart = juce::jmax<int64>(clipStart, blockStart);
            const auto overlapEnd = juce::jmin<int64>(clipEnd, blockEnd);
            const auto clipOffset = (int) (overlapStart - clipStart);
            const auto destOffset = (int) (overlapStart - blockStart);
            const auto numSamples = (int) (overlapEnd - overlapStart);
            const auto clipChannels = clip.buffer.getNumChannels();

            if (clipChannels == 1)
            {
                for (int channel = 0; channel < trackRenderBuffer.getNumChannels(); ++channel)
                    trackRenderBuffer.addFrom(channel, destOffset, clip.buffer, 0, clipOffset, numSamples);
            }
            else
            {
                for (int channel = 0; channel < trackRenderBuffer.getNumChannels(); ++channel)
                {
                    auto sourceChannel = juce::jmin(channel, clipChannels - 1);
                    trackRenderBuffer.addFrom(channel, destOffset, clip.buffer, sourceChannel, clipOffset, numSamples);
                }
            }

            renderedAnyClip = true;
        }

        if (! renderedAnyClip)
            continue;

        if (trackIndex >= 0 && juce::isPositiveAndBelow(trackIndex, owner.tracks.size()))
        {
            auto* track = owner.tracks[(size_t) trackIndex];
            if (track != nullptr && track->insert.hasPlugin())
            {
                juce::AudioSourceChannelInfo trackInfo(&trackRenderBuffer, 0, bufferToFill.numSamples);
                track->insert.getNextAudioBlock(trackInfo);
            }
        }

        const auto trackGain = trackIndex >= 0 ? juce::jlimit(0.0f, 2.0f, owner.getTrackGain(trackIndex)) : 1.0f;
        const auto trackPan = trackIndex >= 0 ? juce::jlimit(-1.0f, 1.0f, owner.getTrackPan(trackIndex)) : 0.0f;

        for (int channel = 0; channel < outputChannels; ++channel)
        {
            auto channelGain = trackGain;
            if (outputChannels >= 2)
            {
                if (channel == 0)
                    channelGain *= trackPan <= 0.0f ? 1.0f : 1.0f - trackPan;
                else if (channel == 1)
                    channelGain *= trackPan >= 0.0f ? 1.0f : 1.0f + trackPan;
            }

            auto sourceChannel = juce::jmin(channel, trackRenderBuffer.getNumChannels() - 1);
            bufferToFill.buffer->addFrom(channel, bufferToFill.startSample, trackRenderBuffer, sourceChannel, 0, bufferToFill.numSamples, channelGain);
        }
    }

    playbackSamplePosition += bufferToFill.numSamples;
}

WorkstationAudioEngine::WorkstationAudioEngine()
    : arrangementSource(*this),
      masterOutputSource(*this, mixerSource, masterInsertSource)
{
    recordingThread.startThread();

    mixerSource.addInputSource(&assetPreviewSource, false);
}

void WorkstationAudioEngine::prepareGraph(double sampleRate, int blockSize)
{
    graphSampleRate = sampleRate;
    graphBlockSize = blockSize;
    lowPassState = {};
    echoHistoryLeft.fill(0.0f);
    echoHistoryRight.fill(0.0f);
    echoWritePosition = 0;
    signalGraph.prepare(sampleRate, blockSize);
    graphVstInsertSource.prepareToPlay(blockSize, sampleRate);
}

int WorkstationAudioEngine::addTrack(const juce::String& trackName)
{
    auto trackIndex = tracks.size();
    auto resolvedName = trackName.isNotEmpty() ? trackName : createDefaultTrackName(trackIndex);
    auto* track = tracks.add(new TrackChannelSource(resolvedName, 55.0 + (trackIndex * 17.5)));
    track->setInputChannel(inputSources.isEmpty() ? -1 : 0);
    mixerSource.addInputSource(track, false);

    if (graphSampleRate > 0.0 && graphBlockSize > 0)
        track->prepareToPlay(graphBlockSize, graphSampleRate);

    return trackIndex;
}

bool WorkstationAudioEngine::removeTrack(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return false;

    auto* track = tracks[(size_t) trackIndex];
    mixerSource.removeInputSource(track);
    tracks.remove(trackIndex);
    return true;
}

void WorkstationAudioEngine::clearTracks()
{
    while (! tracks.isEmpty())
    {
        auto* track = tracks.getLast();
        mixerSource.removeInputSource(track);
        tracks.removeLast();
    }
}

juce::Array<WorkstationAudioEngine::InputSourceDescriptor> WorkstationAudioEngine::getInputSources() const
{
    return inputSources;
}

int WorkstationAudioEngine::getTrackInputChannel(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->getInputChannel();

    return -1;
}

void WorkstationAudioEngine::setTrackInputChannel(int trackIndex, int inputChannel)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setInputChannel(inputChannel);
}

void WorkstationAudioEngine::setTrackRecordingArmed(int trackIndex, bool armed)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setRecordingArmed(armed);
}

bool WorkstationAudioEngine::isTrackRecordingArmed(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->isRecordingArmed();

    return false;
}

void WorkstationAudioEngine::setTrackMonitoringEnabled(int trackIndex, bool enabled)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setMonitoringEnabled(enabled);
}

bool WorkstationAudioEngine::isTrackMonitoringEnabled(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->isMonitoringEnabled();

    return false;
}

void WorkstationAudioEngine::setTrackStereoEnabled(int trackIndex, bool enabled)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setStereoEnabled(enabled);
}

bool WorkstationAudioEngine::isTrackStereoEnabled(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->isStereoEnabled();

    return false;
}

void WorkstationAudioEngine::rebuildInputSources(int totalNumInputChannels)
{
    if (inputSources.size() == totalNumInputChannels)
        return;

    inputSources.clear();
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        InputSourceDescriptor source;
        source.channelIndex = channel;
        source.id = "input-" + juce::String(channel + 1);
        source.name = "Input " + juce::String(channel + 1);

        if (totalNumInputChannels == 2)
            source.name << (channel == 0 ? " / Left" : " / Right");
        else
            source.name << " / Mono " << juce::String(channel + 1).paddedLeft('0', 2);

        inputSources.add(source);
    }

    for (int trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
    {
        auto* track = tracks[(size_t) trackIndex];
        if (track == nullptr)
            continue;

        auto inputChannel = track->getInputChannel();
        if (! juce::isPositiveAndBelow(inputChannel, totalNumInputChannels))
            track->setInputChannel(totalNumInputChannels > 0 ? 0 : -1);
    }
}

void WorkstationAudioEngine::rebuildInputSources(juce::AudioIODevice& device)
{
    auto activeChannels = device.getActiveInputChannels();
    auto channelNames = device.getInputChannelNames();
    auto activeCount = activeChannels.countNumberOfSetBits();

    inputSources.clear();

    for (int physicalChannel = 0; physicalChannel <= activeChannels.getHighestBit(); ++physicalChannel)
    {
        if (! activeChannels[physicalChannel])
            continue;

        InputSourceDescriptor source;
        source.channelIndex = inputSources.size();
        source.id = device.getTypeName() + ":" + device.getName() + ":" + juce::String(physicalChannel);

        auto channelName = juce::isPositiveAndBelow(physicalChannel, channelNames.size()) ? channelNames[physicalChannel] : juce::String();
        source.name = device.getName();

        if (channelName.isNotEmpty() && channelName != device.getName())
            source.name << " / " << channelName;
        else if (activeCount == 2)
            source.name << (source.channelIndex == 0 ? " / Left" : " / Right");
        else
            source.name << " / Mono " << juce::String(source.channelIndex + 1).paddedLeft('0', 2);

        inputSources.add(source);
    }

    for (int trackIndex = 0; trackIndex < static_cast<int>(tracks.size()); ++trackIndex)
    {
        auto* track = tracks[(size_t) trackIndex];
        if (track == nullptr)
            continue;

        auto inputChannel = track->getInputChannel();
        if (! juce::isPositiveAndBelow(inputChannel, inputSources.size()))
            track->setInputChannel(inputSources.size() > 0 ? 0 : -1);
    }
}

void WorkstationAudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    auto sampleRate = device != nullptr ? device->getCurrentSampleRate() : graphSampleRate;
    auto blockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : graphBlockSize;

    if (device != nullptr)
        rebuildInputSources(*device);
    else
        rebuildInputSources(0);

    masterOutputSource.prepareToPlay(blockSize, sampleRate);
}

void WorkstationAudioEngine::audioDeviceStopped()
{
    masterOutputSource.releaseResources();
    callbackRenderBuffer.setSize(0, 0);
    callbackRecordBuffer.setSize(0, 0);
    rebuildInputSources(0);
}

void WorkstationAudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                              int totalNumInputChannels,
                                                              float* const* outputChannelData,
                                                              int totalNumOutputChannels,
                                                              int numSamples,
                                                              const juce::AudioIODeviceCallbackContext&)
{
    if (inputSources.size() != totalNumInputChannels)
        rebuildInputSources(totalNumInputChannels);

    callbackRenderBuffer.setSize(juce::jmax(2, totalNumOutputChannels),
                                 numSamples,
                                 false,
                                 false,
                                 true);
    callbackRenderBuffer.clear();

    juce::AudioSourceChannelInfo info(&callbackRenderBuffer, 0, numSamples);
    masterOutputSource.getNextAudioBlock(info);

    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        if (outputChannelData[channel] == nullptr)
            continue;

        if (channel < callbackRenderBuffer.getNumChannels())
            juce::FloatVectorOperations::copy(outputChannelData[channel], callbackRenderBuffer.getReadPointer(channel), numSamples);
        else
            juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
    }

    processInputRouting(inputChannelData, totalNumInputChannels, outputChannelData, totalNumOutputChannels, numSamples);
    renderMetronome(outputChannelData, totalNumOutputChannels, numSamples);
}

void WorkstationAudioEngine::processInputRouting(const float* const* inputChannelData,
                                                 int totalNumInputChannels,
                                                 float* const* outputChannelData,
                                                 int totalNumOutputChannels,
                                                 int numSamples)
{
    if (numSamples <= 0)
        return;

    callbackRecordBuffer.setSize(2, numSamples, false, false, true);
    callbackRecordBuffer.clear();
    auto anyRecordSignal = false;

    for (int trackIndex = 0; trackIndex < static_cast<int>(tracks.size()); ++trackIndex)
    {
        auto* track = tracks[(size_t) trackIndex];
        if (track == nullptr)
            continue;

        auto inputChannel = track->getInputChannel();
        if (! juce::isPositiveAndBelow(inputChannel, totalNumInputChannels)
            || inputChannelData == nullptr
            || inputChannelData[inputChannel] == nullptr)
        {
            track->setInputLevel(0.0f);
            continue;
        }

        auto* leftSource = inputChannelData[inputChannel];
        auto* rightSource = leftSource;
        if (track->isStereoEnabled()
            && juce::isPositiveAndBelow(inputChannel + 1, totalNumInputChannels)
            && inputChannelData[inputChannel + 1] != nullptr)
            rightSource = inputChannelData[inputChannel + 1];

        auto leftRange = juce::FloatVectorOperations::findMinAndMax(leftSource, numSamples);
        auto peak = juce::jmax(std::abs(leftRange.getStart()), std::abs(leftRange.getEnd()));
        if (rightSource != leftSource)
        {
            auto rightRange = juce::FloatVectorOperations::findMinAndMax(rightSource, numSamples);
            peak = juce::jmax(peak, std::abs(rightRange.getStart()), std::abs(rightRange.getEnd()));
        }
        track->setInputLevel(peak);

        auto gain = track->getGain();
        auto pan = track->getPan();
        auto leftGain = panToLeftGain(pan) * gain;
        auto rightGain = panToRightGain(pan) * gain;
        auto muted = track->isMuted();

        if (track->isMonitoringEnabled() && ! muted)
        {
            if (totalNumOutputChannels > 0 && outputChannelData[0] != nullptr)
                juce::FloatVectorOperations::addWithMultiply(outputChannelData[0], leftSource, leftGain, numSamples);
            if (totalNumOutputChannels > 1 && outputChannelData[1] != nullptr)
                juce::FloatVectorOperations::addWithMultiply(outputChannelData[1], rightSource, rightGain, numSamples);
        }

        if (track->isRecordingArmed() && ! muted)
        {
            juce::ignoreUnused(leftGain, rightGain);
            track->pushRecordingPeak(peak);
            writeRecording(trackIndex, leftSource, track->isStereoEnabled() ? rightSource : nullptr, numSamples);
            anyRecordSignal = true;
        }
    }

    juce::ignoreUnused(anyRecordSignal);
}

void WorkstationAudioEngine::renderMetronome(float* const* outputChannelData,
                                             int totalNumOutputChannels,
                                             int numSamples)
{
    if (! metronomeEnabled.load() || ! playing.load() || outputChannelData == nullptr || totalNumOutputChannels <= 0 || numSamples <= 0)
        return;

    auto sampleRate = juce::jmax(1.0, graphSampleRate);
    auto bpm = juce::jlimit(20.0, 320.0, metronomeBpm.load());
    auto beatsPerMeasure = juce::jmax(1, metronomeBeatsPerMeasure.load());
    auto samplesPerBeat = juce::jmax<int64>(1, static_cast<int64>(std::round(sampleRate * 60.0 / bpm)));
    auto clickSamples = juce::jmax(1, static_cast<int>(sampleRate * 0.028));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto positionInBeat = metronomeSampleCounter % samplesPerBeat;

        if (positionInBeat < clickSamples)
        {
            auto beatIndex = (metronomeSampleCounter / samplesPerBeat) % beatsPerMeasure;
            auto accent = beatIndex == 0;
            auto frequency = accent ? 1760.0 : 1040.0;
            auto envelope = 1.0f - (static_cast<float>(positionInBeat) / static_cast<float>(clickSamples));
            envelope *= envelope;
            auto phase = juce::MathConstants<double>::twoPi * frequency * static_cast<double>(positionInBeat) / sampleRate;
            auto click = static_cast<float>(std::sin(phase)) * envelope * (accent ? 0.34f : 0.22f);

            for (int channel = 0; channel < juce::jmin(2, totalNumOutputChannels); ++channel)
                if (outputChannelData[channel] != nullptr)
                    outputChannelData[channel][sample] += click;
        }

        ++metronomeSampleCounter;
    }
}

void WorkstationAudioEngine::processGraph(juce::AudioBuffer<float>& buffer)
{
    if (! playing.load())
        return;

    signalGraph.setEnabled(graphEnabled.load());
    signalGraph.setSourceLevel(graphInput.load());
    signalGraph.setSourceFrequency(graphSourceFrequency.load());
    signalGraph.setDrive(graphDrive.load());
    signalGraph.setTone(graphTone.load());
    signalGraph.setEcho(graphEcho.load());
    signalGraph.setWidth(graphWidth.load());
    signalGraph.setMasterGain(masterGain.load());
    signalGraph.render(buffer);

    if (graphVstEnabled.load() && graphVstInsertSource.hasPlugin())
    {
        auto mix = juce::jlimit(0.0f, 1.0f, graphVstMix.load());
        if (mix > 0.0f)
        {
            juce::AudioBuffer<float> dryBuffer;
            juce::AudioBuffer<float> wetBuffer;
            dryBuffer.makeCopyOf(buffer, true);
            wetBuffer.makeCopyOf(buffer, true);

            juce::AudioSourceChannelInfo wetInfo(&wetBuffer, 0, wetBuffer.getNumSamples());
            graphVstInsertSource.getNextAudioBlock(wetInfo);

            auto dryMix = 1.0f - mix;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto* dest = buffer.getWritePointer(channel);
                auto* dry = dryBuffer.getReadPointer(channel);
                auto* wet = wetBuffer.getReadPointer(channel);
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                    dest[sample] = (dry[sample] * dryMix) + (wet[sample] * mix);
            }
        }
    }

}

bool WorkstationAudioEngine::shouldRenderTrack(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return true;

    auto anySoloed = false;
    for (auto* track : tracks)
    {
        if (track != nullptr && track->isSoloed())
        {
            anySoloed = true;
            break;
        }
    }

    auto* track = tracks[(size_t) trackIndex];
    if (track == nullptr || track->isMuted())
        return false;

    return ! anySoloed || track->isSoloed();
}

void WorkstationAudioEngine::writeRecording(int trackIndex, const float* leftSource, const float* rightSource, int numSamples)
{
    if (! recording.load() || leftSource == nullptr || numSamples <= 0)
        return;

    const juce::ScopedTryLock lock(recordingLock);
    if (! lock.isLocked())
        return;

    for (auto& recordingWriter : recordingWriters)
    {
        if (recordingWriter.trackIndex != trackIndex || recordingWriter.writer == nullptr)
            continue;

        const float* channelData[] = { leftSource, rightSource != nullptr ? rightSource : leftSource };
        recordingWriter.writer->write(channelData, numSamples);
        return;
    }
}

void WorkstationAudioEngine::attachToDevice(juce::AudioDeviceManager& deviceManager)
{
    deviceManager.addAudioCallback(this);
}

void WorkstationAudioEngine::detachFromDevice(juce::AudioDeviceManager& deviceManager)
{
    deviceManager.removeAudioCallback(this);
}

void WorkstationAudioEngine::setPlaying(bool shouldPlay)
{
    playing.store(shouldPlay);

    for (auto* track : tracks)
        track->setPlaying(false);

    if (shouldPlay)
        metronomeSampleCounter = 0;
}

void WorkstationAudioEngine::setPlaybackPositionSeconds(double seconds)
{
    arrangementSource.setPlaybackPositionSeconds(seconds);
}

void WorkstationAudioEngine::setMetronomeTempo(double bpm, int numerator) noexcept
{
    metronomeBpm.store(juce::jlimit(20.0, 320.0, bpm));
    metronomeBeatsPerMeasure.store(juce::jmax(1, numerator));
}

bool WorkstationAudioEngine::previewAssetFile(const juce::File& file, juce::String& errorMessage)
{
    return previewAssetFile(file, {}, errorMessage);
}

bool WorkstationAudioEngine::previewAssetFile(const juce::File& file,
                                              const PreviewSettings& settings,
                                              juce::String& errorMessage)
{
    auto loaded = assetPreviewSource.loadFile(file, settings, errorMessage);
    return loaded;
}

bool WorkstationAudioEngine::previewGeneratedBuffer(const juce::AudioBuffer<float>& buffer,
                                                    double sampleRate,
                                                    juce::String& errorMessage)
{
    juce::ignoreUnused(sampleRate);
    auto loaded = assetPreviewSource.loadBuffer(buffer, sampleRate, errorMessage);
    return loaded;
}

bool WorkstationAudioEngine::setFoleyArrangement(const juce::ValueTree& arrangementState,
                                                 const juce::File& assetsDirectory,
                                                 juce::String& errorMessage)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::Array<ArrangementSource::Clip> clips;

    for (int childIndex = 0; childIndex < arrangementState.getNumChildren(); ++childIndex)
    {
        auto clipState = arrangementState.getChild(childIndex);
        if (! clipState.hasType("Clip"))
            continue;

        PreviewSettings settings;
        settings.startNormalized = (double) clipState.getProperty("trimStart", 0.0);
        settings.endNormalized = (double) clipState.getProperty("trimEnd", 1.0);
        settings.gainDecibels = (float) clipState.getProperty("gainDecibels", 0.0f);
        settings.fadeInNormalized = (float) clipState.getProperty("fadeInNormalized", 0.0f);
        settings.fadeOutNormalized = (float) clipState.getProperty("fadeOutNormalized", 0.0f);
        settings.reverse = (bool) clipState.getProperty("reverse", false);
        settings.normalize = (bool) clipState.getProperty("normalize", false);

        auto assetFile = assetsDirectory.getChildFile(clipState.getProperty("assetFileName").toString());
        if (! assetFile.existsAsFile())
            continue;

        auto* reader = formatManager.createReaderFor(assetFile);
        if (reader == nullptr)
        {
            errorMessage = "Could not open Foley asset: " + assetFile.getFileName();
            continue;
        }

        auto totalSamples = (int) reader->lengthInSamples;
        if (totalSamples <= 0)
        {
            delete reader;
            continue;
        }

        auto safeStart = juce::jlimit(0.0, 1.0, settings.startNormalized);
        auto safeEnd = juce::jlimit(safeStart + 0.001, 1.0, settings.endNormalized);
        auto startSample = juce::jlimit(0, totalSamples - 1, juce::roundToInt((double) totalSamples * safeStart));
        auto endSample = juce::jlimit(startSample + 1, totalSamples, juce::roundToInt((double) totalSamples * safeEnd));
        auto sliceSamples = juce::jmax(1, endSample - startSample);
        auto numChannels = juce::jmax(2, (int) reader->numChannels);

        ArrangementSource::Clip clip;
        clip.buffer.setSize(numChannels, sliceSamples, false, false, true);
        clip.buffer.clear();
        reader->read(&clip.buffer, 0, sliceSamples, startSample, true, true);
        delete reader;

        if (settings.normalize)
        {
            auto peak = clip.buffer.getMagnitude(0, sliceSamples);
            if (peak > 0.0f)
                clip.buffer.applyGain(0.95f / peak);
        }

        clip.buffer.applyGain(juce::Decibels::decibelsToGain(settings.gainDecibels));

        auto fadeInSamples = juce::jlimit(0, sliceSamples, juce::roundToInt((float) sliceSamples * settings.fadeInNormalized));
        auto fadeOutSamples = juce::jlimit(0, sliceSamples, juce::roundToInt((float) sliceSamples * settings.fadeOutNormalized));

        for (int channel = 0; channel < clip.buffer.getNumChannels(); ++channel)
        {
            if (fadeInSamples > 0)
                clip.buffer.applyGainRamp(channel, 0, fadeInSamples, 0.0f, 1.0f);
            if (fadeOutSamples > 0)
                clip.buffer.applyGainRamp(channel, sliceSamples - fadeOutSamples, fadeOutSamples, 1.0f, 0.0f);
        }

        if (settings.reverse)
        {
            for (int channel = 0; channel < clip.buffer.getNumChannels(); ++channel)
            {
                auto* data = clip.buffer.getWritePointer(channel);
                std::reverse(data, data + sliceSamples);
            }
        }

        auto startBeat = (int) clipState.getProperty("startBeat", 0);
        clip.startSample = (int64) std::llround(startBeat * foleySecondsPerBeat * graphSampleRate);
        clip.trackIndex = -1;
        clips.add(std::move(clip));
    }

    arrangementSource.setClips(std::move(clips));
    return true;
}

bool WorkstationAudioEngine::setTrackerPlaybackClips(const juce::Array<PlaybackClipTarget>& targets,
                                                     juce::String& errorMessage)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::Array<ArrangementSource::Clip> clips;

    for (const auto& target : targets)
    {
        if (! target.file.existsAsFile())
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(target.file));
        if (reader == nullptr)
        {
            errorMessage = "Could not open recorded clip: " + target.file.getFileName();
            continue;
        }

        const auto totalSamples = (int64) reader->lengthInSamples;
        if (totalSamples <= 0)
            continue;

        ArrangementSource::Clip clip;
        const auto sourceStartSample = juce::jlimit<int64>(0,
                                                           totalSamples - 1,
                                                           (int64) std::llround(target.sourceStartSeconds * reader->sampleRate));
        const auto requestedSamples = target.durationSeconds > 0.0
                                        ? (int64) std::llround(target.durationSeconds * reader->sampleRate)
                                        : totalSamples - sourceStartSample;
        const auto samplesToRead = juce::jlimit<int64>(1, totalSamples - sourceStartSample, requestedSamples);

        clip.buffer.setSize(juce::jmax(2, (int) reader->numChannels), (int) samplesToRead, false, false, true);
        clip.buffer.clear();
        reader->read(&clip.buffer, 0, (int) samplesToRead, sourceStartSample, true, true);
        clip.startSample = (int64) std::llround(target.startSeconds * graphSampleRate);
        clip.trackIndex = target.trackIndex;
        clips.add(std::move(clip));
    }

    if (clips.isEmpty() && ! targets.isEmpty())
    {
        errorMessage = "No tracker audio clips could be loaded for playback.";
        return false;
    }

    arrangementSource.setClips(std::move(clips));
    return true;
}

bool WorkstationAudioEngine::renderTrackerMixToBuffer(const juce::Array<PlaybackClipTarget>& targets,
                                                      double durationSeconds,
                                                      const RenderSettings& settings,
                                                      juce::AudioBuffer<float>& outputBuffer,
                                                      juce::String& errorMessage)
{
    if (targets.isEmpty())
    {
        errorMessage = "There are no tracker clips to render.";
        return false;
    }

    const auto safeSampleRate = juce::jmax(8000.0, settings.sampleRate);
    const auto safeBlockSize = juce::jmax(64, settings.blockSize);
    const auto totalSamples = (int) std::ceil(juce::jmax(0.01, durationSeconds) * safeSampleRate);

    if (totalSamples <= 0)
    {
        errorMessage = "The render range is empty.";
        return false;
    }

    const auto wasPlaying = playing.load();
    playing.store(false);

    const auto previousSampleRate = graphSampleRate;
    const auto previousBlockSize = graphBlockSize;
    prepareGraph(safeSampleRate, safeBlockSize);

    for (auto* track : tracks)
        if (track != nullptr)
            track->prepareToPlay(safeBlockSize, safeSampleRate);

    masterInsertSource.prepareToPlay(safeBlockSize, safeSampleRate);

    if (! setTrackerPlaybackClips(targets, errorMessage))
    {
        prepareGraph(previousSampleRate, previousBlockSize);
        playing.store(wasPlaying);
        return false;
    }

    arrangementSource.prepareToPlay(safeBlockSize, safeSampleRate);
    arrangementSource.setPlaybackPositionSeconds(0.0);

    outputBuffer.setSize(2, totalSamples, false, false, true);
    outputBuffer.clear();

    juce::AudioBuffer<float> blockBuffer(2, safeBlockSize);
    auto renderedSamples = 0;

    while (renderedSamples < totalSamples)
    {
        const auto samplesThisBlock = juce::jmin(safeBlockSize, totalSamples - renderedSamples);
        blockBuffer.clear();

        juce::AudioSourceChannelInfo blockInfo(&blockBuffer, 0, samplesThisBlock);
        arrangementSource.getNextAudioBlock(blockInfo);
        masterInsertSource.getNextAudioBlock(blockInfo);
        blockBuffer.applyGain(0, samplesThisBlock, masterGain.load());

        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            outputBuffer.copyFrom(channel, renderedSamples, blockBuffer, channel, 0, samplesThisBlock);

        renderedSamples += samplesThisBlock;
    }

    if (settings.normalizePeak)
    {
        const auto peak = outputBuffer.getMagnitude(0, outputBuffer.getNumSamples());
        if (peak > 0.0f)
        {
            const auto targetGain = juce::Decibels::decibelsToGain(settings.peakTargetDecibels);
            outputBuffer.applyGain(targetGain / peak);
        }
    }

    arrangementSource.setPlaybackPositionSeconds(0.0);
    prepareGraph(previousSampleRate, previousBlockSize);
    for (auto* track : tracks)
        if (track != nullptr)
            track->prepareToPlay(previousBlockSize, previousSampleRate);
    masterInsertSource.prepareToPlay(previousBlockSize, previousSampleRate);
    playing.store(wasPlaying);
    return true;
}

void WorkstationAudioEngine::stopAssetPreview()
{
    assetPreviewSource.stop();
}

bool WorkstationAudioEngine::isPreviewingAsset() const noexcept
{
    return assetPreviewSource.isPreviewing();
}

bool WorkstationAudioEngine::startRecordingToFile(const juce::File& file, juce::String& errorMessage)
{
    RecordingTarget target;
    target.trackIndex = 0;
    target.file = file;

    juce::Array<RecordingTarget> targets;
    targets.add(target);
    return startRecordingToFiles(targets, errorMessage);
}

bool WorkstationAudioEngine::startRecordingToFiles(const juce::Array<RecordingTarget>& targets, juce::String& errorMessage)
{
    if (targets.isEmpty())
    {
        errorMessage = "No armed tracks were available for recording.";
        return false;
    }

    for (const auto& target : targets)
    {
        if (! juce::isPositiveAndBelow(target.trackIndex, tracks.size()))
        {
            errorMessage = "A recording target did not match a valid track.";
            return false;
        }

        if (target.file.getFullPathName().isEmpty())
        {
            errorMessage = "A recording file path was empty.";
            return false;
        }
    }

    if (recording.load())
        stopRecording();

    juce::WavAudioFormat wavFormat;
    std::vector<TrackRecordingWriter> newWriters;

    for (const auto& target : targets)
    {
        target.file.getParentDirectory().createDirectory();
        if (target.file.existsAsFile())
            target.file.deleteFile();

        std::unique_ptr<juce::FileOutputStream> outputStream(target.file.createOutputStream());
        if (outputStream == nullptr)
        {
            errorMessage = "Could not create recording file: " + target.file.getFileName();
            return false;
        }

        auto numChannels = tracks[(size_t) target.trackIndex]->isStereoEnabled() ? 2 : 1;
        auto* writer = wavFormat.createWriterFor(outputStream.release(),
                                                 graphSampleRate,
                                                 (unsigned int) numChannels,
                                                 24,
                                                 {},
                                                 0);

        if (writer == nullptr)
        {
            errorMessage = "Could not create audio writer: " + target.file.getFileName();
            return false;
        }

        TrackRecordingWriter recordingWriter;
        recordingWriter.trackIndex = target.trackIndex;
        recordingWriter.numChannels = numChannels;
        recordingWriter.file = target.file;
        recordingWriter.writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(writer, recordingThread, 32768);
        newWriters.push_back(std::move(recordingWriter));
    }

    const juce::ScopedLock lock(recordingLock);
    recordingWriters = std::move(newWriters);
    recordingFile = recordingWriters.empty() ? juce::File{} : recordingWriters.front().file;
    recording = true;
    return true;
}

void WorkstationAudioEngine::stopRecording()
{
    const juce::ScopedLock lock(recordingLock);
    recordingWriters.clear();
    recording = false;
    recordingFile = {};
}

juce::Array<juce::File> WorkstationAudioEngine::getRecordingFiles() const
{
    juce::Array<juce::File> files;
    const juce::ScopedLock lock(recordingLock);

    for (const auto& recordingWriter : recordingWriters)
        files.add(recordingWriter.file);

    return files;
}

juce::String WorkstationAudioEngine::getTrackName(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->getName();

    return {};
}

void WorkstationAudioEngine::setTrackName(int trackIndex, const juce::String& name)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setName(name.isNotEmpty() ? name : createDefaultTrackName(trackIndex));
}

float WorkstationAudioEngine::getTrackLevel(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return juce::jmax(tracks[(size_t) trackIndex]->getLevel(),
                          tracks[(size_t) trackIndex]->getInputLevel());

    return 0.0f;
}

float WorkstationAudioEngine::consumeTrackRecordingPeak(int trackIndex)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->consumeRecordingPeak();

    return 0.0f;
}

float WorkstationAudioEngine::getTrackGain(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->getGain();

    return 0.0f;
}

float WorkstationAudioEngine::getTrackPan(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->getPan();

    return 0.0f;
}

bool WorkstationAudioEngine::isTrackMuted(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->isMuted();

    return false;
}

bool WorkstationAudioEngine::isTrackSoloed(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->isSoloed();

    return false;
}

void WorkstationAudioEngine::setTrackGain(int trackIndex, float gain)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setGain(gain);
}

void WorkstationAudioEngine::setTrackPan(int trackIndex, float pan)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setPan(pan);
}

void WorkstationAudioEngine::setTrackMuted(int trackIndex, bool shouldMute)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setMuted(shouldMute);
}

void WorkstationAudioEngine::setTrackSoloed(int trackIndex, bool shouldSolo)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setSoloed(shouldSolo);
}

void WorkstationAudioEngine::setMasterGain(float gain)
{
    masterGain.store(gain);
}

void WorkstationAudioEngine::setGraphEnabled(bool shouldEnable)
{
    graphEnabled.store(shouldEnable);
    signalGraph.setEnabled(shouldEnable);
}

void WorkstationAudioEngine::setGraphDrive(float amount)
{
    graphDrive.store(amount);
    signalGraph.setDrive(amount);
}

void WorkstationAudioEngine::setGraphInput(float amount)
{
    graphInput.store(amount);
    signalGraph.setSourceLevel(amount);
}

void WorkstationAudioEngine::setGraphSourceFrequency(float hz)
{
    graphSourceFrequency.store(hz);
    signalGraph.setSourceFrequency(hz);
}

void WorkstationAudioEngine::setGraphTone(float amount)
{
    graphTone.store(amount);
    signalGraph.setTone(amount);
}

void WorkstationAudioEngine::setGraphEcho(float amount)
{
    graphEcho.store(amount);
    signalGraph.setEcho(amount);
}

void WorkstationAudioEngine::setGraphWidth(float amount)
{
    graphWidth.store(amount);
    signalGraph.setWidth(amount);
}

bool WorkstationAudioEngine::loadGraphVstPlugin(const juce::File& file, juce::String& errorMessage)
{
    return graphVstInsertSource.loadPlugin(file, errorMessage);
}

void WorkstationAudioEngine::unloadGraphVstPlugin()
{
    graphVstInsertSource.unloadPlugin();
}

juce::String WorkstationAudioEngine::getGraphVstPluginName() const
{
    return graphVstInsertSource.getPluginName();
}

juce::File WorkstationAudioEngine::getGraphVstPluginFile() const
{
    return graphVstInsertSource.getPluginFile();
}

bool WorkstationAudioEngine::hasGraphVstPlugin() const noexcept
{
    return graphVstInsertSource.hasPlugin();
}

void WorkstationAudioEngine::setGraphVstEnabled(bool shouldEnable)
{
    graphVstEnabled.store(shouldEnable);
}

void WorkstationAudioEngine::setGraphVstMix(float amount)
{
    graphVstMix.store(juce::jlimit(0.0f, 1.0f, amount));
}

juce::AudioProcessorEditor* WorkstationAudioEngine::createGraphVstPluginEditor()
{
    return graphVstInsertSource.createEditor();
}

bool WorkstationAudioEngine::loadMasterPlugin(const juce::File& file, juce::String& errorMessage)
{
    return masterInsertSource.loadPlugin(file, errorMessage);
}

void WorkstationAudioEngine::unloadMasterPlugin()
{
    masterInsertSource.unloadPlugin();
}

juce::String WorkstationAudioEngine::getMasterPluginName() const
{
    return masterInsertSource.getPluginName();
}

juce::File WorkstationAudioEngine::getMasterPluginFile() const
{
    return masterInsertSource.getPluginFile();
}

bool WorkstationAudioEngine::hasMasterPlugin() const noexcept
{
    return masterInsertSource.hasPlugin();
}

void WorkstationAudioEngine::setMasterPluginBypassed(bool shouldBypass)
{
    masterInsertSource.setBypassed(shouldBypass);
}

bool WorkstationAudioEngine::isMasterPluginBypassed() const noexcept
{
    return masterInsertSource.isBypassed();
}

juce::AudioProcessorEditor* WorkstationAudioEngine::createMasterPluginEditor()
{
    return masterInsertSource.createEditor();
}

bool WorkstationAudioEngine::loadTrackPlugin(int trackIndex, const juce::File& file, juce::String& errorMessage)
{
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
    {
        errorMessage = "Track index is out of range.";
        return false;
    }

    return tracks[(size_t) trackIndex]->insert.loadPlugin(file, errorMessage);
}

void WorkstationAudioEngine::unloadTrackPlugin(int trackIndex)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->insert.unloadPlugin();
}

juce::String WorkstationAudioEngine::getTrackPluginName(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insert.getPluginName();

    return {};
}

juce::File WorkstationAudioEngine::getTrackPluginFile(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insert.getPluginFile();

    return {};
}

bool WorkstationAudioEngine::hasTrackPlugin(int trackIndex) const noexcept
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insert.hasPlugin();

    return false;
}

void WorkstationAudioEngine::setTrackPluginBypassed(int trackIndex, bool shouldBypass)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->insert.setBypassed(shouldBypass);
}

bool WorkstationAudioEngine::isTrackPluginBypassed(int trackIndex) const noexcept
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insert.isBypassed();

    return false;
}

juce::AudioProcessorEditor* WorkstationAudioEngine::createTrackPluginEditor(int trackIndex)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insert.createEditor();

    return nullptr;
}

juce::ValueTree WorkstationAudioEngine::createSessionState() const
{
    juce::ValueTree state("CreationStationSession");
    state.setProperty("version", 1, nullptr);
    state.setProperty("masterGain", masterGain.load(), nullptr);
    state.setProperty("playing", playing.load(), nullptr);

    juce::ValueTree tracksState("Tracks");
    for (int trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
    {
        auto* track = tracks[(size_t) trackIndex];
        juce::ValueTree trackState("Track");
        trackState.setProperty("index", trackIndex, nullptr);
        trackState.setProperty("name", track->getName(), nullptr);
        trackState.setProperty("gain", track->getGain(), nullptr);
        trackState.setProperty("pan", track->getPan(), nullptr);
        trackState.setProperty("muted", track->isMuted(), nullptr);
        trackState.setProperty("soloed", track->isSoloed(), nullptr);
        trackState.setProperty("inputChannel", track->getInputChannel(), nullptr);
        trackState.setProperty("recordingArmed", track->isRecordingArmed(), nullptr);
        trackState.setProperty("monitoringEnabled", track->isMonitoringEnabled(), nullptr);
        trackState.setProperty("stereoEnabled", track->isStereoEnabled(), nullptr);

        juce::ValueTree insertState("Insert");
        insertState.setProperty("bypassed", track->insert.isBypassed(), nullptr);
        insertState.setProperty("file", track->insert.getPluginFile().getFullPathName(), nullptr);
        insertState.setProperty("name", track->insert.getPluginName(), nullptr);

        juce::MemoryBlock pluginState;
        if (track->insert.copyStateTo(pluginState))
            insertState.setProperty("state", juce::Base64::toBase64(pluginState.getData(), pluginState.getSize()), nullptr);

        trackState.addChild(insertState, -1, nullptr);
        tracksState.addChild(trackState, -1, nullptr);
    }

    state.addChild(tracksState, -1, nullptr);

    juce::ValueTree masterInsert("MasterInsert");
    masterInsert.setProperty("bypassed", masterInsertSource.isBypassed(), nullptr);
    masterInsert.setProperty("file", masterInsertSource.getPluginFile().getFullPathName(), nullptr);
    masterInsert.setProperty("name", masterInsertSource.getPluginName(), nullptr);

    juce::MemoryBlock masterState;
    if (masterInsertSource.copyStateTo(masterState))
        masterInsert.setProperty("state", juce::Base64::toBase64(masterState.getData(), masterState.getSize()), nullptr);

    state.addChild(masterInsert, -1, nullptr);

    juce::ValueTree graphInsert("GraphInsert");
    graphInsert.setProperty("enabled", graphVstEnabled.load(), nullptr);
    graphInsert.setProperty("mix", graphVstMix.load(), nullptr);
    graphInsert.setProperty("file", graphVstInsertSource.getPluginFile().getFullPathName(), nullptr);
    graphInsert.setProperty("name", graphVstInsertSource.getPluginName(), nullptr);

    juce::MemoryBlock graphState;
    if (graphVstInsertSource.copyStateTo(graphState))
        graphInsert.setProperty("state", juce::Base64::toBase64(graphState.getData(), graphState.getSize()), nullptr);

    state.addChild(graphInsert, -1, nullptr);
    return state;
}

bool WorkstationAudioEngine::restoreSessionState(const juce::ValueTree& sessionState, juce::String& errorMessage)
{
    if (! sessionState.isValid() || sessionState.getType() != juce::Identifier("CreationStationSession"))
    {
        errorMessage = "Session file was not recognized.";
        return false;
    }

    setMasterGain((float) sessionState.getProperty("masterGain", masterGain.load()));
    setPlaying((bool) sessionState.getProperty("playing", false));
    clearTracks();

    if (auto tracksState = sessionState.getChildWithName("Tracks"); tracksState.isValid())
    {
        for (const auto child : tracksState)
        {
            auto trackIndex = (int) child.getProperty("index", -1);
            if (trackIndex < 0)
                continue;

            while (tracks.size() <= trackIndex)
                addTrack();

            auto* track = tracks[(size_t) trackIndex];
            track->setName(child.getProperty("name").toString());
            track->setGain((float) child.getProperty("gain", track->getGain()));
            track->setPan((float) child.getProperty("pan", track->getPan()));
            track->setMuted((bool) child.getProperty("muted", track->isMuted()));
            track->setSoloed((bool) child.getProperty("soloed", track->isSoloed()));
            track->setInputChannel((int) child.getProperty("inputChannel", track->getInputChannel()));
            track->setRecordingArmed((bool) child.getProperty("recordingArmed", false));
            track->setMonitoringEnabled((bool) child.getProperty("monitoringEnabled", false));
            track->setStereoEnabled((bool) child.getProperty("stereoEnabled", false));

            auto insertState = child.getChildWithName("Insert");
            auto filePath = insertState.getProperty("file").toString();
            if (filePath.isNotEmpty())
            {
                juce::MemoryBlock pluginState;
                auto encoded = insertState.getProperty("state").toString();
                if (encoded.isNotEmpty())
                    pluginState.fromBase64Encoding(encoded);

                juce::String loadError;
                if (! track->insert.loadPlugin(juce::File(filePath), pluginState.getSize() > 0 ? &pluginState : nullptr, loadError))
                {
                    errorMessage = loadError;
                    continue;
                }

                track->insert.setBypassed((bool) insertState.getProperty("bypassed", false));
            }
        }
    }

    if (auto masterInsert = sessionState.getChildWithName("MasterInsert"); masterInsert.isValid())
    {
        auto filePath = masterInsert.getProperty("file").toString();
        if (filePath.isNotEmpty())
        {
            juce::MemoryBlock masterState;
            auto encoded = masterInsert.getProperty("state").toString();
            if (encoded.isNotEmpty())
                masterState.fromBase64Encoding(encoded);

            juce::String loadError;
            if (! masterInsertSource.loadPlugin(juce::File(filePath), masterState.getSize() > 0 ? &masterState : nullptr, loadError))
            {
                errorMessage = loadError;
            }
            else
            {
                masterInsertSource.setBypassed((bool) masterInsert.getProperty("bypassed", false));
            }
        }
    }

    if (auto graphInsert = sessionState.getChildWithName("GraphInsert"); graphInsert.isValid())
    {
        graphVstEnabled.store((bool) graphInsert.getProperty("enabled", true));
        graphVstMix.store((float) graphInsert.getProperty("mix", 0.5f));

        auto filePath = graphInsert.getProperty("file").toString();
        if (filePath.isNotEmpty())
        {
            juce::MemoryBlock graphState;
            auto encoded = graphInsert.getProperty("state").toString();
            if (encoded.isNotEmpty())
                graphState.fromBase64Encoding(encoded);

            juce::String loadError;
            if (! graphVstInsertSource.loadPlugin(juce::File(filePath), graphState.getSize() > 0 ? &graphState : nullptr, loadError))
                errorMessage = loadError;
        }
    }

    return true;
}
