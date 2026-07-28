#include "EqCurveComponent.h"
#include <cmath>

namespace cs::plugins
{
void EqCurveComponent::setSampleRate(double newSampleRate)
{
    sampleRate = newSampleRate;
    repaint();
}

void EqCurveComponent::setBands(std::vector<Band> newBands)
{
    bands = std::move(newBands);
    updateDisplayRange();
    repaint();
}

void EqCurveComponent::updateAnalyzer(const juce::AudioBuffer<float>& buffer)
{
    fftData.fill(0.0f);

    if (buffer.getNumSamples() <= 0)
    {
        analyzerMagnitudes.clear();
        repaint();
        return;
    }

    constexpr int analyzerFftSize = 1 << 11;
    auto samplesToCopy = juce::jmin(buffer.getNumSamples(), analyzerFftSize);
    auto* input = buffer.getReadPointer(0);

    for (int index = 0; index < samplesToCopy; ++index)
    {
        auto window = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * (float) index
                                             / (float) juce::jmax(1, samplesToCopy - 1));
        fftData[(size_t) index] = input[index] * window;
    }

    fft.performFrequencyOnlyForwardTransform(fftData.data());

    auto bins = analyzerFftSize / 2;
    analyzerMagnitudes.resize(bins);
    analyzerMagnitudes.set(0, -100.0f);
    for (int index = 1; index < bins; ++index)
        analyzerMagnitudes.set(index, juce::Decibels::gainToDecibels(fftData[(size_t) index] / (float) analyzerFftSize,
                                                                     -100.0f));

    repaint();
}

void EqCurveComponent::updateDisplayRange()
{
    constexpr int numPoints = 200;
    auto peakAbsDb = 0.0f;

    for (int i = 0; i < numPoints; ++i)
    {
        auto t = (double) i / (double) (numPoints - 1);
        auto freq = minFrequency * std::pow(maxFrequency / minFrequency, t);

        auto magnitude = 1.0;
        for (const auto& band : bands)
            if (band.coefficients != nullptr)
                magnitude *= band.coefficients->getMagnitudeForFrequency(freq, sampleRate);

        auto db = juce::Decibels::gainToDecibels(magnitude, (double) -maxDisplayableDb);
        peakAbsDb = juce::jmax(peakAbsDb, (float) std::abs(db));
    }

    // At least a +/-6dB window (so a dead-flat response doesn't look like a razor-thin sliver),
    // with 30% headroom past the current peak so there's visible room to keep dragging further,
    // capped at the parameter's own +/-24dB ceiling.
    auto halfRange = juce::jlimit(6.0f, maxDisplayableDb, peakAbsDb * 1.3f);
    minDb = -halfRange;
    maxDb = halfRange;
}

int EqCurveComponent::frequencyToX(double frequencyHz) const
{
    auto clamped = juce::jlimit(minFrequency, maxFrequency, frequencyHz);
    auto t = std::log(clamped / minFrequency) / std::log(maxFrequency / minFrequency);
    return juce::roundToInt(t * (double) getWidth());
}

double EqCurveComponent::xToFrequency(int x) const
{
    auto t = juce::jlimit(0.0, 1.0, (double) x / (double) juce::jmax(1, getWidth()));
    return minFrequency * std::pow(maxFrequency / minFrequency, t);
}

int EqCurveComponent::gainToY(float gainDb) const
{
    auto t = (juce::jlimit(minDb, maxDb, gainDb) - minDb) / (maxDb - minDb);
    return juce::roundToInt((double) getHeight() * (1.0 - (double) t));
}

float EqCurveComponent::yToGain(int y) const
{
    auto t = juce::jlimit(0.0, 1.0, 1.0 - (double) y / (double) juce::jmax(1, getHeight()));
    return minDb + (float) t * (maxDb - minDb);
}

int EqCurveComponent::hitTestBand(juce::Point<int> position) const
{
    constexpr float hitRadius = 12.0f;

    for (int i = 0; i < (int) bands.size(); ++i)
    {
        const auto& band = bands[(size_t) i];
        auto x = frequencyToX(band.frequencyHz);
        auto y = band.hasGainAxis ? gainToY(band.gainDb) : gainToY(0.0f);

        if (position.toFloat().getDistanceFrom(juce::Point<float>((float) x, (float) y)) <= hitRadius)
            return i;
    }

    return -1;
}

void EqCurveComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(0xff0d1218));

    if (! analyzerMagnitudes.isEmpty())
    {
        juce::Path analyzerPath;
        constexpr float analyzerTopDb = 12.0f;
        constexpr float analyzerBottomDb = -96.0f;
        auto bins = juce::jmax(2, analyzerMagnitudes.size());
        auto nyquist = juce::jmax(1000.0, sampleRate * 0.5);

        for (int index = 1; index < bins; ++index)
        {
            auto frequency = ((double) index / (double) bins) * nyquist;
            auto db = juce::jlimit(analyzerBottomDb, analyzerTopDb, analyzerMagnitudes[index]);
            auto x = (float) frequencyToX(frequency);
            auto normalized = juce::jmap(db, analyzerBottomDb, analyzerTopDb, 1.0f, 0.0f);
            auto y = juce::jmap(normalized, 0.0f, 1.0f, 0.0f, (float) bounds.getHeight());

            if (index == 1)
                analyzerPath.startNewSubPath(x, y);
            else
                analyzerPath.lineTo(x, y);
        }

        auto filledAnalyzer = analyzerPath;
        filledAnalyzer.lineTo((float) bounds.getRight(), (float) bounds.getBottom());
        filledAnalyzer.lineTo(0.0f, (float) bounds.getBottom());
        filledAnalyzer.closeSubPath();

        g.setColour(juce::Colour(0x1836bffa));
        g.fillPath(filledAnalyzer);
        g.setColour(juce::Colour(0xff48c8ff).withAlpha(0.7f));
        g.strokePath(analyzerPath, juce::PathStrokeType(1.5f, juce::PathStrokeType::JointStyle::curved));
    }

    g.setColour(juce::Colour(0xff1c2634));
    for (float db : { minDb, minDb * 0.5f, 0.0f, maxDb * 0.5f, maxDb })
        g.drawHorizontalLine(gainToY(db), 0.0f, (float) bounds.getWidth());

    for (double freq : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
        g.drawVerticalLine(frequencyToX(freq), 0.0f, (float) bounds.getHeight());

    g.setColour(juce::Colour(0xff3a465a));
    g.drawHorizontalLine(gainToY(0.0f), 0.0f, (float) bounds.getWidth());

    constexpr int numPoints = 200;
    juce::Path path;

    for (int i = 0; i < numPoints; ++i)
    {
        auto t = (double) i / (double) (numPoints - 1);
        auto freq = minFrequency * std::pow(maxFrequency / minFrequency, t);

        auto magnitude = 1.0;
        for (const auto& band : bands)
            if (band.coefficients != nullptr)
                magnitude *= band.coefficients->getMagnitudeForFrequency(freq, sampleRate);

        auto db = juce::jlimit((double) minDb, (double) maxDb, juce::Decibels::gainToDecibels(magnitude, (double) minDb));
        auto x = frequencyToX(freq);
        auto y = gainToY((float) db);

        if (i == 0)
            path.startNewSubPath((float) x, (float) y);
        else
            path.lineTo((float) x, (float) y);
    }

    g.setColour(juce::Colour(0xff67e8a5));
    g.strokePath(path, juce::PathStrokeType(2.0f));

    for (int i = 0; i < (int) bands.size(); ++i)
    {
        const auto& band = bands[(size_t) i];
        auto x = frequencyToX(band.frequencyHz);
        auto y = band.hasGainAxis ? gainToY(band.gainDb) : gainToY(0.0f);

        g.setColour(juce::Colours::black);
        g.fillEllipse((float) x - 6.0f, (float) y - 6.0f, 12.0f, 12.0f);
        g.setColour(i == draggingBandIndex ? juce::Colours::white : juce::Colour(0xffffd166));
        g.fillEllipse((float) x - 4.5f, (float) y - 4.5f, 9.0f, 9.0f);
    }
}

void EqCurveComponent::mouseDown(const juce::MouseEvent& event)
{
    draggingBandIndex = hitTestBand(event.getPosition());
    repaint();
}

void EqCurveComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (draggingBandIndex < 0 || draggingBandIndex >= (int) bands.size())
        return;

    auto& band = bands[(size_t) draggingBandIndex];
    band.frequencyHz = juce::jlimit(minFrequency, maxFrequency, xToFrequency(event.x));
    if (band.hasGainAxis)
        band.gainDb = juce::jlimit(minDb, maxDb, yToGain(event.y));

    if (onBandDragged)
        onBandDragged(draggingBandIndex, band.frequencyHz, band.gainDb);

    repaint();
}

void EqCurveComponent::mouseUp(const juce::MouseEvent&)
{
    draggingBandIndex = -1;
    repaint();
}
}
