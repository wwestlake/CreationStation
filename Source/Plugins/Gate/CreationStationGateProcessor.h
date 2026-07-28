#pragma once

#include <JuceHeader.h>
#include <atomic>

namespace cs::plugins
{
// A noise gate / expander. juce::dsp::NoiseGate provides both behaviors from the same
// threshold/ratio/attack/release controls - a high ratio acts as a hard gate, a low ratio acts as
// a gentle downward expander - so one plugin covers both roadmap entries.
class CreationStationGateProcessor final : public juce::AudioProcessor
{
public:
    CreationStationGateProcessor();
    ~CreationStationGateProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }
    const std::atomic<float>& getGainReductionValue() const noexcept { return currentGainReductionDb; }
    const std::atomic<float>& getInputLevelValue() const noexcept { return currentInputLevelDb; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* ratioParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;

    juce::dsp::NoiseGate<float> gate;

    std::atomic<float> currentGainReductionDb { 0.0f };
    std::atomic<float> currentInputLevelDb { -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationGateProcessor)
};
}
