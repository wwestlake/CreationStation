#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace cs::plugins
{
// A split-band de-esser: the signal is split into a low band and a high ("sibilance") band at a
// user-set frequency, a compressor-style threshold/ratio gain reduction is computed from the high
// band's level and smoothed sample-accurately (avoiding block-rate zipper noise), then applied
// only to the high band before the two bands are summed back together. A "Listen" toggle solos
// the high band so the split frequency can be tuned by ear, standard practice for this effect
// type.
class CreationStationDeEsserProcessor final : public juce::AudioProcessor
{
public:
    CreationStationDeEsserProcessor();
    ~CreationStationDeEsserProcessor() override;

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
    const std::atomic<float>& getReductionValue() const noexcept { return currentReductionDb; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void updateSplitCoefficients(float frequencyHz);

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* frequencyParam = nullptr;
    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* ratioParam = nullptr;
    std::atomic<float>* listenParam = nullptr;

    using FilterDuplicator = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    FilterDuplicator lowFilter;
    FilterDuplicator highFilter;

    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSmoother;

    // Ramped rather than jumping straight to the new target - swapping the split filters'
    // coefficients outright mid-stream causes an audible click each time, same issue found (and
    // fixed the same way) in the Parametric EQ.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> frequencySmoother;

    float lastSplitFrequency = -1.0f;
    double currentSampleRate = 44100.0;
    float displayedReductionDb = 0.0f;

    std::atomic<float> currentReductionDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationDeEsserProcessor)
};
}
