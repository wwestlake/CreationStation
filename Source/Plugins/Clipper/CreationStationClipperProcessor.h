#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>

namespace cs::plugins
{
class CreationStationClipperProcessor final : public juce::AudioProcessor
{
public:
    CreationStationClipperProcessor();
    ~CreationStationClipperProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Creation Station Clipper"; }
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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }

    std::atomic<float>& getInputLevelValue() noexcept { return currentInputLevelDb; }
    std::atomic<float>& getOutputLevelValue() noexcept { return currentOutputLevelDb; }
    std::atomic<float>& getClipReductionValue() noexcept { return currentClipReductionDb; }

private:
    enum class CurveType
    {
        soft = 0,
        hard = 1,
        asymmetric = 2
    };

    void updateCurveSettings();

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* typeParam = nullptr;
    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* ceilingParam = nullptr;
    std::atomic<float>* softnessParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* mixParam = nullptr;

    juce::dsp::Gain<float> driveGain;
    juce::dsp::Gain<float> outputGain;
    juce::dsp::DryWetMixer<float> dryWetMixer;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> ceilingSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> softnessSmoother;
    double currentSampleRate = 44100.0;
    int lastCurveType = -1;

    std::atomic<float> currentInputLevelDb { -100.0f };
    std::atomic<float> currentOutputLevelDb { -100.0f };
    std::atomic<float> currentClipReductionDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationClipperProcessor)
};
}
