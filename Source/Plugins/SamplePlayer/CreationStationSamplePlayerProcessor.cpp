#include "CreationStationSamplePlayerProcessor.h"
#include "CreationStationSamplePlayerEditor.h"

namespace cs::plugins
{
namespace
{
constexpr auto attackId = "attack";
constexpr auto decayId = "decay";
constexpr auto sustainId = "sustain";
constexpr auto releaseId = "release";
}

juce::AudioProcessorValueTreeState::ParameterLayout CreationStationSamplePlayerProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    for (int layer = 0; layer < numLayers; ++layer)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { layerGainParamId(layer), 1 }, "Layer " + juce::String(layer + 1) + " Gain",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.8f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { attackId, 1 }, "Attack",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.35f), 0.001f,
        juce::AudioParameterFloatAttributes().withLabel("s")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { decayId, 1 }, "Decay",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.35f), 0.05f,
        juce::AudioParameterFloatAttributes().withLabel("s")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { sustainId, 1 }, "Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { releaseId, 1 }, "Release",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.35f), 0.2f,
        juce::AudioParameterFloatAttributes().withLabel("s")));

    return { params.begin(), params.end() };
}

CreationStationSamplePlayerProcessor::CreationStationSamplePlayerProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();

    for (auto& synth : layerSynths)
        for (int i = 0; i < numVoicesPerLayer; ++i)
            synth.addVoice(new juce::SamplerVoice());
}

CreationStationSamplePlayerProcessor::~CreationStationSamplePlayerProcessor()
{
}

void CreationStationSamplePlayerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (auto& synth : layerSynths)
        synth.setCurrentPlaybackSampleRate(sampleRate);

    layerScratchBuffer.setSize(2, samplesPerBlock);
}

void CreationStationSamplePlayerProcessor::releaseResources()
{
}

void CreationStationSamplePlayerProcessor::applyEnvelopeToAllSounds()
{
    for (auto& synth : layerSynths)
    {
        for (int i = 0; i < synth.getNumSounds(); ++i)
        {
            if (auto* samplerSound = dynamic_cast<juce::SamplerSound*>(synth.getSound(i).get()))
                samplerSound->setEnvelopeParameters(cachedEnvelope);
        }
    }
}

void CreationStationSamplePlayerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    // Re-check the ADSR knobs each block (cheap) rather than wiring a parameter-listener - a
    // slow-moving UI knob doesn't need lower latency than "next block."
    juce::ADSR::Parameters liveEnvelope
    {
        apvts.getRawParameterValue(attackId)->load(),
        apvts.getRawParameterValue(decayId)->load(),
        apvts.getRawParameterValue(sustainId)->load(),
        apvts.getRawParameterValue(releaseId)->load()
    };

    if (liveEnvelope.attack != cachedEnvelope.attack || liveEnvelope.decay != cachedEnvelope.decay
        || liveEnvelope.sustain != cachedEnvelope.sustain || liveEnvelope.release != cachedEnvelope.release)
    {
        cachedEnvelope = liveEnvelope;
        applyEnvelopeToAllSounds();
    }

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    buffer.clear();

    for (int layer = 0; layer < numLayers; ++layer)
    {
        if (layerSynths[(size_t) layer].getNumSounds() == 0)
            continue;

        layerScratchBuffer.setSize(numChannels, numSamples, false, false, true);
        layerScratchBuffer.clear();

        layerSynths[(size_t) layer].renderNextBlock(layerScratchBuffer, midiMessages, 0, numSamples);

        auto gain = apvts.getRawParameterValue(layerGainParamId(layer))->load();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.addFrom(channel, 0, layerScratchBuffer, channel, 0, buffer.getNumSamples(), gain);
    }

    auto magnitude = buffer.getMagnitude(0, buffer.getNumSamples());
    outputLevelDb.store(juce::Decibels::gainToDecibels(magnitude, -60.0f));
}

juce::AudioProcessorEditor* CreationStationSamplePlayerProcessor::createEditor()
{
    return new CreationStationSamplePlayerEditor(*this);
}

void CreationStationSamplePlayerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    juce::ValueTree layers("Layers");
    for (int layer = 0; layer < numLayers; ++layer)
    {
        juce::ValueTree layerNode("Layer");
        layerNode.setProperty("index", layer, nullptr);
        layerNode.setProperty("packFolder", layerPackFolders[(size_t) layer].getFullPathName(), nullptr);
        layers.addChild(layerNode, -1, nullptr);
    }
    state.addChild(layers, -1, nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CreationStationSamplePlayerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName(apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml(*xml);
    apvts.replaceState(state);

    auto layers = state.getChildWithName("Layers");
    if (! layers.isValid())
        return;

    for (const auto layerNode : layers)
    {
        if (! layerNode.hasType("Layer"))
            continue;

        auto index = (int) layerNode.getProperty("index", -1);
        auto packPath = layerNode.getProperty("packFolder").toString();

        if (juce::isPositiveAndBelow(index, numLayers) && packPath.isNotEmpty())
        {
            juce::String errorMessage;
            loadLayerPack(index, juce::File(packPath), errorMessage);
        }
    }
}

bool CreationStationSamplePlayerProcessor::loadLayerPack(int layerIndex, const juce::File& packFolder, juce::String& errorMessage)
{
    if (! juce::isPositiveAndBelow(layerIndex, numLayers))
    {
        errorMessage = "Invalid layer index.";
        return false;
    }

    if (! packFolder.isDirectory())
    {
        errorMessage = "That folder does not exist.";
        return false;
    }

    juce::Array<juce::File> noteFiles;
    packFolder.findChildFiles(noteFiles, juce::File::findFiles, false, "Note_*.wav");

    if (noteFiles.isEmpty())
    {
        errorMessage = "That folder doesn't contain any Note_NNN.wav files.";
        return false;
    }

    auto& synth = layerSynths[(size_t) layerIndex];
    synth.clearSounds();

    for (const auto& file : noteFiles)
    {
        auto noteNumber = file.getFileNameWithoutExtension().fromLastOccurrenceOf("_", false, false).getIntValue();
        if (! juce::isPositiveAndBelow(noteNumber, 128))
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr)
            continue;

        juce::BigInteger noteRange;
        noteRange.setBit(noteNumber);

        // Every note's file was already pitch-corrected offline to exactly this note, so its
        // root note equals the note it plays and its range covers only that one note -
        // SamplerVoice's own pitch-ratio math then comes out to (practically) 1.0 for it: no
        // real-time pitch shifting, just JUCE's tested voice/polyphony machinery.
        auto* sound = new juce::SamplerSound(juce::String(noteNumber), *reader, noteRange, noteNumber,
                                             0.0, 0.05, 10.0);
        sound->setEnvelopeParameters(cachedEnvelope);
        synth.addSound(sound);
    }

    layerPackFolders[(size_t) layerIndex] = packFolder;

    if (synth.getNumSounds() == 0)
    {
        errorMessage = "No valid Note_NNN.wav files could be loaded from that folder.";
        return false;
    }

    return true;
}

juce::File CreationStationSamplePlayerProcessor::getLayerPackFolder(int layerIndex) const
{
    if (! juce::isPositiveAndBelow(layerIndex, numLayers))
        return {};

    return layerPackFolders[(size_t) layerIndex];
}

int CreationStationSamplePlayerProcessor::getLayerNoteCount(int layerIndex) const
{
    if (! juce::isPositiveAndBelow(layerIndex, numLayers))
        return 0;

    return layerSynths[(size_t) layerIndex].getNumSounds();
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cs::plugins::CreationStationSamplePlayerProcessor();
}
