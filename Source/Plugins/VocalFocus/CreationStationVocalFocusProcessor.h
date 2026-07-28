#pragma once

#include <JuceHeader.h>
#include <atomic>

namespace cs::plugins
{
class CreationStationVocalFocusProcessor final : public juce::AudioProcessor
{
public:
    enum class Mode
    {
        reduceCenter = 0,
        isolateCenter = 1
    };

    CreationStationVocalFocusProcessor();
    ~CreationStationVocalFocusProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Creation Station Vocal Focus"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    float processFocusedBand(float sample);
    void updateFilterCoefficients(float lowCutHz, float highCutHz);

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* lowCutParam = nullptr;
    std::atomic<float>* highCutParam = nullptr;
    std::atomic<float>* bleedParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;

    juce::IIRFilter highPassFilter;
    juce::IIRFilter lowPassFilter;
    double currentSampleRate = 44100.0;
    float lastLowCutHz = -1.0f;
    float lastHighCutHz = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationVocalFocusProcessor)
};
}
