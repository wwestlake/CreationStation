#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

namespace cs::plugins
{
class CreationStationTuneProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int displayHistorySize = 240;

    enum class Mode
    {
        tuner = 0,
        automatic = 1,
        manual = 2
    };

    enum class Scale
    {
        chromatic = 0,
        major,
        minor,
        dorian,
        phrygian,
        lydian,
        mixolydian,
        locrian,
        arabian,
        egyptian
    };

    CreationStationTuneProcessor();
    ~CreationStationTuneProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Creation Station Tune"; }
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

    double getDetectedMidi() const noexcept { return detectedMidiDisplay.load(); }
    double getTargetMidi() const noexcept { return targetMidiDisplay.load(); }
    float getDetectedCents() const noexcept { return detectedCentsDisplay.load(); }
    float getAppliedCorrectionCents() const noexcept { return appliedCorrectionCentsDisplay.load(); }
    float getDetectionConfidence() const noexcept { return detectionConfidenceDisplay.load(); }
    int getCurrentModeIndex() const noexcept { return currentModeDisplay.load(); }
    void copyPitchHistory(std::array<float, displayHistorySize>& destination, int& validPoints) const;

    static juce::String noteNameForMidi(double midiValue);

private:
    class DelayPitchShifter
    {
    public:
        void prepare(double sampleRate, int maxBlockSize, int numChannels);
        void reset();
        void setSmoothingTime(double sampleRate, double seconds);
        void setTargetRatio(double newRatio);
        void processBlock(juce::AudioBuffer<float>& buffer);

    private:
        float readSample(int channel, double delaySamples) const;

        juce::AudioBuffer<float> delayBuffer;
        juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> ratio;
        double lastSmoothingSeconds = -1.0;
        int writePosition = 0;
        int delayBufferSize = 0;
        int windowSize = 1024;
        int baseDelay = 1024;
        double phase = 0.0;
    };

    void analysePitchWindow();
    void pushDetectorSample(float sample) noexcept;
    double findNearestAllowedMidi(double detectedMidi, int rootNote, Scale scale) const;
    bool isPitchClassAllowed(int pitchClass, int rootNote, Scale scale) const;
    void pushHistoryPoint(float centsToTarget);

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* referenceParam = nullptr;
    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* speedParam = nullptr;
    std::atomic<float>* strengthParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* octaveProtectParam = nullptr;
    std::array<std::atomic<float>*, 12> allowedNoteParams {};

    DelayPitchShifter shifter;
    juce::AudioBuffer<float> dryBuffer;
    std::array<float, 4096> detectorWindow {};
    int detectorWritePosition = 0;
    bool detectorWrapped = false;
    int samplesSinceLastAnalysis = 0;
    double currentSampleRate = 44100.0;

    std::atomic<double> detectedMidiDisplay { 69.0 };
    std::atomic<double> targetMidiDisplay { 69.0 };
    std::atomic<float> detectedCentsDisplay { 0.0f };
    std::atomic<float> appliedCorrectionCentsDisplay { 0.0f };
    std::atomic<float> detectionConfidenceDisplay { 0.0f };
    std::atomic<int> currentModeDisplay { 0 };

    mutable juce::SpinLock historyLock;
    std::array<float, displayHistorySize> pitchHistory {};
    int historyWritePosition = 0;
    int historyCount = 0;

    double lastStableMidi = 69.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreationStationTuneProcessor)
};
}
