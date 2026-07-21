#include "CreationStationInstrumentProcessor.h"

#include <cmath>

namespace
{
float getNumericProperty(const juce::NamedValueSet& properties, const juce::Identifier& key, float fallback)
{
    return (float) properties.getWithDefault(key, fallback);
}
}

class CreationStationInstrumentProcessor::Editor final : public juce::AudioProcessorEditor
{
public:
    explicit Editor(CreationStationInstrumentProcessor& processorToEdit)
        : juce::AudioProcessorEditor(processorToEdit), processor(processorToEdit)
    {
        setSize(560, 240);

        titleLabel.setText("Creation Station Instrument", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(24.0f).boldened());
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(titleLabel);

        subtitleLabel.setText("Load an exported instrument patch and play it from MIDI.", juce::dontSendNotification);
        subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));
        addAndMakeVisible(subtitleLabel);

        patchLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd7e4ff));
        addAndMakeVisible(patchLabel);

        pathLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7f91aa));
        addAndMakeVisible(pathLabel);

        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8edb8a));
        addAndMakeVisible(statusLabel);

        loadButton.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser>("Load Creation Station patch", juce::File{}, "*.cspatch");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                 [safeThis = juce::Component::SafePointer<Editor>(this)](const juce::FileChooser& fileChooser)
                                 {
                                     if (safeThis == nullptr)
                                         return;

                                     auto file = fileChooser.getResult();
                                     safeThis->chooser.reset();

                                     if (! file.existsAsFile())
                                         return;

                                     juce::String errorMessage;
                                     if (! safeThis->processor.loadPatchFile(file, errorMessage))
                                     {
                                         safeThis->statusLabel.setText(errorMessage, juce::dontSendNotification);
                                         return;
                                     }

                                     safeThis->refreshFromProcessor();
                                 });
        };
        addAndMakeVisible(loadButton);

        auditionButton.onClick = [this]
        {
            processor.triggerAudition();
            refreshFromProcessor();
        };
        addAndMakeVisible(auditionButton);

        refreshFromProcessor();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff11151c));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(18);
        titleLabel.setBounds(area.removeFromTop(34));
        subtitleLabel.setBounds(area.removeFromTop(22));
        area.removeFromTop(12);

        auto infoArea = area.removeFromTop(96);
        patchLabel.setBounds(infoArea.removeFromTop(28));
        pathLabel.setBounds(infoArea.removeFromTop(22));
        statusLabel.setBounds(infoArea.removeFromTop(22));

        area.removeFromTop(10);
        auto buttonRow = area.removeFromTop(34);
        loadButton.setBounds(buttonRow.removeFromLeft(140));
        buttonRow.removeFromLeft(10);
        auditionButton.setBounds(buttonRow.removeFromLeft(140));
    }

private:
    void refreshFromProcessor()
    {
        patchLabel.setText(processor.getLoadedPatchName(), juce::dontSendNotification);
        pathLabel.setText(processor.getLoadedPatchPath(), juce::dontSendNotification);
        statusLabel.setText(processor.getPatchStatusText(), juce::dontSendNotification);
    }

    CreationStationInstrumentProcessor& processor;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label patchLabel;
    juce::Label pathLabel;
    juce::Label statusLabel;
    juce::TextButton loadButton { "Load Patch" };
    juce::TextButton auditionButton { "Audition" };
    std::unique_ptr<juce::FileChooser> chooser;
};

CreationStationInstrumentProcessor::CreationStationInstrumentProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    juce::String errorMessage;
    loadPatchJson(R"({
        "schemaVersion":"1.0",
        "patchId":"factory-init",
        "name":"Factory Init Tone",
        "type":"instrument",
        "author":"LagDaemon",
        "description":"Default instrument patch.",
        "createdAt":"2026-07-15T00:00:00Z",
        "updatedAt":"2026-07-15T00:00:00Z",
        "runtime":"creation-station",
        "minimumVersion":"0.2.0",
        "parameters":[
            {"id":"baseFrequency","name":"Base Frequency","kind":"float","defaultValue":220.0,"minValue":20.0,"maxValue":2000.0,"unit":"Hz"},
            {"id":"brightness","name":"Brightness","kind":"float","defaultValue":0.72,"minValue":0.0,"maxValue":1.0,"unit":"normalized"}
        ],
        "automationLanes":[],
        "sources":[
            {"id":"osc-a","kind":"oscillator","waveform":"sine","noiseType":"","level":0.75,"frequencyParameter":"baseFrequency"},
            {"id":"osc-b","kind":"oscillator","waveform":"triangle","noiseType":"","level":0.25,"frequencyParameter":"baseFrequency"}
        ],
        "nodes":[{"id":"env-main","kind":"envelope","properties":{"attackPosition":0.08,"sustainPosition":0.35,"releasePosition":0.82,"sustainLevel":0.62}}],
        "connections":[],
        "output":{"channelMode":"stereo","gain":0.8,"pan":0.0}
    })", errorMessage);
    juce::ignoreUnused(errorMessage);
}

void CreationStationInstrumentProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = juce::jmax(1.0, sampleRate);
    sampleCounter = 0;
    resetVoice();
    auditionSamplesRemaining = 0;
    auditionGapSamplesRemaining = 0;
}

void CreationStationInstrumentProcessor::releaseResources()
{
}

bool CreationStationInstrumentProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

void CreationStationInstrumentProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    handleMidi(midi);
    midi.clear();

    buffer.clear();

    auto numChannels = buffer.getNumChannels();
    auto numSamples = buffer.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (auditionGapSamplesRemaining > 0)
        {
            --auditionGapSamplesRemaining;
            if (auditionGapSamplesRemaining == 0)
                startNextAuditionNote();
        }

        if (auditionSamplesRemaining > 0)
        {
            --auditionSamplesRemaining;
            if (auditionSamplesRemaining == 0)
            {
                for (auto& voice : voices)
                    if (voice.active && voice.noteHeld)
                        voice.noteHeld = false;

                if (auditionActive)
                    auditionGapSamplesRemaining = juce::roundToInt(0.04 * currentSampleRate);
            }
        }

        float value = 0.0f;
        for (auto& voice : voices)
            if (voice.active)
                value += renderSampleForVoice(voice);

        auto left = value * juce::jlimit(0.0f, 1.0f, 1.0f - juce::jmax(0.0f, pan));
        auto right = value * juce::jlimit(0.0f, 1.0f, 1.0f + juce::jmin(0.0f, pan));

        if (numChannels > 0)
            buffer.addSample(0, sample, left);
        if (numChannels > 1)
            buffer.addSample(1, sample, right);

        ++sampleCounter;
    }
}

juce::AudioProcessorEditor* CreationStationInstrumentProcessor::createEditor()
{
    return new Editor(*this);
}

void CreationStationInstrumentProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree state("CreationStationInstrument");
    {
        juce::ScopedLock lock(patchLock);
        state.setProperty("patchJson", loadedPatchJson, nullptr);
        state.setProperty("patchPath", loadedPatchPath, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void CreationStationInstrumentProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml(*xml);
    if (! state.isValid())
        return;

    juce::String errorMessage;
    auto patchJson = state.getProperty("patchJson").toString();
    if (patchJson.isNotEmpty())
        loadPatchJson(patchJson, errorMessage);

    auto restoredPath = state.getProperty("patchPath").toString();
    if (restoredPath.isNotEmpty())
    {
        juce::ScopedLock lock(patchLock);
        loadedPatchPath = restoredPath;
    }
}

bool CreationStationInstrumentProcessor::loadPatchFile(const juce::File& file, juce::String& errorMessage)
{
    if (! file.existsAsFile())
    {
        errorMessage = "The selected patch file does not exist.";
        return false;
    }

    auto jsonText = file.loadFileAsString();
    if (! loadPatchJson(jsonText, errorMessage))
        return false;

    juce::ScopedLock lock(patchLock);
    loadedPatchPath = file.getFullPathName();
    return true;
}

bool CreationStationInstrumentProcessor::loadPatchJson(const juce::String& jsonText, juce::String& errorMessage)
{
    cw::PatchDocument patch;
    if (! cw::parsePatchDocumentJson(jsonText, patch, errorMessage))
        return false;

    if (patch.type != "instrument")
    {
        errorMessage = "This plugin loads instrument patches only.";
        return false;
    }

    {
        juce::ScopedLock lock(patchLock);
        loadedPatch = patch;
        loadedPatchJson = jsonText;
        if (loadedPatchPath.isEmpty())
            loadedPatchPath = "Embedded patch";
        updatePatchDerivedState();
    }

    return true;
}

void CreationStationInstrumentProcessor::triggerAudition()
{
    auditionNotes.clearQuick();
    auditionNotes.addArray({ 64, 62, 60, 62, 64, 64, 64,
                             62, 62, 62, 64, 67, 67,
                             64, 62, 60, 62, 64, 64, 64, 64,
                             62, 62, 64, 62, 60 });
    auditionIndex = 0;
    auditionActive = true;
    auditionSamplesRemaining = 0;
    auditionGapSamplesRemaining = 0;
    startNextAuditionNote();
}

juce::String CreationStationInstrumentProcessor::getLoadedPatchName() const
{
    juce::ScopedLock lock(patchLock);
    return loadedPatch.name.isNotEmpty() ? loadedPatch.name : "No patch loaded";
}

juce::String CreationStationInstrumentProcessor::getLoadedPatchPath() const
{
    juce::ScopedLock lock(patchLock);
    return loadedPatchPath;
}

juce::String CreationStationInstrumentProcessor::getPatchStatusText() const
{
    juce::ScopedLock lock(patchLock);
    if (loadedPatch.name.isEmpty())
        return "No patch loaded.";

    return "Loaded instrument patch. Play MIDI notes to audition it.";
}

void CreationStationInstrumentProcessor::resetVoice()
{
    for (auto& voice : voices)
        voice = {};
}

void CreationStationInstrumentProcessor::handleMidi(const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto& message = metadata.getMessage();
        if (message.isNoteOn())
        {
            auto* chosenVoice = &voices[0];
            for (auto& voice : voices)
            {
                if (! voice.active || voice.envelopeLevel <= 0.0f)
                {
                    chosenVoice = &voice;
                    break;
                }
            }

            chosenVoice->midiNote = message.getNoteNumber();
            chosenVoice->noteHeld = true;
            chosenVoice->active = true;
            chosenVoice->envelopeLevel = 0.0f;
            chosenVoice->phase = 0.0;
            chosenVoice->velocity = juce::jlimit(0.05f, 1.0f, (float) message.getVelocity());
            chosenVoice->ageSamples = 0;
            chosenVoice->releaseStartLevel = 0.0f;
            chosenVoice->filterState = 0.0f;
            auditionActive = false;
        }
        else if (message.isNoteOff())
        {
            for (auto& voice : voices)
                if (voice.active && voice.midiNote == message.getNoteNumber())
                {
                    voice.noteHeld = false;
                    voice.releaseStartLevel = voice.envelopeLevel;
                }
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            for (auto& voice : voices)
            {
                voice.noteHeld = false;
                voice.releaseStartLevel = voice.envelopeLevel;
            }
        }
    }
}

float CreationStationInstrumentProcessor::renderSampleForVoice(VoiceState& voice)
{
    if (voice.midiNote < 0)
        return 0.0f;

    cw::PatchDocument patchCopy;
    {
        juce::ScopedLock lock(patchLock);
        patchCopy = loadedPatch;
    }

    auto frequency = juce::MidiMessage::getMidiNoteInHertz(voice.midiNote);
    auto phaseDelta = juce::MathConstants<double>::twoPi * frequency / currentSampleRate;
    voice.phase += phaseDelta;
    if (voice.phase >= juce::MathConstants<double>::twoPi)
        voice.phase = std::fmod(voice.phase, juce::MathConstants<double>::twoPi);

    if (voice.noteHeld)
    {
        auto attackSamples = juce::jmax(1, juce::roundToInt(attackSeconds * currentSampleRate));
        auto decaySamples = juce::jmax(1, juce::roundToInt(decaySeconds * currentSampleRate));

        if (voice.ageSamples < attackSamples)
            voice.envelopeLevel = juce::jmap((float) voice.ageSamples / (float) attackSamples, 0.0f, 1.0f);
        else if (voice.ageSamples < attackSamples + decaySamples)
            voice.envelopeLevel = juce::jmap((float) (voice.ageSamples - attackSamples) / (float) decaySamples, 1.0f, sustainLevel);
        else
            voice.envelopeLevel = sustainLevel;
    }
    else
    {
        auto releaseStep = releaseSeconds <= 0.0f ? 1.0f : 1.0f / (float) (releaseSeconds * currentSampleRate);
        voice.envelopeLevel = juce::jmax(0.0f, voice.envelopeLevel - juce::jmax(releaseStep, voice.releaseStartLevel * releaseStep));
        if (voice.envelopeLevel <= 0.0f)
        {
            voice.active = false;
            voice.midiNote = -1;
            voice.velocity = 1.0f;
            voice.ageSamples = 0;
            voice.releaseStartLevel = 0.0f;
            voice.filterState = 0.0f;
            return 0.0f;
        }
    }

    float mixed = 0.0f;
    float totalLevel = 0.0f;
    for (const auto& source : patchCopy.sources)
    {
        mixed += renderSourceSample(source, voice.phase, sampleCounter) * (float) source.level;
        totalLevel += (float) source.level;
    }

    auto normalizedBrightness = juce::jlimit(0.02f, 1.0f, brightness);
    auto cutoffHz = juce::jmap(normalizedBrightness, 180.0f, 12000.0f);
    auto filterAlpha = juce::jlimit(0.001f, 0.99f, (float) (juce::MathConstants<double>::twoPi * cutoffHz / currentSampleRate));
    auto filtered = voice.filterState + filterAlpha * (mixed - voice.filterState);
    voice.filterState = filtered;

    ++voice.ageSamples;
    auto normalizer = totalLevel > 0.0f ? (1.0f / totalLevel) : 0.0f;
    return filtered * normalizer * outputGain * voice.envelopeLevel * voice.velocity;
}

float CreationStationInstrumentProcessor::renderSourceSample(const cw::PatchSource& source, double phase, int sampleIndex) const
{
    if (source.kind == "oscillator")
    {
        if (source.waveform == "sine")
            return (float) std::sin(phase);
        if (source.waveform == "saw")
        {
            auto wrapped = (float) (phase / juce::MathConstants<double>::twoPi);
            return 2.0f * (wrapped - std::floor(wrapped + 0.5f));
        }
        if (source.waveform == "square")
            return std::sin(phase) >= 0.0 ? 1.0f : -1.0f;
        if (source.waveform == "triangle")
            return std::asin(std::sin(phase)) * (2.0f / juce::MathConstants<float>::pi);
    }
    else if (source.kind == "noise")
    {
        auto hashed = std::sin((float) sampleIndex * 12.9898f + 78.233f) * 43758.5453f;
        return 2.0f * (hashed - std::floor(hashed)) - 1.0f;
    }

    return 0.0f;
}

void CreationStationInstrumentProcessor::updatePatchDerivedState()
{
    outputGain = (float) loadedPatch.output.gain;
    pan = (float) loadedPatch.output.pan;
    brightness = 0.72f;

    attackPosition = 0.12f;
    sustainPosition = 0.42f;
    releasePosition = 0.82f;
    sustainLevel = 0.48f;

    for (const auto& parameter : loadedPatch.parameters)
    {
        if (parameter.id == "brightness")
            brightness = (float) parameter.defaultValue;
    }

    for (const auto& node : loadedPatch.nodes)
    {
        if (node.kind == "envelope")
        {
            attackPosition = getNumericProperty(node.properties, "attackPosition", attackPosition);
            sustainPosition = getNumericProperty(node.properties, "sustainPosition", sustainPosition);
            releasePosition = getNumericProperty(node.properties, "releasePosition", releasePosition);
            sustainLevel = getNumericProperty(node.properties, "sustainLevel", sustainLevel);
            break;
        }
    }

    auto safeAttack = juce::jlimit(0.01f, 0.95f, attackPosition);
    auto safeSustain = juce::jlimit(safeAttack + 0.02f, 0.98f, sustainPosition);
    auto safeRelease = juce::jlimit(safeAttack + 0.05f, 1.0f, juce::jmax(releasePosition, sustainPosition + 0.05f));
    attackSeconds = juce::jmap(safeAttack, 0.005f, 0.4f);
    decaySeconds = juce::jmap(juce::jmax(0.02f, safeSustain - safeAttack), 0.02f, 0.35f);
    releaseSeconds = juce::jmap(1.0f - safeRelease, 0.05f, 0.6f);
}

bool CreationStationInstrumentProcessor::startNextAuditionNote()
{
    if (auditionIndex >= auditionNotes.size())
    {
        auditionActive = false;
        return false;
    }

        auto& voice = voices[0];
        voice.midiNote = auditionNotes[auditionIndex++];
        voice.noteHeld = true;
        voice.active = true;
        voice.envelopeLevel = 0.0f;
        voice.phase = 0.0;
        voice.velocity = 0.85f;
        voice.ageSamples = 0;
        voice.releaseStartLevel = 0.0f;
        voice.filterState = 0.0f;
        auditionSamplesRemaining = juce::roundToInt(0.28 * currentSampleRate);
        return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CreationStationInstrumentProcessor();
}
