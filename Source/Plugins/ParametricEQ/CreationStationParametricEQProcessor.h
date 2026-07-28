#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

namespace cs::plugins
{
// A 4-band fully flexible parametric EQ - every band can independently be Bell/Peak, Low Shelf,
// High Shelf, Low-Pass, or High-Pass, rather than fixed to "band 1 = low shelf" etc. Built on
// juce::dsp::IIR::Filter/Coefficients, same DSP module already proven in the Compressor/Limiter
// plugins, chained in series via juce::dsp::ProcessorDuplicator for stereo.
class CreationStationParametricEQProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 4;
    static constexpr int analyzerFftOrder = 11;
    static constexpr int analyzerFftSize = 1 << analyzerFftOrder;

    enum class BandType { bell = 0, lowShelf = 1, highShelf = 2, lowPass = 3, highPass = 4 };

    CreationStationParametricEQProcessor();
    ~CreationStationParametricEQProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Creation Station Parametric EQ"; }
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

    static juce::String typeParamId(int band) { return "band" + juce::String(band) + "Type"; }
    static juce::String freqParamId(int band) { return "band" + juce::String(band) + "Freq"; }
    static juce::String gainParamId(int band) { return "band" + juce::String(band) + "Gain"; }
    static juce::String qParamId(int band) { return "band" + juce::String(band) + "Q"; }
    static juce::String bypassParamId(int band) { return "band" + juce::String(band) + "Bypass"; }

    // For the editor's EqCurveComponent: current coefficients (null if that band is bypassed) and
    // the raw parameter values used to position its draggable point.
    juce::dsp::IIR::Coefficients<float>::Ptr getBandCoefficients(int band) const;
    double getBandFrequency(int band) const;
    float getBandGainDb(int band) const;
    BandType getBandType(int band) const;
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }
    bool copyAnalyzerBuffer(juce::AudioBuffer<float>& destination) const;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Rebuilds band's coefficients from its current parameter values, but only if something
    // actually changed since the last rebuild - avoids reallocating a Coefficients object every
    // single block for a band nobody is touching.
    void updateBandCoefficients(int band);

    using FilterDuplicator = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

    std::array<FilterDuplicator, (size_t) numBands> filters;
    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, (size_t) numBands> currentCoefficients;

    struct CachedBandState
    {
        int type = -1;
        float freq = -1.0f, gain = 0.0f, q = -1.0f;
    };
    std::array<CachedBandState, (size_t) numBands> cachedState;

    // Freq/gain/Q are ramped over a short window rather than jumping straight to the new target -
    // swapping IIR coefficients outright mid-stream (e.g. while dragging the curve, which can fire
    // many parameter changes per second) causes an audible click/crackle each time, since the
    // filter's internal state was built for the old transfer function. Type isn't smoothed since a
    // topology change (Bell -> Low-Pass) has no meaningful in-between value.
    struct BandSmoothers
    {
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freq, gain, q;
    };
    std::array<BandSmoothers, (size_t) numBands> smoothers;

    void pushAnalyzerSamples(const juce::AudioBuffer<float>& buffer);

    double currentSampleRate = 44100.0;
    mutable juce::SpinLock analyzerLock;
    juce::AudioBuffer<float> analyzerBuffer;
    int analyzerWritePosition = 0;
    bool analyzerBufferWrapped = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationParametricEQProcessor)
};
}
