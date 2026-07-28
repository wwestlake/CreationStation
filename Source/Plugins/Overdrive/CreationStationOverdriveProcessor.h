#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace cs::plugins
{
// Drive-into-waveshaper saturation/overdrive. The nonlinear stage runs 2x oversampled to keep
// aliasing down, matching standard practice for any real-time waveshaper. Three curve types
// (Soft/Hard/Tube) cover the "Overdrive/Saturation" roadmap entry as one flexible plugin rather
// than three separate ones.
class CreationStationOverdriveProcessor final : public juce::AudioProcessor
{
public:
    enum class CurveType { soft = 0, hard = 1, tube = 2 };

    CreationStationOverdriveProcessor();
    ~CreationStationOverdriveProcessor() override;

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
    const std::atomic<float>& getInputLevelValue() const noexcept { return currentInputLevelDb; }
    const std::atomic<float>& getOutputLevelValue() const noexcept { return currentOutputLevelDb; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void updateWaveshaperFunction(int typeIndex);
    void updateToneCoefficients(float toneHz);

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* driveParam = nullptr;
    std::atomic<float>* toneParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* typeParam = nullptr;

    juce::dsp::Gain<float> driveGain;
    juce::dsp::Gain<float> outputGain;
    juce::dsp::DryWetMixer<float> dryWetMixer;
    juce::dsp::WaveShaper<float> waveshaper;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> toneFilter;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    int lastCurveType = -1;
    float lastToneHz = -1.0f;
    double currentSampleRate = 44100.0;

    // Ramped rather than jumping straight to the new target - swapping the tone filter's
    // coefficients outright mid-stream causes an audible click each time, same issue found (and
    // fixed the same way) in the Parametric EQ.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> toneSmoother;

    std::atomic<float> currentInputLevelDb { -100.0f };
    std::atomic<float> currentOutputLevelDb { -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationOverdriveProcessor)
};
}
