#include "WorkstationAudioEngine.h"

#include <cmath>
#include <array>
#include <algorithm>

namespace
{
bool readAndResampleSection(juce::AudioFormatReader& reader,
                            int64 sourceStartSample,
                            int64 sourceSamplesToRead,
                            double targetSampleRate,
                            juce::AudioBuffer<float>& destination)
{
    const auto clampedSamplesToRead = juce::jmax<int64>(1, sourceSamplesToRead);
    const auto sourceRate = juce::jmax(1.0, reader.sampleRate);
    const auto outputRate = juce::jmax(1.0, targetSampleRate);
    const auto channelCount = juce::jmax(1, (int) reader.numChannels);

    juce::AudioBuffer<float> sourceBuffer(channelCount, (int) clampedSamplesToRead);
    sourceBuffer.clear();
    reader.read(&sourceBuffer, 0, (int) clampedSamplesToRead, sourceStartSample, true, true);

    if (std::abs(sourceRate - outputRate) < 0.5)
    {
        destination.setSize(juce::jmax(2, channelCount), (int) clampedSamplesToRead, false, false, true);
        destination.clear();
        for (int channel = 0; channel < channelCount; ++channel)
            destination.copyFrom(channel, 0, sourceBuffer, channel, 0, (int) clampedSamplesToRead);
        return true;
    }

    const auto durationSeconds = (double) clampedSamplesToRead / sourceRate;
    const auto targetSamples = juce::jmax(1, (int) std::llround(durationSeconds * outputRate));
    destination.setSize(juce::jmax(2, channelCount), targetSamples, false, false, true);
    destination.clear();

    const auto speedRatio = sourceRate / outputRate;
    for (int channel = 0; channel < channelCount; ++channel)
    {
        juce::LagrangeInterpolator interpolator;
        interpolator.reset();
        interpolator.process(speedRatio,
                             sourceBuffer.getReadPointer(channel),
                             destination.getWritePointer(channel),
                             targetSamples);
    }

    return true;
}

// Provides hosted plugins with transport/tempo/PPQ info. No plugin instance was ever given an
// AudioPlayHead in this codebase - some plugins (e.g. a sampler with a tempo-synced Grooves
// feature, which SSD5 has) may dereference a null/absent playhead as soon as they actually need
// to render a note against host tempo, not before - which matches the observed crash exactly:
// identical fault signature whether triggered by idle processing, an offline render loop, or a
// single live note, always only once real note processing actually happens.
class EngineTransportPlayHead final : public juce::AudioPlayHead
{
public:
    void update(bool isPlayingNow, double bpm, int64 samplePosition, double sampleRate) noexcept
    {
        playing.store(isPlayingNow);
        tempoBpm.store(juce::jmax(1.0, bpm));
        auto seconds = sampleRate > 0.0 ? (double) samplePosition / sampleRate : 0.0;
        ppqPosition.store(seconds * tempoBpm.load() / 60.0);
    }

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying(playing.load());
        info.setBpm(tempoBpm.load());
        info.setPpqPosition(ppqPosition.load());
        info.setTimeSignature(TimeSignature { 4, 4 });
        return info;
    }

private:
    std::atomic<bool> playing { false };
    std::atomic<double> tempoBpm { 120.0 };
    std::atomic<double> ppqPosition { 0.0 };
};

EngineTransportPlayHead enginePlayHead;

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

// Enables only the main input/output bus, explicitly disabling any others - e.g. the per-piece
// multi-out buses a drum sampler like SSD5 exposes (confirmed: 48 output channels). Reaper only
// activates the main stereo pair by default; enabling every bus can cause a multi-out instrument
// to spread its audio across its individual outs instead of the consolidated main bus we read
// from, producing silence on the main bus even though the plugin is processing correctly.
void configureMainBusOnly(juce::AudioPluginInstance& instance)
{
    if (instance.getBusCount(true) > 0)
    {
        if (auto* mainIn = instance.getBus(true, 0))
            mainIn->enable(true);

        for (int i = 1; i < instance.getBusCount(true); ++i)
            if (auto* bus = instance.getBus(true, i))
                bus->enable(false);
    }

    if (instance.getBusCount(false) > 0)
    {
        if (auto* mainOut = instance.getBus(false, 0))
            mainOut->enable(true);

        for (int i = 1; i < instance.getBusCount(false); ++i)
            if (auto* bus = instance.getBus(false, i))
                bus->enable(false);
    }
}

juce::File getPluginStateDiagnosticsFile()
{
    auto logDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("Creation Station");
    logDirectory.createDirectory();
    return logDirectory.getChildFile("plugin-state-diagnostics.log");
}

void appendPluginStateDiagnostic(const juce::String& eventName,
                                 const juce::String& pluginName,
                                 const juce::File& pluginFile,
                                 size_t stateBytes)
{
    auto line = juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S")
                + " | " + eventName
                + " | plugin=" + (pluginName.isNotEmpty() ? pluginName : "<unnamed>")
                + " | file=" + pluginFile.getFullPathName()
                + " | bytes=" + juce::String((int64) stateBytes)
                + "\n";
    getPluginStateDiagnosticsFile().appendText(line, false, false, "\n");
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
        configureMainBusOnly(*pluginInstance);
        pluginInstance->setPlayHead(&enginePlayHead);
        pluginInputChannels = juce::jmax(0, pluginInstance->getTotalNumInputChannels());
        pluginOutputChannels = juce::jmax(1, pluginInstance->getTotalNumOutputChannels());
        pluginInstance->setPlayConfigDetails(pluginInputChannels, pluginOutputChannels, sampleRate, blockSize);
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
    cachedState.reset();

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
            configureMainBusOnly(*instance);
            instance->setPlayHead(&enginePlayHead);
            pluginInputChannels = juce::jmax(0, instance->getTotalNumInputChannels());
            pluginOutputChannels = juce::jmax(1, instance->getTotalNumOutputChannels());
            instance->setPlayConfigDetails(pluginInputChannels, pluginOutputChannels, sampleRate, blockSize);

            if (savedState != nullptr && savedState->getSize() > 0)
            {
                instance->setStateInformation(savedState->getData(), (int) savedState->getSize());
                cachedState = *savedState;
                appendPluginStateDiagnostic("loadPlugin.prepared-state-before-activate",
                                            pluginDescription->name,
                                            file,
                                            savedState->getSize());
            }

            instance->prepareToPlay(sampleRate, blockSize);

            {
                const juce::ScopedLock lock(pluginInstanceLock);
                pluginInstance = std::move(instance);
            }
            pluginFile = file;
            bypassed.store(false);
            appendPluginStateDiagnostic("loadPlugin.success",
                                        pluginDescription->name,
                                        file,
                                        cachedState.getSize());
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
    const juce::ScopedLock lock(pluginInstanceLock);

    if (pluginInstance != nullptr)
    {
        pluginInstance->releaseResources();
        pluginInstance.reset();
    }

    pluginFile = {};
    bypassed.store(false);
    pluginInputChannels = 2;
    pluginOutputChannels = 2;
    cachedState.reset();
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
    appendPluginStateDiagnostic("copyStateTo",
                                pluginInstance->getName(),
                                pluginFile,
                                destination.getSize());
    return destination.getSize() > 0;
}

bool WorkstationAudioEngine::PluginInsertSource::restoreStateFrom(const juce::MemoryBlock& source)
{
    if (pluginInstance == nullptr || source.getSize() == 0)
        return false;

    pluginInstance->setStateInformation(source.getData(), (int) source.getSize());
    cachedState = source;
    appendPluginStateDiagnostic("restoreStateFrom",
                                pluginInstance->getName(),
                                pluginFile,
                                source.getSize());
    return true;
}

bool WorkstationAudioEngine::PluginInsertSource::reapplyCachedState()
{
    if (cachedState.getSize() == 0)
        return false;

    appendPluginStateDiagnostic("reapplyCachedState",
                                getPluginName(),
                                pluginFile,
                                cachedState.getSize());
    return restoreStateFrom(cachedState);
}

void WorkstationAudioEngine::PluginInsertSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;

    // Real-time-safe: loadPlugin()/unloadPlugin() only hold this lock for a pointer swap/reset,
    // never while constructing or destroying the plugin itself, so contention here is brief -
    // unlike letting the audio thread dereference a pointer that's mid-reset on another thread.
    const juce::ScopedTryLock scopedLock(pluginInstanceLock);
    if (! scopedLock.isLocked() || pluginInstance == nullptr)
        return;

    auto pluginChannelCount = juce::jmax(1, juce::jmax(pluginInputChannels, pluginOutputChannels));
    const auto deviceChannels = bufferToFill.buffer->getNumChannels();

    pluginBuffer.setSize(pluginChannelCount, bufferToFill.numSamples, false, false, true);

    for (int channel = 0; channel < pluginChannelCount; ++channel)
    {
        if (channel < pluginInputChannels && channel < deviceChannels)
            pluginBuffer.copyFrom(channel, 0, *bufferToFill.buffer, channel, bufferToFill.startSample, bufferToFill.numSamples);
        else
            pluginBuffer.clear(channel, 0, bufferToFill.numSamples);
    }

    pluginMidiBuffer.clear();
    pluginMidiBuffer.addEvents(pendingExternalMidi, 0, bufferToFill.numSamples, 0);
    pendingExternalMidi.clear();

    if (bypassed.load())
        pluginInstance->processBlockBypassed(pluginBuffer, pluginMidiBuffer);
    else
        pluginInstance->processBlock(pluginBuffer, pluginMidiBuffer);

    for (int channel = 0; channel < juce::jmin(pluginOutputChannels, deviceChannels); ++channel)
        bufferToFill.buffer->copyFrom(channel, bufferToFill.startSample, pluginBuffer, channel, 0, bufferToFill.numSamples);
}

int WorkstationAudioEngine::PluginInsertSource::getParameterCount() const
{
    return pluginInstance != nullptr ? pluginInstance->getParameters().size() : 0;
}

juce::String WorkstationAudioEngine::PluginInsertSource::getParameterName(int paramIndex) const
{
    if (pluginInstance == nullptr)
        return {};

    const auto& params = pluginInstance->getParameters();
    if (! juce::isPositiveAndBelow(paramIndex, params.size()))
        return {};

    return params[paramIndex]->getName(64);
}

float WorkstationAudioEngine::PluginInsertSource::getParameterValue(int paramIndex) const
{
    if (pluginInstance == nullptr)
        return 0.0f;

    const auto& params = pluginInstance->getParameters();
    if (! juce::isPositiveAndBelow(paramIndex, params.size()))
        return 0.0f;

    return params[paramIndex]->getValue();
}

bool WorkstationAudioEngine::PluginInsertSource::setParameterValueRealtime(int paramIndex, float normalizedValue)
{
    // Same try-lock-or-skip pattern as getNextAudioBlock above, guarding against a concurrent
    // message-thread loadPlugin()/unloadPlugin() swapping the instance out from under this call.
    const juce::ScopedTryLock scopedLock(pluginInstanceLock);
    if (! scopedLock.isLocked() || pluginInstance == nullptr)
        return false;

    const auto& params = pluginInstance->getParameters();
    if (! juce::isPositiveAndBelow(paramIndex, params.size()))
        return false;

    // setValue() (not setValueNotifyingHost()) - this is a per-block automation write, not a
    // user-gesture-driven UI change, so it must not trigger the listener/notification path.
    params[paramIndex]->setValue(juce::jlimit(0.0f, 1.0f, normalizedValue));
    return true;
}

void WorkstationAudioEngine::PluginInsertChain::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
    sampleRate = newSampleRate;
    blockSize = samplesPerBlockExpected;

    for (auto* insert : inserts)
        if (insert != nullptr)
            insert->prepareToPlay(blockSize, sampleRate);
}

void WorkstationAudioEngine::PluginInsertChain::releaseResources()
{
    for (auto* insert : inserts)
        if (insert != nullptr)
            insert->releaseResources();
}

void WorkstationAudioEngine::PluginInsertChain::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    for (auto* insert : inserts)
        if (insert != nullptr && insert->hasPlugin())
            insert->getNextAudioBlock(bufferToFill);
}

void WorkstationAudioEngine::PluginInsertChain::pushLiveMidiToFirstSlot(const juce::MidiBuffer& midi)
{
    if (! inserts.isEmpty() && inserts.getFirst() != nullptr)
        inserts.getFirst()->addExternalMidi(midi);
}

int WorkstationAudioEngine::PluginInsertChain::getParameterCount(int slotIndex) const
{
    if (juce::isPositiveAndBelow(slotIndex, inserts.size()))
        if (auto* insert = inserts[slotIndex])
            return insert->getParameterCount();

    return 0;
}

juce::String WorkstationAudioEngine::PluginInsertChain::getParameterName(int slotIndex, int paramIndex) const
{
    if (juce::isPositiveAndBelow(slotIndex, inserts.size()))
        if (auto* insert = inserts[slotIndex])
            return insert->getParameterName(paramIndex);

    return {};
}

float WorkstationAudioEngine::PluginInsertChain::getParameterValue(int slotIndex, int paramIndex) const
{
    if (juce::isPositiveAndBelow(slotIndex, inserts.size()))
        if (auto* insert = inserts[slotIndex])
            return insert->getParameterValue(paramIndex);

    return 0.0f;
}

bool WorkstationAudioEngine::PluginInsertChain::setParameterValueRealtime(int slotIndex, int paramIndex, float normalizedValue)
{
    if (juce::isPositiveAndBelow(slotIndex, inserts.size()))
        if (auto* insert = inserts[slotIndex])
            return insert->setParameterValueRealtime(paramIndex, normalizedValue);

    return false;
}

bool WorkstationAudioEngine::PluginInsertChain::addPlugin(const juce::File& file, juce::String& errorMessage)
{
    return addPlugin(file, nullptr, false, errorMessage);
}

bool WorkstationAudioEngine::PluginInsertChain::addPlugin(const juce::File& file,
                                                          const juce::MemoryBlock* savedState,
                                                          bool bypassed,
                                                          juce::String& errorMessage)
{
    return insertPlugin(inserts.size(), file, savedState, bypassed, errorMessage);
}

bool WorkstationAudioEngine::PluginInsertChain::insertPlugin(int slotIndex,
                                                             const juce::File& file,
                                                             const juce::MemoryBlock* savedState,
                                                             bool bypassed,
                                                             juce::String& errorMessage)
{
    auto insert = std::make_unique<PluginInsertSource>();
    insert->prepareToPlay(blockSize, sampleRate);

    if (! insert->loadPlugin(file, savedState, errorMessage))
        return false;

    insert->setBypassed(bypassed);
    inserts.insert(juce::jlimit(0, inserts.size(), slotIndex), insert.release());
    return true;
}

void WorkstationAudioEngine::PluginInsertChain::removeLastPlugin()
{
    if (! inserts.isEmpty())
        inserts.removeLast();
}

void WorkstationAudioEngine::PluginInsertChain::removePlugin(int slotIndex)
{
    if (juce::isPositiveAndBelow(slotIndex, inserts.size()))
        inserts.remove(slotIndex);
}

bool WorkstationAudioEngine::PluginInsertChain::movePlugin(int fromSlotIndex, int toSlotIndex)
{
    if (! juce::isPositiveAndBelow(fromSlotIndex, inserts.size()))
        return false;

    if (! juce::isPositiveAndBelow(toSlotIndex, inserts.size()))
        return false;

    if (fromSlotIndex == toSlotIndex)
        return true;

    inserts.move(fromSlotIndex, toSlotIndex);
    return true;
}

void WorkstationAudioEngine::PluginInsertChain::clear()
{
    inserts.clear(true);
}

juce::String WorkstationAudioEngine::PluginInsertChain::getPluginName(int slotIndex) const
{
    if (juce::isPositiveAndBelow(slotIndex, inserts.size()))
        if (auto* insert = inserts[slotIndex])
            return insert->getPluginName();

    return {};
}

juce::StringArray WorkstationAudioEngine::PluginInsertChain::getPluginNames() const
{
    juce::StringArray names;
    for (auto* insert : inserts)
        names.add(insert != nullptr && insert->getPluginName().isNotEmpty() ? insert->getPluginName()
                                                                            : "Unnamed plugin");

    return names;
}

juce::Array<bool> WorkstationAudioEngine::PluginInsertChain::getBypassStates() const
{
    juce::Array<bool> states;
    for (auto* insert : inserts)
        states.add(insert != nullptr && insert->isBypassed());

    return states;
}

juce::String WorkstationAudioEngine::PluginInsertChain::getSummaryName() const
{
    if (inserts.isEmpty())
        return {};

    juce::StringArray names;
    for (auto* insert : inserts)
        if (insert != nullptr && insert->getPluginName().isNotEmpty())
            names.add(insert->getPluginName());

    return names.joinIntoString(" -> ");
}

juce::File WorkstationAudioEngine::PluginInsertChain::getPluginFile(int slotIndex) const
{
    if (juce::isPositiveAndBelow(slotIndex, inserts.size()))
        if (auto* insert = inserts[slotIndex])
            return insert->getPluginFile();

    return {};
}

void WorkstationAudioEngine::PluginInsertChain::setLastBypassed(bool shouldBypass) noexcept
{
    if (auto* insert = inserts.getLast())
        insert->setBypassed(shouldBypass);
}

void WorkstationAudioEngine::PluginInsertChain::setBypassed(int slotIndex, bool shouldBypass) noexcept
{
    if (juce::isPositiveAndBelow(slotIndex, inserts.size()))
        if (auto* insert = inserts[slotIndex])
            insert->setBypassed(shouldBypass);
}

bool WorkstationAudioEngine::PluginInsertChain::isLastBypassed() const noexcept
{
    if (auto* insert = inserts.getLast())
        return insert->isBypassed();

    return false;
}

bool WorkstationAudioEngine::PluginInsertChain::isBypassed(int slotIndex) const noexcept
{
    if (juce::isPositiveAndBelow(slotIndex, inserts.size()))
        if (auto* insert = inserts[slotIndex])
            return insert->isBypassed();

    return false;
}

juce::AudioProcessorEditor* WorkstationAudioEngine::PluginInsertChain::createLastEditor()
{
    if (auto* insert = inserts.getLast())
        return insert->createEditor();

    return nullptr;
}

juce::AudioProcessorEditor* WorkstationAudioEngine::PluginInsertChain::createEditor(int slotIndex)
{
    if (juce::isPositiveAndBelow(slotIndex, inserts.size()))
        if (auto* insert = inserts[slotIndex])
            return insert->createEditor();

    return nullptr;
}

bool WorkstationAudioEngine::PluginInsertChain::copyStateTo(int slotIndex, juce::MemoryBlock& destination) const
{
    if (juce::isPositiveAndBelow(slotIndex, inserts.size()))
        if (auto* insert = inserts[slotIndex])
            return insert->copyStateTo(destination);

    destination.reset();
    return false;
}

WorkstationAudioEngine::TrackChannelSource::TrackChannelSource(juce::String trackName, double frequencyHz)
    : source(std::move(trackName), frequencyHz)
{
}

void WorkstationAudioEngine::TrackChannelSource::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
    source.prepareToPlay(samplesPerBlockExpected, newSampleRate);
    insertChain.prepareToPlay(samplesPerBlockExpected, newSampleRate);
}

void WorkstationAudioEngine::TrackChannelSource::releaseResources()
{
    insertChain.releaseResources();
    source.releaseResources();
}

void WorkstationAudioEngine::TrackChannelSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;

    bufferToFill.clearActiveBufferRegion();

    // An automation track has no source or output of its own - it only pushes values into other
    // tracks' controls once per block (see WorkstationAudioEngine::applyAutomationForBlock).
    if (isAutomationKind.load())
        return;

    source.getNextAudioBlock(bufferToFill);
    insertChain.getNextAudioBlock(bufferToFill);
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

    bufferToFill.clearActiveBufferRegion();

    // `source` (the live per-track mixer/insert-chain path) is for live monitoring/audition
    // only - clip playback FX is already applied independently inside ArrangementSource, per
    // track, only for tracks with an actual clip sounding in this block. So `source` must NOT
    // run just because the transport is playing: continuously running every loaded plugin's
    // processBlock destabilized at least one real-world instrument plugin (confirmed crash,
    // same signature, both at idle and during playback) when driven with no real need to. It
    // only runs for tracks the user explicitly asked to hear live - armed or monitor-enabled -
    // or while previewing an asset.
    const auto liveMonitoring = owner.anyTrackNeedsLiveMonitoring();
    const auto hasRecentMidiActivity = owner.liveAudioTailSamplesRemaining > 0;
    if (liveMonitoring || owner.assetPreviewSource.isPreviewing() || hasRecentMidiActivity)
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
    if (! readAndResampleSection(*reader, startSample, sliceSamples, sampleRate, previewBuffer))
    {
        errorMessage = "That file could not be resampled for preview.";
        delete reader;
        return false;
    }
    delete reader;

    if (settings.normalize)
    {
        auto peak = previewBuffer.getMagnitude(0, previewBuffer.getNumSamples());
        if (peak > 0.0f)
            previewBuffer.applyGain(0.95f / peak);
    }

    auto gain = juce::Decibels::decibelsToGain(settings.gainDecibels);
    previewBuffer.applyGain(gain);

    auto previewSamples = previewBuffer.getNumSamples();
    auto fadeInSamples = juce::jlimit(0, previewSamples, juce::roundToInt((float) previewSamples * settings.fadeInNormalized));
    auto fadeOutSamples = juce::jlimit(0, previewSamples, juce::roundToInt((float) previewSamples * settings.fadeOutNormalized));

    for (int channel = 0; channel < previewBuffer.getNumChannels(); ++channel)
    {
        if (fadeInSamples > 0)
            previewBuffer.applyGainRamp(channel, 0, fadeInSamples, 0.0f, 1.0f);

        if (fadeOutSamples > 0)
            previewBuffer.applyGainRamp(channel, previewSamples - fadeOutSamples, fadeOutSamples, 1.0f, 0.0f);
    }

    if (settings.reverse)
    {
        for (int channel = 0; channel < previewBuffer.getNumChannels(); ++channel)
        {
            auto* data = previewBuffer.getWritePointer(channel);
            std::reverse(data, data + previewSamples);
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
    const auto trackCount = owner.getTrackCount();

    // Automation tracks must be evaluated and applied before any track's own rendering below,
    // in this same callback, so a curve drawn in the tracker plays back block-accurately (not
    // one block delayed) - see WorkstationAudioEngine::applyAutomationForBlock.
    if (sampleRate > 0.0)
        owner.applyAutomationForBlock((double) blockStart / sampleRate);

    // Mixes every clip on `trackIndex` overlapping this block into `destination`. Shared between
    // the -1 (Foley/master-level) pseudo-track and every real track below.
    auto mixClipsInto = [&](int trackIndex, juce::AudioBuffer<float>& destination)
    {
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
                for (int channel = 0; channel < destination.getNumChannels(); ++channel)
                    destination.addFrom(channel, destOffset, clip.buffer, 0, clipOffset, numSamples);
            }
            else
            {
                for (int channel = 0; channel < destination.getNumChannels(); ++channel)
                {
                    auto sourceChannel = juce::jmin(channel, clipChannels - 1);
                    destination.addFrom(channel, destOffset, clip.buffer, sourceChannel, clipOffset, numSamples);
                }
            }

            renderedAnyClip = true;
        }

        return renderedAnyClip;
    };

    // The -1 pseudo-track (Foley/master-level clips not tied to a real track) has no
    // TrackChannelSource and can't be a folder-routing participant - always straight to master,
    // exactly as before.
    {
        trackRenderBuffer.setSize(juce::jmax(2, outputChannels), bufferToFill.numSamples, false, false, true);
        trackRenderBuffer.clear();

        if (mixClipsInto(-1, trackRenderBuffer))
        {
            for (int channel = 0; channel < outputChannels; ++channel)
            {
                auto sourceChannel = juce::jmin(channel, trackRenderBuffer.getNumChannels() - 1);
                bufferToFill.buffer->addFrom(channel, bufferToFill.startSample, trackRenderBuffer, sourceChannel, 0, bufferToFill.numSamples, 1.0f);
            }
        }
    }

    if (trackCount <= 0)
    {
        playbackSamplePosition += bufferToFill.numSamples;
        return;
    }

    if ((int) perTrackBuffers.size() != trackCount)
        perTrackBuffers.resize((size_t) trackCount);

    for (auto& buffer : perTrackBuffers)
    {
        buffer.setSize(juce::jmax(2, outputChannels), bufferToFill.numSamples, false, false, true);
        buffer.clear();
    }

    auto routing = owner.getCachedTrackRouting();

    // Processes one real track: mixes its clips, runs its insert chain, applies its own
    // gain/pan, then routes the result into its parent's buffer (if it has one and that parent
    // is itself a bus destination) or straight to master otherwise. Bus-destination tracks
    // (folders) always fully process even with no clips of their own, since children processed
    // earlier in `order` may already have summed audio into their buffer.
    auto processTrack = [&](int trackIndex)
    {
        if (! juce::isPositiveAndBelow(trackIndex, trackCount) || ! owner.shouldRenderTrack(trackIndex))
            return;

        auto& trackBuffer = perTrackBuffers[(size_t) trackIndex];
        auto renderedAnyClip = mixClipsInto(trackIndex, trackBuffer);

        auto isBusDestination = routing != nullptr && juce::isPositiveAndBelow(trackIndex, (int) routing->isBusDestination.size())
                                   && routing->isBusDestination[(size_t) trackIndex];

        if (! renderedAnyClip && ! isBusDestination)
            return;

        auto* track = owner.tracks[(size_t) trackIndex];
        if (track != nullptr && track->insertChain.hasPlugin())
        {
            juce::AudioSourceChannelInfo trackInfo(&trackBuffer, 0, bufferToFill.numSamples);
            track->insertChain.getNextAudioBlock(trackInfo);
        }

        const auto trackGain = juce::jlimit(0.0f, 2.0f, owner.getTrackGain(trackIndex));
        const auto trackPan = juce::jlimit(-1.0f, 1.0f, owner.getTrackPan(trackIndex));
        const auto parentTrackIndex = track != nullptr ? track->getParentTrackIndex() : -1;
        const auto hasValidParent = juce::isPositiveAndBelow(parentTrackIndex, trackCount);

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

            auto sourceChannel = juce::jmin(channel, trackBuffer.getNumChannels() - 1);

            if (hasValidParent)
                perTrackBuffers[(size_t) parentTrackIndex].addFrom(channel, 0, trackBuffer, sourceChannel, 0, bufferToFill.numSamples, channelGain);
            else
                bufferToFill.buffer->addFrom(channel, bufferToFill.startSample, trackBuffer, sourceChannel, 0, bufferToFill.numSamples, channelGain);
        }
    };

    if (routing != nullptr && (int) routing->renderOrder.size() == trackCount)
    {
        for (auto trackIndex : routing->renderOrder)
            processTrack(trackIndex);
    }
    else
    {
        // No cached routing yet (e.g. before the very first hierarchy publish) - plain index
        // order is correct as long as nothing has a parent yet, matching today's behaviour.
        for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex)
            processTrack(trackIndex);
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

    rebuildTrackRoutingCache();
    return trackIndex;
}

bool WorkstationAudioEngine::removeTrack(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return false;

    auto* track = tracks[(size_t) trackIndex];
    mixerSource.removeInputSource(track);
    tracks.remove(trackIndex);
    rebuildTrackRoutingCache();
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

void WorkstationAudioEngine::setTrackMidiInputChannel(int trackIndex, int channel)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setMidiInputChannel(juce::jlimit(0, 16, channel));
}

int WorkstationAudioEngine::getTrackMidiInputChannel(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->getMidiInputChannel();

    return 0;
}

void WorkstationAudioEngine::setTrackMidiInputDeviceId(int trackIndex, const juce::String& deviceId)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setMidiInputDeviceId(deviceId);
}

juce::String WorkstationAudioEngine::getTrackMidiInputDeviceId(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->getMidiInputDeviceId();

    return {};
}

void WorkstationAudioEngine::setTrackIsMidiKind(int trackIndex, bool isMidi)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setIsMidiKind(isMidi);
}

void WorkstationAudioEngine::setTrackIsAutomationKind(int trackIndex, bool isAutomation)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setIsAutomationKind(isAutomation);
}

void WorkstationAudioEngine::setTrackAutomationData(int trackIndex, const cs::AutomationTarget& target, const std::vector<cs::AutomationPoint>& points)
{
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return;

    auto data = std::make_shared<AutomationTrackData>();
    data->target = target;
    data->points = points;
    tracks[(size_t) trackIndex]->setAutomationData(std::move(data));
}

void WorkstationAudioEngine::setTrackAutomationWriteActive(int trackIndex, bool active)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setAutomationWriteActive(active);
}

void WorkstationAudioEngine::applyAutomationForBlock(double blockStartSeconds)
{
    for (int trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
    {
        auto* track = tracks[(size_t) trackIndex];
        if (track == nullptr || ! track->getIsAutomationKind() || track->getAutomationWriteActive())
            continue;

        auto data = track->getAutomationData();
        if (data == nullptr || data->points.empty() || data->target.kind == cs::AutomationTargetKind::none)
            continue;

        const auto& points = data->points;
        float value;

        if (blockStartSeconds <= points.front().seconds)
            value = points.front().value;
        else if (blockStartSeconds >= points.back().seconds)
            value = points.back().value;
        else
        {
            value = points.back().value;
            for (size_t i = 0; i + 1 < points.size(); ++i)
            {
                if (blockStartSeconds >= points[i].seconds && blockStartSeconds <= points[i + 1].seconds)
                {
                    value = cs::evaluateAutomationSegment(points[i], points[i + 1], blockStartSeconds);
                    break;
                }
            }
        }

        const auto& target = data->target;
        if (! juce::isPositiveAndBelow(target.targetTrackIndex, tracks.size()))
            continue;

        switch (target.kind)
        {
            case cs::AutomationTargetKind::trackVolume:
                // Gain is already 0..1 everywhere it's set manually (header slider, mixer fader,
                // X-Touch fader all use a 0..1 range) - automation's 0..1 curve value maps to it
                // directly, with no remapping, so a manually-ridden fader position and a drawn
                // automation point mean the same thing.
                tracks[(size_t) target.targetTrackIndex]->setGain(value);
                break;

            case cs::AutomationTargetKind::trackPan:
                tracks[(size_t) target.targetTrackIndex]->setPan(juce::jmap(value, 0.0f, 1.0f, -1.0f, 1.0f));
                break;

            case cs::AutomationTargetKind::pluginParameter:
                tracks[(size_t) target.targetTrackIndex]->insertChain.setParameterValueRealtime(target.pluginSlotIndex, target.pluginParameterIndex, value);
                break;

            case cs::AutomationTargetKind::none:
            default:
                break;
        }
    }
}

void WorkstationAudioEngine::setTrackHasOpenEditor(int trackIndex, bool hasOpenEditor)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setHasOpenEditor(hasOpenEditor);
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

    {
        const juce::ScopedLock lock(midiDeviceCollectorsLock);
        for (auto& entry : midiDeviceCollectors)
            if (entry.collector != nullptr)
                entry.collector->reset(sampleRate);
    }

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

    // Tail window: kept open for a few seconds after the last delivered note rather than just the
    // exact block a note-on/off fell in, so a triggered sound (e.g. a drum hit) has time to
    // actually render instead of being truncated to a single ~10-20ms audio block.
    constexpr double liveAudioTailSeconds = 4.0;
    const auto engineSampleRate = arrangementSource.getSampleRate();
    bool blockHadNewMidiActivity = false;

    enginePlayHead.update(playing.load(), metronomeBpm.load(), arrangementSource.getPlaybackSamplePosition(), engineSampleRate);

    // Drain each physical MIDI input device's own collector separately, keeping messages tagged
    // with which device they came from - needed so a track can be routed to one specific device
    // (setTrackMidiInputDeviceId) rather than every enabled device being merged into one stream.
    std::vector<std::pair<juce::String, juce::MidiBuffer>> deviceBlocks;
    {
        const juce::ScopedTryLock devicesLock(midiDeviceCollectorsLock);
        if (devicesLock.isLocked())
        {
            for (auto& entry : midiDeviceCollectors)
            {
                if (entry.collector == nullptr)
                    continue;

                juce::MidiBuffer buffer;
                entry.collector->removeNextBlockOfMessages(buffer, numSamples);
                if (! buffer.isEmpty())
                    deviceBlocks.emplace_back(entry.deviceId, std::move(buffer));
            }
        }
    }

    if (! deviceBlocks.empty())
    {
        blockHadNewMidiActivity = true;

        const auto midiRecording = midiRecordingActive.load();
        const auto recordBlockStartSample = arrangementSource.getPlaybackSamplePosition();

        for (int trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
        {
            auto* track = tracks[trackIndex];
            if (track == nullptr)
                continue;

            auto routedDeviceId = track->getMidiInputDeviceId();
            auto channelFilter = track->getMidiInputChannel();

            juce::MidiBuffer combined;
            for (const auto& deviceBlock : deviceBlocks)
            {
                if (routedDeviceId.isNotEmpty() && deviceBlock.first != routedDeviceId)
                    continue;

                if (channelFilter == 0)
                {
                    combined.addEvents(deviceBlock.second, 0, numSamples, 0);
                }
                else
                {
                    for (const auto metadata : deviceBlock.second)
                        if (metadata.getMessage().getChannel() == channelFilter)
                            combined.addEvent(metadata.getMessage(), metadata.samplePosition);
                }
            }

            if (combined.isEmpty())
                continue;

            track->insertChain.pushLiveMidiToFirstSlot(combined);

            // Capture note on/off events for later pairing into clip notes, for any armed track
            // while a MIDI recording session is active. Deliberately does NOT also require
            // getIsMidiKind() here - that flag is only refreshed at specific UI sync points and
            // can go stale independently of the track's real kind, silently blocking capture
            // even though live audio (which doesn't check it) works fine. An audio track with no
            // in-progress MIDI clip simply has nothing for these captured events to attach to.
            if (midiRecording && track->isRecordingArmed())
            {
                const juce::ScopedTryLock recordLock(recordedMidiEventsLock);
                if (recordLock.isLocked())
                {
                    for (const auto metadata : combined)
                    {
                        const auto& message = metadata.getMessage();
                        if (message.isNoteOnOrOff())
                            recordedMidiEvents.push_back({ trackIndex, message, recordBlockStartSample + metadata.samplePosition });
                    }
                }
            }
        }
    }

    // Deliver scheduled MIDI clip notes live, sample-accurately, through the same injection path
    // as a live keyboard - not offline-rendered. Naturally paced by the real audio callback rate,
    // just like live input, so it doesn't reproduce the instability an offline batch-render loop
    // caused in at least one real-world plugin.
    if (playing.load())
    {
        const juce::ScopedTryLock midiClipsLock(scheduledMidiClipsLock);
        if (midiClipsLock.isLocked() && ! scheduledMidiClips.empty())
        {
            const auto blockStart = arrangementSource.getPlaybackSamplePosition();
            const auto blockEnd = blockStart + numSamples;
            const auto tempoBpm = juce::jmax(1.0, metronomeBpm.load());

            const auto beatsToSamples = [tempoBpm, engineSampleRate](double beats)
            {
                return (int64) std::llround((beats * 60.0 / tempoBpm) * engineSampleRate);
            };

            for (const auto& clip : scheduledMidiClips)
            {
                if (! juce::isPositiveAndBelow(clip.trackIndex, tracks.size()))
                    continue;

                const auto clipStartSample = (int64) std::llround(clip.startSeconds * engineSampleRate);

                juce::MidiBuffer clipMidi;
                for (const auto& note : clip.notes)
                {
                    auto onSample = clipStartSample + beatsToSamples(note.startBeats);
                    auto offSample = clipStartSample + beatsToSamples(note.startBeats + note.lengthBeats);

                    if (onSample >= blockStart && onSample < blockEnd)
                        clipMidi.addEvent(juce::MidiMessage::noteOn((int) note.channel, note.pitch, (juce::uint8) note.velocity),
                                          (int) (onSample - blockStart));

                    if (offSample >= blockStart && offSample < blockEnd)
                        clipMidi.addEvent(juce::MidiMessage::noteOff((int) note.channel, note.pitch),
                                          (int) (offSample - blockStart));
                }

                if (! clipMidi.isEmpty())
                {
                    blockHadNewMidiActivity = true;
                    tracks[(size_t) clip.trackIndex]->insertChain.pushLiveMidiToFirstSlot(clipMidi);
                }
            }
        }
    }

    // Deliver any UI-driven audition requests (e.g. clicking a note in the piano roll) - queued
    // from the message thread, drained here on the audio thread, targeting one specific track
    // directly regardless of its MIDI channel filter.
    {
        std::vector<AuditionRequest> requestsToDeliver;
        {
            const juce::ScopedTryLock auditionLock(auditionRequestsLock);
            if (auditionLock.isLocked() && ! pendingAuditionRequests.empty())
            {
                requestsToDeliver = std::move(pendingAuditionRequests);
                pendingAuditionRequests.clear();
            }
        }

        for (const auto& request : requestsToDeliver)
        {
            if (! juce::isPositiveAndBelow(request.trackIndex, tracks.size()))
                continue;

            juce::MidiBuffer auditionMidi;
            if (request.noteOn)
                auditionMidi.addEvent(juce::MidiMessage::noteOn(1, request.pitch, (juce::uint8) request.velocity), 0);
            else
                auditionMidi.addEvent(juce::MidiMessage::noteOff(1, request.pitch), 0);

            blockHadNewMidiActivity = true;
            tracks[(size_t) request.trackIndex]->insertChain.pushLiveMidiToFirstSlot(auditionMidi);
        }
    }

    // Stopping the transport mid-note otherwise leaves a stuck voice sounding indefinitely - a
    // real host-side bug (the note's real-time-scheduled note-off was scheduled for a future
    // block that will now never arrive), not a plugin quirk.
    if (allNotesOffRequested.exchange(false))
    {
        for (auto* track : tracks)
        {
            if (track == nullptr)
                continue;

            juce::MidiBuffer panicMidi;
            for (int channel = 1; channel <= 16; ++channel)
            {
                panicMidi.addEvent(juce::MidiMessage::controllerEvent(channel, 123, 0), 0); // All Notes Off
                panicMidi.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), 0); // All Sound Off
            }

            blockHadNewMidiActivity = true;
            track->insertChain.pushLiveMidiToFirstSlot(panicMidi);
        }
    }

    if (blockHadNewMidiActivity)
        liveAudioTailSamplesRemaining = (int64) std::llround(liveAudioTailSeconds * engineSampleRate);
    else if (liveAudioTailSamplesRemaining > 0)
        liveAudioTailSamplesRemaining = juce::jmax<int64>(0, liveAudioTailSamplesRemaining - numSamples);

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

        if (track->getIsMidiKind())
        {
            // A MIDI track has no analog input signal of its own - don't meter, monitor, or
            // record whatever the leftover audio input channel selection happens to be.
            track->setInputLevel(0.0f);
            continue;
        }

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

bool WorkstationAudioEngine::anyTrackNeedsLiveMonitoring() const noexcept
{
    for (auto* track : tracks)
        if (track != nullptr
            && (track->isMonitoringEnabled()
                || track->isRecordingArmed()
                || (track->getHasOpenEditor() && track->getIsMidiKind())))
            return true;

    return false;
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

    return ! anySoloed || isTrackOrDescendantSoloed(trackIndex);
}

bool WorkstationAudioEngine::isTrackOrDescendantSoloed(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return false;

    for (int candidateIndex = 0; candidateIndex < tracks.size(); ++candidateIndex)
    {
        auto* candidate = tracks[(size_t) candidateIndex];
        if (candidate == nullptr || ! candidate->isSoloed())
            continue;

        // Walk candidate's ancestor chain (including itself); if it passes through trackIndex,
        // trackIndex is candidate's ancestor (or candidate itself) - i.e. trackIndex "contains"
        // a soloed track and must stay audible. Guard bounds the walk against any cycle.
        auto current = candidateIndex;
        for (int guard = 0; guard < tracks.size() && current >= 0; ++guard)
        {
            if (current == trackIndex)
                return true;

            current = juce::isPositiveAndBelow(current, tracks.size()) ? tracks[(size_t) current]->getParentTrackIndex() : -1;
        }
    }

    return false;
}

void WorkstationAudioEngine::setTrackParentIndex(int trackIndex, int parentTrackIndex)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->setParentTrackIndex(parentTrackIndex);

    rebuildTrackRoutingCache();
}

void WorkstationAudioEngine::moveTrackRange(int startIndex, int length, int destinationIndex)
{
    auto trackCount = tracks.size();
    if (length <= 0 || ! juce::isPositiveAndBelow(startIndex, trackCount) || startIndex + length > trackCount)
        return;

    destinationIndex = juce::jlimit(0, trackCount - length, destinationIndex);

    // Same erase-then-insert-adjusted-for-shift math as TimelineModel::moveTrackRange - the two
    // must agree exactly or the model and engine track arrays fall out of index-parity.
    juce::Array<TrackChannelSource*> moving;
    for (int i = 0; i < length; ++i)
        moving.add(tracks.removeAndReturn(startIndex));

    auto insertAt = destinationIndex > startIndex ? destinationIndex - length : destinationIndex;
    insertAt = juce::jlimit(0, tracks.size(), insertAt);

    for (int i = 0; i < moving.size(); ++i)
        tracks.insert(insertAt + i, moving[i]);

    rebuildTrackRoutingCache();
}

void WorkstationAudioEngine::rebuildTrackRoutingCache()
{
    auto trackCount = tracks.size();
    auto info = std::make_shared<TrackRoutingInfo>();
    info->isBusDestination.assign((size_t) trackCount, false);

    for (int i = 0; i < trackCount; ++i)
    {
        auto* track = tracks[(size_t) i];
        auto parent = track != nullptr ? track->getParentTrackIndex() : -1;
        if (juce::isPositiveAndBelow(parent, trackCount))
            info->isBusDestination[(size_t) parent] = true;
    }

    // Children-before-parents order: repeatedly place any not-yet-placed track whose parent (if
    // any) is already placed or absent. O(n^2) worst case, trivial at DAW track counts, and
    // simpler to get right than a recursive post-order walk.
    std::vector<bool> visited((size_t) trackCount, false);
    info->renderOrder.reserve((size_t) trackCount);
    auto remaining = trackCount;
    while (remaining > 0)
    {
        auto placedAny = false;
        for (int i = 0; i < trackCount; ++i)
        {
            if (visited[(size_t) i])
                continue;

            auto* track = tracks[(size_t) i];
            auto parent = track != nullptr ? track->getParentTrackIndex() : -1;
            auto parentReady = ! juce::isPositiveAndBelow(parent, trackCount) || visited[(size_t) parent];

            if (parentReady)
            {
                visited[(size_t) i] = true;
                info->renderOrder.push_back(i);
                --remaining;
                placedAny = true;
            }
        }

        // A cycle (shouldn't happen - TimelineModel::setTrackParent rejects them) would otherwise
        // spin forever; break it by force-placing whatever's left in index order.
        if (! placedAny)
        {
            for (int i = 0; i < trackCount; ++i)
                if (! visited[(size_t) i])
                    info->renderOrder.push_back(i);
            break;
        }
    }

    cachedTrackRouting.store(std::move(info));
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

bool WorkstationAudioEngine::isControlSurfaceMidiDevice(const juce::String& deviceName)
{
    auto lowered = deviceName.toLowerCase();
    return lowered.contains("x-touch")
        || lowered.contains("xtouch")
        || lowered.contains("x touch")
        || lowered.contains("mackie")
        || lowered.contains("bcr2000")
        || lowered.contains("b-control rotary")
        || lowered.contains("b-control");
}

void WorkstationAudioEngine::attachToDevice(juce::AudioDeviceManager& deviceManager)
{
    deviceManager.addAudioCallback(this);

    attachedDeviceManager = &deviceManager;
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (isControlSurfaceMidiDevice(device.name))
            continue;

        deviceManager.setMidiInputDeviceEnabled(device.identifier, true);
        deviceManager.addMidiInputDeviceCallback(device.identifier, this);
        getOrCreateMidiDeviceCollector(device.identifier);
    }
}

juce::MidiMessageCollector& WorkstationAudioEngine::getOrCreateMidiDeviceCollector(const juce::String& deviceId)
{
    const juce::ScopedLock lock(midiDeviceCollectorsLock);

    for (auto& entry : midiDeviceCollectors)
        if (entry.deviceId == deviceId)
            return *entry.collector;

    auto collector = std::make_unique<juce::MidiMessageCollector>();
    collector->reset(arrangementSource.getSampleRate());
    midiDeviceCollectors.push_back({ deviceId, std::move(collector) });
    return *midiDeviceCollectors.back().collector;
}

void WorkstationAudioEngine::detachFromDevice(juce::AudioDeviceManager& deviceManager)
{
    deviceManager.removeAudioCallback(this);

    for (const auto& device : juce::MidiInput::getAvailableDevices())
        if (! isControlSurfaceMidiDevice(device.name))
            deviceManager.removeMidiInputDeviceCallback(device.identifier, this);

    attachedDeviceManager = nullptr;
}

void WorkstationAudioEngine::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    if (source == nullptr)
        return;

    if (message.isNoteOn(true) || (message.isController() && message.getControllerValue() > 0))
    {
        auto number = message.isController() ? message.getControllerNumber() : message.getNoteNumber();
        offerMidiLearnCandidate(source->getIdentifier(), message.getChannel(), number, message.isController());
    }

    getOrCreateMidiDeviceCollector(source->getIdentifier()).addMessageToQueue(message);
}

void WorkstationAudioEngine::setPlaying(bool shouldPlay)
{
    playing.store(shouldPlay);

    for (auto* track : tracks)
        track->setPlaying(false);

    if (shouldPlay)
        metronomeSampleCounter = 0;
    else
        requestAllNotesOff();
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
        if (! readAndResampleSection(*reader, sourceStartSample, samplesToRead, graphSampleRate, clip.buffer))
        {
            errorMessage = "Could not resample recorded clip: " + target.file.getFileName();
            continue;
        }
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

void WorkstationAudioEngine::setTrackerMidiClips(const juce::Array<MidiPlaybackClip>& clips)
{
    std::vector<MidiPlaybackClip> newClips;
    newClips.reserve((size_t) clips.size());
    for (const auto& clip : clips)
        newClips.push_back(clip);

    const juce::ScopedLock scopedLock(scheduledMidiClipsLock);
    scheduledMidiClips = std::move(newClips);
}

void WorkstationAudioEngine::auditionNoteOn(int trackIndex, int pitch, int velocity)
{
    const juce::ScopedLock scopedLock(auditionRequestsLock);
    pendingAuditionRequests.push_back({ trackIndex, pitch, velocity, true });
}

void WorkstationAudioEngine::auditionNoteOff(int trackIndex, int pitch)
{
    const juce::ScopedLock scopedLock(auditionRequestsLock);
    pendingAuditionRequests.push_back({ trackIndex, pitch, 0, false });
}

void WorkstationAudioEngine::requestAllNotesOff()
{
    allNotesOffRequested.store(true);
}

void WorkstationAudioEngine::armMidiLearn(const juce::String& deviceIdFilter)
{
    const juce::ScopedLock lock(midiLearnLock);
    midiLearnState.armed = true;
    midiLearnState.deviceIdFilter = deviceIdFilter;
    midiLearnState.hasResult = false;
}

void WorkstationAudioEngine::cancelMidiLearn()
{
    const juce::ScopedLock lock(midiLearnLock);
    midiLearnState.armed = false;
}

bool WorkstationAudioEngine::isMidiLearnArmed() const noexcept
{
    const juce::ScopedLock lock(midiLearnLock);
    return midiLearnState.armed;
}

bool WorkstationAudioEngine::takeMidiLearnResult(MidiLearnResult& result)
{
    const juce::ScopedLock lock(midiLearnLock);
    if (! midiLearnState.hasResult)
        return false;

    result = midiLearnState.result;
    midiLearnState.hasResult = false;
    return true;
}

bool WorkstationAudioEngine::offerMidiLearnCandidate(const juce::String& deviceId, int channel, int number, bool isController)
{
    const juce::ScopedLock lock(midiLearnLock);

    if (! midiLearnState.armed)
        return false;

    if (midiLearnState.deviceIdFilter.isNotEmpty() && midiLearnState.deviceIdFilter != deviceId)
        return false;

    midiLearnState.result.deviceId = deviceId;
    midiLearnState.result.channel = channel;
    midiLearnState.result.number = number;
    midiLearnState.result.isController = isController;
    midiLearnState.hasResult = true;
    midiLearnState.armed = false;
    return true;
}

void WorkstationAudioEngine::startMidiRecording()
{
    {
        const juce::ScopedLock lock(recordedMidiEventsLock);
        recordedMidiEvents.clear();
    }

    midiRecordingActive.store(true);
}

void WorkstationAudioEngine::stopMidiRecording()
{
    midiRecordingActive.store(false);
}

std::vector<WorkstationAudioEngine::RecordedMidiEvent> WorkstationAudioEngine::takeRecordedMidiEvents()
{
    const juce::ScopedLock lock(recordedMidiEventsLock);
    auto events = std::move(recordedMidiEvents);
    recordedMidiEvents.clear();
    return events;
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

void WorkstationAudioEngine::reapplyHostedPluginStates()
{
    for (auto* track : tracks)
        if (track != nullptr)
            track->insertChain.reapplyCachedStates();

    masterInsertSource.reapplyCachedState();
    graphVstInsertSource.reapplyCachedState();
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
    return insertTrackPlugin(trackIndex, getTrackPluginCount(trackIndex), file, errorMessage);
}

bool WorkstationAudioEngine::insertTrackPlugin(int trackIndex, int slotIndex, const juce::File& file, juce::String& errorMessage)
{
    if (! juce::isPositiveAndBelow(trackIndex, tracks.size()))
    {
        errorMessage = "Track index is out of range.";
        return false;
    }

    return tracks[(size_t) trackIndex]->insertChain.insertPlugin(slotIndex, file, nullptr, false, errorMessage);
}

void WorkstationAudioEngine::unloadTrackPlugin(int trackIndex)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->insertChain.removeLastPlugin();
}

void WorkstationAudioEngine::unloadTrackPlugin(int trackIndex, int slotIndex)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->insertChain.removePlugin(slotIndex);
}

bool WorkstationAudioEngine::moveTrackPlugin(int trackIndex, int fromSlotIndex, int toSlotIndex)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.movePlugin(fromSlotIndex, toSlotIndex);

    return false;
}

juce::String WorkstationAudioEngine::getTrackPluginName(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.getSummaryName();

    return {};
}

juce::StringArray WorkstationAudioEngine::getTrackPluginNames(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.getPluginNames();

    return {};
}

juce::Array<bool> WorkstationAudioEngine::getTrackPluginBypassStates(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.getBypassStates();

    return {};
}

juce::File WorkstationAudioEngine::getTrackPluginFile(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
    {
        const auto count = tracks[(size_t) trackIndex]->insertChain.getPluginCount();
        return count > 0 ? tracks[(size_t) trackIndex]->insertChain.getPluginFile(count - 1)
                         : juce::File();
    }

    return {};
}

juce::File WorkstationAudioEngine::getTrackInstrumentPluginFile(int trackIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
    {
        const auto count = tracks[(size_t) trackIndex]->insertChain.getPluginCount();
        return count > 0 ? tracks[(size_t) trackIndex]->insertChain.getPluginFile(0)
                         : juce::File();
    }

    return {};
}

int WorkstationAudioEngine::getTrackPluginParameterCount(int trackIndex, int slotIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.getParameterCount(slotIndex);

    return 0;
}

juce::String WorkstationAudioEngine::getTrackPluginParameterName(int trackIndex, int slotIndex, int paramIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.getParameterName(slotIndex, paramIndex);

    return {};
}

float WorkstationAudioEngine::getTrackPluginParameterValue(int trackIndex, int slotIndex, int paramIndex) const
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.getParameterValue(slotIndex, paramIndex);

    return 0.0f;
}

void WorkstationAudioEngine::setTrackPluginParameterValueRealtime(int trackIndex, int slotIndex, int paramIndex, float normalizedValue)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->insertChain.setParameterValueRealtime(slotIndex, paramIndex, normalizedValue);
}

bool WorkstationAudioEngine::renderMidiClipToFile(const juce::File& instrumentPluginFile,
                                                  const std::vector<cs::MidiNoteEvent>& notes,
                                                  const std::vector<cs::MidiCCEvent>& ccEvents,
                                                  double tempoBpm,
                                                  double durationSeconds,
                                                  juce::File& outputFile,
                                                  juce::String& errorMessage) const
{
    if (! instrumentPluginFile.existsAsFile())
    {
        errorMessage = "No instrument plugin is loaded on this track.";
        return false;
    }

    if (notes.empty())
    {
        errorMessage = "The MIDI clip has no notes to render.";
        return false;
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    juce::OwnedArray<juce::PluginDescription> pluginDescriptions;
    for (auto* format : formatManager.getFormats())
    {
        if (format != nullptr && format->fileMightContainThisPluginType(instrumentPluginFile.getFullPathName()))
            format->findAllTypesForFile(pluginDescriptions, instrumentPluginFile.getFullPathName());
    }

    if (pluginDescriptions.isEmpty())
    {
        errorMessage = "Could not identify the instrument plugin format.";
        return false;
    }

    const auto sampleRate = juce::jmax(8000.0, graphSampleRate);
    const auto blockSize = juce::jmax(64, graphBlockSize);

    std::unique_ptr<juce::AudioPluginInstance> instance;
    for (auto* pluginDescription : pluginDescriptions)
    {
        if (pluginDescription == nullptr)
            continue;

        juce::String creationError;
        instance = formatManager.createPluginInstance(*pluginDescription, sampleRate, blockSize, creationError);
        if (instance != nullptr)
            break;
    }

    if (instance == nullptr)
    {
        errorMessage = "Could not create an instance of the instrument plugin for rendering.";
        return false;
    }

    configureMainBusOnly(*instance);
    instance->setPlayHead(&enginePlayHead);
    instance->setPlayConfigDetails(2, 2, sampleRate, blockSize);
    instance->prepareToPlay(sampleRate, blockSize);

    // Sample-accurate note/CC scheduling, converted from clip-relative beats to samples.
    const auto beatsToSamples = [tempoBpm, sampleRate](double beats)
    {
        return (int64) std::llround((beats * 60.0 / juce::jmax(1.0, tempoBpm)) * sampleRate);
    };

    juce::MidiBuffer fullMidi;
    for (const auto& note : notes)
    {
        auto onSample = beatsToSamples(note.startBeats);
        auto offSample = beatsToSamples(note.startBeats + note.lengthBeats);
        fullMidi.addEvent(juce::MidiMessage::noteOn((int) note.channel, note.pitch, (juce::uint8) note.velocity), (int) onSample);
        fullMidi.addEvent(juce::MidiMessage::noteOff((int) note.channel, note.pitch), (int) juce::jmax(onSample + 1, offSample));
    }

    for (const auto& point : ccEvents)
        fullMidi.addEvent(juce::MidiMessage::controllerEvent(1, point.controller, point.value), (int) beatsToSamples(point.beats));

    // Add a release tail so one-shot samples and reverb/decay aren't cut off.
    auto tailSeconds = juce::jlimit(0.0, 4.0, instance->getTailLengthSeconds());
    const auto totalSamples = (int64) std::ceil((durationSeconds + juce::jmax(0.5, tailSeconds)) * sampleRate);

    juce::AudioBuffer<float> outputBuffer(2, (int) totalSamples);
    outputBuffer.clear();

    juce::AudioBuffer<float> blockBuffer(2, blockSize);

    for (int64 blockStart = 0; blockStart < totalSamples; blockStart += blockSize)
    {
        const auto samplesThisBlock = (int) juce::jmin<int64>(blockSize, totalSamples - blockStart);

        blockBuffer.clear();

        juce::MidiBuffer blockMidi;
        for (const auto metadata : fullMidi)
        {
            auto samplePos = (int64) metadata.samplePosition;
            if (samplePos >= blockStart && samplePos < blockStart + samplesThisBlock)
                blockMidi.addEvent(metadata.getMessage(), (int) (samplePos - blockStart));
        }

        instance->processBlock(blockBuffer, blockMidi);
        outputBuffer.copyFrom(0, (int) blockStart, blockBuffer, 0, 0, samplesThisBlock);
        outputBuffer.copyFrom(1, (int) blockStart, blockBuffer, 1, 0, samplesThisBlock);
    }

    instance->releaseResources();
    instance.reset();

    auto cacheDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("CreationStationMidiRender");
    cacheDir.createDirectory();
    auto renderFile = cacheDir.getChildFile("clip_" + juce::Uuid().toString() + ".wav");

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream(renderFile.createOutputStream());
    if (outputStream == nullptr)
    {
        errorMessage = "Could not create a temporary render file.";
        return false;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(outputStream.get(), sampleRate, 2, 24, {}, 0));
    if (writer == nullptr)
    {
        errorMessage = "Could not create a WAV writer for the rendered clip.";
        return false;
    }

    outputStream.release(); // writer now owns the stream
    writer->writeFromAudioSampleBuffer(outputBuffer, 0, outputBuffer.getNumSamples());
    writer.reset();

    outputFile = renderFile;
    return true;
}

bool WorkstationAudioEngine::hasTrackPlugin(int trackIndex) const noexcept
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.hasPlugin();

    return false;
}

int WorkstationAudioEngine::getTrackPluginCount(int trackIndex) const noexcept
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.getPluginCount();

    return 0;
}

void WorkstationAudioEngine::setTrackPluginBypassed(int trackIndex, bool shouldBypass)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->insertChain.setLastBypassed(shouldBypass);
}

void WorkstationAudioEngine::setTrackPluginBypassed(int trackIndex, int slotIndex, bool shouldBypass)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        tracks[(size_t) trackIndex]->insertChain.setBypassed(slotIndex, shouldBypass);
}

bool WorkstationAudioEngine::isTrackPluginBypassed(int trackIndex) const noexcept
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.isLastBypassed();

    return false;
}

bool WorkstationAudioEngine::isTrackPluginBypassed(int trackIndex, int slotIndex) const noexcept
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.isBypassed(slotIndex);

    return false;
}

juce::AudioProcessorEditor* WorkstationAudioEngine::createTrackPluginEditor(int trackIndex)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.createLastEditor();

    return nullptr;
}

juce::AudioProcessorEditor* WorkstationAudioEngine::createTrackPluginEditor(int trackIndex, int slotIndex)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.createEditor(slotIndex);

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
        trackState.setProperty("midiInputChannel", track->getMidiInputChannel(), nullptr);
        trackState.setProperty("midiInputDeviceId", track->getMidiInputDeviceId(), nullptr);
        trackState.setProperty("recordingArmed", track->isRecordingArmed(), nullptr);
        trackState.setProperty("monitoringEnabled", track->isMonitoringEnabled(), nullptr);
        trackState.setProperty("stereoEnabled", track->isStereoEnabled(), nullptr);

        juce::ValueTree insertChainState("InsertChain");
        insertChainState.setProperty("count", track->insertChain.getPluginCount(), nullptr);
        for (int slotIndex = 0; slotIndex < track->insertChain.getPluginCount(); ++slotIndex)
        {
            juce::ValueTree slotState("Insert");
            slotState.setProperty("slot", slotIndex, nullptr);
            slotState.setProperty("file", track->insertChain.getPluginFile(slotIndex).getFullPathName(), nullptr);
            slotState.setProperty("name", track->insertChain.getPluginName(slotIndex), nullptr);

            juce::MemoryBlock pluginState;
            if (track->insertChain.copyStateTo(slotIndex, pluginState))
                slotState.setProperty("state", juce::Base64::toBase64(pluginState.getData(), pluginState.getSize()), nullptr);

            slotState.setProperty("bypassed", track->insertChain.isBypassed(slotIndex), nullptr);

            insertChainState.addChild(slotState, -1, nullptr);
        }

        trackState.addChild(insertChainState, -1, nullptr);
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

juce::String WorkstationAudioEngine::createHostedPluginStateSignature() const
{
    juce::ValueTree state("HostedPluginState");

    juce::ValueTree tracksState("Tracks");
    for (int trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
    {
        auto* track = tracks[(size_t) trackIndex];
        juce::ValueTree trackState("Track");
        trackState.setProperty("index", trackIndex, nullptr);

        juce::ValueTree insertChainState("InsertChain");
        insertChainState.setProperty("count", track->insertChain.getPluginCount(), nullptr);
        for (int slotIndex = 0; slotIndex < track->insertChain.getPluginCount(); ++slotIndex)
        {
            juce::ValueTree slotState("Insert");
            slotState.setProperty("slot", slotIndex, nullptr);
            slotState.setProperty("file", track->insertChain.getPluginFile(slotIndex).getFullPathName(), nullptr);
            slotState.setProperty("bypassed", track->insertChain.isBypassed(slotIndex), nullptr);

            juce::MemoryBlock pluginState;
            if (track->insertChain.copyStateTo(slotIndex, pluginState))
                slotState.setProperty("state", juce::Base64::toBase64(pluginState.getData(), pluginState.getSize()), nullptr);

            insertChainState.addChild(slotState, -1, nullptr);
        }

        trackState.addChild(insertChainState, -1, nullptr);
        tracksState.addChild(trackState, -1, nullptr);
    }

    state.addChild(tracksState, -1, nullptr);

    juce::ValueTree masterInsert("MasterInsert");
    masterInsert.setProperty("bypassed", masterInsertSource.isBypassed(), nullptr);
    masterInsert.setProperty("file", masterInsertSource.getPluginFile().getFullPathName(), nullptr);
    juce::MemoryBlock masterState;
    if (masterInsertSource.copyStateTo(masterState))
        masterInsert.setProperty("state", juce::Base64::toBase64(masterState.getData(), masterState.getSize()), nullptr);
    state.addChild(masterInsert, -1, nullptr);

    juce::ValueTree graphInsert("GraphInsert");
    graphInsert.setProperty("enabled", graphVstEnabled.load(), nullptr);
    graphInsert.setProperty("mix", graphVstMix.load(), nullptr);
    graphInsert.setProperty("file", graphVstInsertSource.getPluginFile().getFullPathName(), nullptr);
    juce::MemoryBlock graphState;
    if (graphVstInsertSource.copyStateTo(graphState))
        graphInsert.setProperty("state", juce::Base64::toBase64(graphState.getData(), graphState.getSize()), nullptr);
    state.addChild(graphInsert, -1, nullptr);

    if (auto xml = state.createXml())
        return xml->toString();

    return {};
}

bool WorkstationAudioEngine::reapplyTrackPluginState(int trackIndex, int slotIndex)
{
    if (juce::isPositiveAndBelow(trackIndex, tracks.size()))
        return tracks[(size_t) trackIndex]->insertChain.reapplyCachedState(slotIndex);

    return false;
}

void WorkstationAudioEngine::PluginInsertChain::reapplyCachedStates()
{
    for (auto* insert : inserts)
        if (insert != nullptr && insert->hasPlugin())
            insert->reapplyCachedState();
}

bool WorkstationAudioEngine::PluginInsertChain::reapplyCachedState(int slotIndex)
{
    if (juce::isPositiveAndBelow(slotIndex, inserts.size()))
        if (auto* insert = inserts[slotIndex])
            return insert->hasPlugin() && insert->reapplyCachedState();

    return false;
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
            track->setMidiInputChannel((int) child.getProperty("midiInputChannel", track->getMidiInputChannel()));
            track->setMidiInputDeviceId(child.getProperty("midiInputDeviceId", track->getMidiInputDeviceId()).toString());
            track->setRecordingArmed((bool) child.getProperty("recordingArmed", false));
            track->setMonitoringEnabled((bool) child.getProperty("monitoringEnabled", false));
            track->setStereoEnabled((bool) child.getProperty("stereoEnabled", false));

            if (auto insertChainState = child.getChildWithName("InsertChain"); insertChainState.isValid())
            {
                track->insertChain.clear();
                for (const auto slotState : insertChainState)
                {
                    auto filePath = slotState.getProperty("file").toString();
                    if (filePath.isEmpty())
                        continue;

                    juce::MemoryBlock pluginState;
                    auto encoded = slotState.getProperty("state").toString();
                    if (encoded.isNotEmpty())
                        pluginState.fromBase64Encoding(encoded);

                    juce::String loadError;
                    if (! track->insertChain.addPlugin(juce::File(filePath),
                                                       pluginState.getSize() > 0 ? &pluginState : nullptr,
                                                       (bool) slotState.getProperty("bypassed", false),
                                                       loadError))
                    {
                        errorMessage = loadError;
                        continue;
                    }
                }
            }
            else if (auto insertState = child.getChildWithName("Insert"); insertState.isValid())
            {
                auto filePath = insertState.getProperty("file").toString();
                if (filePath.isEmpty())
                    continue;

                juce::MemoryBlock pluginState;
                auto encoded = insertState.getProperty("state").toString();
                if (encoded.isNotEmpty())
                    pluginState.fromBase64Encoding(encoded);

                juce::String loadError;
                track->insertChain.clear();
                if (! track->insertChain.addPlugin(juce::File(filePath),
                                                   pluginState.getSize() > 0 ? &pluginState : nullptr,
                                                   (bool) insertState.getProperty("bypassed", false),
                                                   loadError))
                {
                    errorMessage = loadError;
                    continue;
                }
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
