#pragma once

#include <JuceHeader.h>
#include <array>

namespace cs::plugins
{
// A lightweight, host-agnostic instrument that loads pre-built Sample Pack Builder outputs
// (Note_NNN.wav files, one per MIDI note, already pitch-corrected offline) and plays them back
// with zero real-time pitch processing. Built on JUCE's own juce::Synthesiser/SamplerSound/
// SamplerVoice: each loaded note's sample covers a MIDI note range of exactly that one note with
// its root note set to match, so SamplerVoice's own pitch-ratio math comes out to (practically)
// 1.0 for every played note - no hand-rolled lookup/playback/voice-stealing code needed.
//
// Up to numLayers independently-loaded packs can play at once (e.g. different mic/pickup takes
// of the same instrument), each with its own gain, summed into the output - "layers" here means
// separate full packs, not multiple takes of one note (that's resolved once, offline, by the
// Sample Pack Builder). One global ADSR (attack/decay/sustain/release) is pushed identically into
// every loaded SamplerSound across every layer.
class CreationStationSamplePlayerProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int numLayers = 4;
    static constexpr int numVoicesPerLayer = 16;

    CreationStationSamplePlayerProcessor();
    ~CreationStationSamplePlayerProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Creation Station Sample Player"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override { }
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override { }

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Loads a sample pack folder (Note_NNN.wav files) into the given layer slot, replacing
    // whatever was loaded there before. Returns false (with errorMessage set) if nothing usable
    // was found.
    bool loadLayerPack(int layerIndex, const juce::File& packFolder, juce::String& errorMessage);
    juce::File getLayerPackFolder(int layerIndex) const;
    int getLayerNoteCount(int layerIndex) const;
    static juce::String layerGainParamId(int layerIndex) { return "layer" + juce::String(layerIndex) + "Gain"; }

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

    // Output level in dB (for LevelMeterComponent), not raw gain.
    std::atomic<float> outputLevelDb { -60.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Re-pushes the current ADSR parameter values into every SamplerSound in every layer -
    // called after a pack loads and whenever the cached ADSR values change.
    void applyEnvelopeToAllSounds();

    std::array<juce::Synthesiser, (size_t) numLayers> layerSynths;
    std::array<juce::File, (size_t) numLayers> layerPackFolders;
    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> layerScratchBuffer;

    juce::ADSR::Parameters cachedEnvelope;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationSamplePlayerProcessor)
};
}
