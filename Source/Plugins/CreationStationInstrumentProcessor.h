#pragma once

#include <JuceHeader.h>
#include "../Patch/PatchModel.h"

class CreationStationInstrumentProcessor final : public juce::AudioProcessor
{
public:
    CreationStationInstrumentProcessor();
    ~CreationStationInstrumentProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Creation Station Instrument"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool loadPatchFile(const juce::File& file, juce::String& errorMessage);
    bool loadPatchJson(const juce::String& jsonText, juce::String& errorMessage);
    void triggerAudition();

    juce::String getLoadedPatchName() const;
    juce::String getLoadedPatchPath() const;
    juce::String getPatchStatusText() const;

private:
    struct VoiceState
    {
        int midiNote = -1;
        bool noteHeld = false;
        bool active = false;
        double phase = 0.0;
        float envelopeLevel = 0.0f;
        float velocity = 1.0f;
        int ageSamples = 0;
        float releaseStartLevel = 0.0f;
        float filterState = 0.0f;
    };

    class Editor;

    void resetVoice();
    void handleMidi(const juce::MidiBuffer& midi);
    float renderSampleForVoice(VoiceState& voiceToRender);
    float renderSourceSample(const cw::PatchSource& source, double phase, int sampleIndex) const;
    void updatePatchDerivedState();
    bool startNextAuditionNote();

    juce::CriticalSection patchLock;
    cw::PatchDocument loadedPatch;
    juce::String loadedPatchJson;
    juce::String loadedPatchPath;

    double currentSampleRate = 48000.0;
    int sampleCounter = 0;
    std::array<VoiceState, 8> voices;
    juce::Array<int> auditionNotes;
    int auditionIndex = 0;
    int auditionSamplesRemaining = 0;
    int auditionGapSamplesRemaining = 0;
    bool auditionActive = false;

    float outputGain = 0.9f;
    float brightness = 0.72f;
    float attackPosition = 0.12f;
    float sustainPosition = 0.42f;
    float releasePosition = 0.82f;
    float sustainLevel = 0.48f;
    float attackSeconds = 0.02f;
    float decaySeconds = 0.08f;
    float releaseSeconds = 0.2f;
    float pan = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationInstrumentProcessor)
};
