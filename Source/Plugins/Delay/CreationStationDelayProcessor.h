#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace cs::plugins
{
// A stereo feedback delay with an optional ping-pong crossfeed and a one-pole low-pass filter in
// the feedback path (so repeats darken over time, like a tape delay's high-frequency loss). The
// feedback loop is inherently per-sample/recursive, so unlike the other core plugins this one
// processes sample-by-sample rather than through a single juce::dsp ProcessContext call.
class CreationStationDelayProcessor final : public juce::AudioProcessor
{
public:
    CreationStationDelayProcessor();
    ~CreationStationDelayProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }

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
    void updateFeedbackFilterCoefficients(float toneHz);

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* timeParam = nullptr;
    std::atomic<float>* feedbackParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* toneParam = nullptr;
    std::atomic<float>* pingPongParam = nullptr;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 1 };
    std::array<juce::dsp::IIR::Filter<float>, 2> feedbackFilters;

    float lastToneHz = -1.0f;
    double currentSampleRate = 44100.0;

    // Ramped rather than jumping straight to the new target - an abrupt jump in the feedback
    // filter's coefficients causes an audible click (same issue found in the Parametric EQ), and
    // an abrupt jump in delay time causes the read head to skip instead of gliding.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> toneSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> timeSmoother;

    std::atomic<float> currentInputLevelDb { -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationDelayProcessor)
};
}
