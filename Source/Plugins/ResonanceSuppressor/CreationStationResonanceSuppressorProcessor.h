#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

namespace cs::plugins
{
class CreationStationResonanceSuppressorProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 4;
    static constexpr int analyzerFftOrder = 11;
    static constexpr int analyzerFftSize = 1 << analyzerFftOrder;

    CreationStationResonanceSuppressorProcessor();
    ~CreationStationResonanceSuppressorProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Creation Station Resonance Suppressor"; }
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

    static juce::String freqParamId(int band)      { return "band" + juce::String(band) + "Freq"; }
    static juce::String depthParamId(int band)     { return "band" + juce::String(band) + "Depth"; }
    static juce::String thresholdParamId(int band) { return "band" + juce::String(band) + "Threshold"; }
    static juce::String qParamId(int band)         { return "band" + juce::String(band) + "Q"; }
    static juce::String bypassParamId(int band)    { return "band" + juce::String(band) + "Bypass"; }

    juce::dsp::IIR::Coefficients<float>::Ptr getBandCoefficients(int band) const;
    double getBandFrequency(int band) const;
    float getBandDepthDb(int band) const;
    float getBandDynamicGainDb(int band) const;
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    bool copyAnalyzerBuffer(juce::AudioBuffer<float>& destination) const;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateDetectorFilter(int band);
    void updateBandCoefficients(int band);
    void updateBallistics();
    void pushAnalyzerSamples(const juce::AudioBuffer<float>& buffer);

    using FilterDuplicator = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                            juce::dsp::IIR::Coefficients<float>>;

    struct BandRuntime
    {
        FilterDuplicator filter;
        juce::dsp::StateVariableTPTFilter<float> detector;
        juce::dsp::IIR::Coefficients<float>::Ptr coefficients;
        float currentDynamicGainDb = 0.0f;
        float detectorEnvelopeDb = -100.0f;
        float attackAlpha = 0.0f;
        float releaseAlpha = 0.0f;
        float lastFreq = -1.0f;
        float lastQ = -1.0f;
    };

    std::array<BandRuntime, (size_t) numBands> bands;

    double currentSampleRate = 44100.0;
    float currentAttackMs = -1.0f;
    float currentReleaseMs = -1.0f;
    mutable juce::SpinLock analyzerLock;
    juce::AudioBuffer<float> analyzerBuffer;
    int analyzerWritePosition = 0;
    bool analyzerBufferWrapped = false;
    juce::dsp::DryWetMixer<float> dryWetMixer;
    juce::dsp::Gain<float> outputGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationResonanceSuppressorProcessor)
};
}
