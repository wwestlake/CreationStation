#include "WorkstationAudioEngine.h"

#include <cmath>
#include <array>

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
    static const std::array<const char*, 32> names
    {
        "Drums", "Kick", "Snare", "Hi-Hat", "Tom 1", "Tom 2", "OH L", "OH R",
        "Bass", "Sub", "Keys", "Pad", "Lead", "Vox", "BGV 1", "BGV 2",
        "Gtr 1", "Gtr 2", "Piano", "Organ", "Strings", "Brass", "FX 1", "FX 2",
        "Perc 1", "Perc 2", "Returns", "Bus A", "Bus B", "Bus C", "Print", "Master FX"
    };

    if (juce::isPositiveAndBelow(trackIndex, static_cast<int>(names.size())))
        return names[(size_t) trackIndex];

    return "Track " + juce::String(trackIndex + 1);
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
    insert.prepareToPlay(samplesPerBlockExpected, newSampleRate);
    owner.prepareGraph(newSampleRate, samplesPerBlockExpected);
}

void WorkstationAudioEngine::MasterOutputSource::releaseResources()
{
    insert.releaseResources();
    source.releaseResources();
}

void WorkstationAudioEngine::MasterOutputSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;

    if (! owner.playing.load())
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    bufferToFill.clearActiveBufferRegion();
    source.getNextAudioBlock(bufferToFill);
    insert.getNextAudioBlock(bufferToFill);
    owner.processGraph(*bufferToFill.buffer);
    bufferToFill.buffer->applyGain(bufferToFill.startSample, bufferToFill.numSamples, owner.masterGain.load());
}

WorkstationAudioEngine::WorkstationAudioEngine()
    : masterOutputSource(*this, mixerSource, masterInsertSource)
{
    recordingThread.startThread();

    for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
        tracks.add(new TrackChannelSource(createDefaultTrackName(trackIndex), 55.0 + (trackIndex * 17.5)));

    for (auto* track : tracks)
        mixerSource.addInputSource(track, false);

    audioSourcePlayer.setSource(&masterOutputSource);
    audioSourcePlayer.setGain(0.0f);
}

void WorkstationAudioEngine::prepareGraph(double sampleRate, int blockSize)
{
    graphSampleRate = sampleRate;
    juce::ignoreUnused(blockSize);
    lowPassState = {};
    echoHistoryLeft.fill(0.0f);
    echoHistoryRight.fill(0.0f);
    echoWritePosition = 0;
    signalGraph.prepare(sampleRate, blockSize);
}

void WorkstationAudioEngine::processGraph(juce::AudioBuffer<float>& buffer)
{
    signalGraph.setEnabled(graphEnabled.load());
    signalGraph.setSourceLevel(graphInput.load());
    signalGraph.setDrive(graphDrive.load());
    signalGraph.setTone(graphTone.load());
    signalGraph.setEcho(graphEcho.load());
    signalGraph.setWidth(graphWidth.load());
    signalGraph.setMasterGain(masterGain.load());
    signalGraph.render(buffer);
    writeRecording(buffer);
}

void WorkstationAudioEngine::writeRecording(const juce::AudioBuffer<float>& buffer)
{
    if (! recording.load() || buffer.getNumChannels() < 2 || buffer.getNumSamples() <= 0)
        return;

    const juce::ScopedTryLock lock(recordingLock);
    if (! lock.isLocked() || recordingWriter == nullptr)
        return;

    const float* channelData[] =
    {
        buffer.getReadPointer(0),
        buffer.getReadPointer(1)
    };

    recordingWriter->write(channelData, buffer.getNumSamples());
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

    audioSourcePlayer.setGain(shouldPlay ? 1.0f : 0.0f);
}

bool WorkstationAudioEngine::startRecordingToFile(const juce::File& file, juce::String& errorMessage)
{
    if (file.getFullPathName().isEmpty())
    {
        errorMessage = "Recording file path was empty.";
        return false;
    }

    if (recording.load())
        stopRecording();

    file.getParentDirectory().createDirectory();
    if (file.existsAsFile())
        file.deleteFile();

    std::unique_ptr<juce::FileOutputStream> outputStream(file.createOutputStream());
    if (outputStream == nullptr)
    {
        errorMessage = "Could not create the recording file.";
        return false;
    }

    juce::WavAudioFormat wavFormat;
    auto* writer = wavFormat.createWriterFor(outputStream.release(),
                                             graphSampleRate,
                                             2,
                                             24,
                                             {},
                                             0);

    if (writer == nullptr)
    {
        errorMessage = "Could not create the audio writer.";
        return false;
    }

    const juce::ScopedLock lock(recordingLock);
    recordingWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(writer, recordingThread, 32768);
    recordingFile = file;
    recording = true;
    return true;
}

void WorkstationAudioEngine::stopRecording()
{
    const juce::ScopedLock lock(recordingLock);
    recordingWriter.reset();
    recording = false;
    recordingFile = {};
}

juce::String WorkstationAudioEngine::getTrackName(int trackIndex) const
{
    if (auto* track = tracks[juce::jlimit(0, tracks.size() - 1, trackIndex)])
        return track->getName();

    return {};
}

float WorkstationAudioEngine::getTrackGain(int trackIndex) const
{
    if (auto* track = tracks[juce::jlimit(0, tracks.size() - 1, trackIndex)])
        return track->getGain();

    return 0.0f;
}

float WorkstationAudioEngine::getTrackPan(int trackIndex) const
{
    if (auto* track = tracks[juce::jlimit(0, tracks.size() - 1, trackIndex)])
        return track->getPan();

    return 0.0f;
}

bool WorkstationAudioEngine::isTrackMuted(int trackIndex) const
{
    if (auto* track = tracks[juce::jlimit(0, tracks.size() - 1, trackIndex)])
        return track->isMuted();

    return false;
}

bool WorkstationAudioEngine::isTrackSoloed(int trackIndex) const
{
    if (auto* track = tracks[juce::jlimit(0, tracks.size() - 1, trackIndex)])
        return track->isSoloed();

    return false;
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

    if (auto tracksState = sessionState.getChildWithName("Tracks"); tracksState.isValid())
    {
        for (const auto child : tracksState)
        {
            auto trackIndex = (int) child.getProperty("index", -1);
            if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
                continue;

            auto* track = tracks[(size_t) trackIndex];
            track->setGain((float) child.getProperty("gain", track->getGain()));
            track->setPan((float) child.getProperty("pan", track->getPan()));
            track->setMuted((bool) child.getProperty("muted", track->isMuted()));
            track->setSoloed((bool) child.getProperty("soloed", track->isSoloed()));

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

    return true;
}
