#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace cs::plugins
{
// Algorithmic reverb built on juce::dsp::Reverb (a FreeVerb-style comb/allpass design) - the same
// "reuse JUCE's proven dsp widget" approach as Compressor (juce::dsp::Compressor) and Gate
// (juce::dsp::NoiseGate), rather than writing a convolution/algorithmic reverb from scratch.
class CreationStationReverbProcessor final : public juce::AudioProcessor
{
public:
    CreationStationReverbProcessor();
    ~CreationStationReverbProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }
    const std::atomic<float>& getInputLevelValue() const noexcept { return currentInputLevelDb; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* sizeParam = nullptr;
    std::atomic<float>* dampingParam = nullptr;
    std::atomic<float>* widthParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* freezeParam = nullptr;

    juce::dsp::Reverb reverb;

    std::atomic<float> currentInputLevelDb { -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationReverbProcessor)
};
}
