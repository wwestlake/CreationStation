#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>

namespace cs::plugins
{
class CreationStationLimiterProcessor : public juce::AudioProcessor
{
public:
    CreationStationLimiterProcessor();
    ~CreationStationLimiterProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Creation Station Limiter"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override { }
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override { }

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

    std::atomic<float> inputLevelDb { -60.0f };
    std::atomic<float> gainReductionDb { 0.0f };

private:
    juce::dsp::Limiter<float> limiter;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    float dbToLinear(float db) const;
    float linearToDb(float linear) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationLimiterProcessor)
};
}
