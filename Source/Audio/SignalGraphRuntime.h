#pragma once

#include <JuceHeader.h>

class SignalGraphRuntime final
{
public:
    enum class SourceWaveform
    {
        sine,
        saw,
        square,
        triangle
    };

    SignalGraphRuntime();

    void prepare(double sampleRate, int blockSize);
    void reset();

    void setEnabled(bool shouldEnable) noexcept { enabled.store(shouldEnable); }
    void setSourceLevel(float amount) noexcept { sourceLevel.store(amount); }
    void setSourceFrequency(float hz) noexcept { sourceFrequency.store(hz); }
    void setSourceWaveform(SourceWaveform waveform) noexcept { sourceWaveform.store(static_cast<int>(waveform)); }
    void setDrive(float amount) noexcept { drive.store(amount); }
    void setTone(float amount) noexcept { tone.store(amount); }
    void setEcho(float amount) noexcept { echo.store(amount); }
    void setWidth(float amount) noexcept { width.store(amount); }
    void setMasterGain(float amount) noexcept { masterGain.store(amount); }

    bool loadFileSource(const juce::File& file, juce::String& errorMessage);
    void clearFileSource();
    void setFileSourceEnabled(bool shouldEnable) noexcept { fileSourceEnabled.store(shouldEnable); }
    bool hasFileSource() const noexcept { return fileReaderSource != nullptr; }

    void render(juce::AudioBuffer<float>& buffer);

private:
    float renderSourceSample();
    void applyDriveAndTone(juce::AudioBuffer<float>& buffer);
    void applyDelayAndWidth(juce::AudioBuffer<float>& buffer);

    std::atomic<bool> enabled { true };
    std::atomic<float> sourceLevel { 0.25f };
    std::atomic<float> sourceFrequency { 220.0f };
    std::atomic<int> sourceWaveform { 0 };
    std::atomic<float> drive { 0.15f };
    std::atomic<float> tone { 0.55f };
    std::atomic<float> echo { 0.08f };
    std::atomic<float> width { 0.5f };
    std::atomic<float> masterGain { 0.8f };
    std::atomic<bool> fileSourceEnabled { false };

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> fileReaderSource;
    juce::AudioTransportSource fileTransport;
    juce::AudioBuffer<float> fileRenderBuffer;

    double sampleRate = 44100.0;
    int blockSize = 512;
    double oscillatorPhase = 0.0;
    std::array<float, 2> lowPassState {};
    std::array<float, 4410> echoHistoryLeft {};
    std::array<float, 4410> echoHistoryRight {};
    int echoWritePosition = 0;
};
