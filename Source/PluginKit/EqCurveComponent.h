#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <functional>
#include <vector>

namespace cs::plugins
{
// Draws an EQ's combined frequency-response curve from its bands' current filter coefficients,
// with a draggable point per band (drag horizontally = frequency, vertically = gain) so the
// curve itself is the primary editing surface. It can also render a live spectrum analyzer
// behind the response curve so multiple EQ plugins share one visual language.
class EqCurveComponent final : public juce::Component
{
public:
    struct Band
    {
        // Null (or a bypassed band) is simply skipped when computing the combined curve.
        juce::dsp::IIR::Coefficients<float>::Ptr coefficients;
        double frequencyHz = 1000.0; // positions this band's draggable point
        float gainDb = 0.0f;         // positions this band's draggable point
        bool hasGainAxis = true;     // false for Low-Pass/High-Pass - only frequency is draggable
    };

    void setSampleRate(double newSampleRate);
    void setBands(std::vector<Band> newBands);
    void updateAnalyzer(const juce::AudioBuffer<float>& buffer);

    // Fired while dragging a point: (bandIndex, newFrequencyHz, newGainDb).
    std::function<void(int, double, float)> onBandDragged;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    // The full range any band's Gain parameter allows - dragging must always be able to reach
    // this, even when the display below is auto-zoomed tighter than it for visibility.
    static constexpr float maxDisplayableDb = 24.0f;
    static constexpr double minFrequency = 20.0;
    static constexpr double maxFrequency = 20000.0;

    int frequencyToX(double frequencyHz) const;
    double xToFrequency(int x) const;
    int gainToY(float gainDb) const;
    float yToGain(int y) const;
    int hitTestBand(juce::Point<int> position) const;
    void updateDisplayRange();

    std::vector<Band> bands;
    double sampleRate = 44100.0;
    int draggingBandIndex = -1;
    juce::dsp::FFT fft { 11 };
    std::array<float, 1 << 12> fftData {};
    juce::Array<float> analyzerMagnitudes;

    // Recomputed each time the bands change (updateDisplayRange()) so a subtle few-dB move is
    // still clearly visible instead of a barely-there wiggle inside a fixed +/-24dB box, while a
    // dramatic move still shows at full scale.
    float minDb = -6.0f;
    float maxDb = 6.0f;
};
}
