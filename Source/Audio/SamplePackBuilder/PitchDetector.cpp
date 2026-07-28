#include "PitchDetector.h"
#include <cmath>

namespace
{
// Skips the noisy pluck/strike transient and analyzes a sustained window after it, where the
// periodic content is cleanest. Falls back to the start of the buffer if the file is too short
// to have a meaningful post-transient region.
constexpr double transientSkipSeconds = 0.02;
constexpr double analysisWindowSeconds = 0.25;
constexpr double confidenceThreshold = 0.3;
}

PitchDetectionResult PitchDetector::detectPitch(const float* samples,
                                                int numSamples,
                                                double sampleRate,
                                                double minFrequencyHz,
                                                double maxFrequencyHz)
{
    PitchDetectionResult result;

    if (samples == nullptr || numSamples <= 0 || sampleRate <= 0.0
        || minFrequencyHz <= 0.0 || maxFrequencyHz <= minFrequencyHz)
        return result;

    auto minLag = juce::jmax(1, (int) std::floor(sampleRate / maxFrequencyHz));
    auto maxLag = (int) std::ceil(sampleRate / minFrequencyHz);

    auto skipSamples = (int) (transientSkipSeconds * sampleRate);
    auto windowSamples = (int) (analysisWindowSeconds * sampleRate);

    auto analysisStart = juce::jmin(skipSamples, juce::jmax(0, numSamples - maxLag - 1));
    auto analysisLength = juce::jmin(windowSamples, numSamples - analysisStart - maxLag - 1);

    if (analysisLength <= maxLag)
    {
        // Not enough signal after skipping the transient and leaving room for the largest lag -
        // fall back to analyzing from the very start of the buffer instead.
        analysisStart = 0;
        analysisLength = numSamples - maxLag - 1;
    }

    if (analysisLength <= minLag)
        return result; // file too short to say anything meaningful

    const float* window = samples + analysisStart;

    auto autocorrelationAt = [&](int lag) -> double
    {
        double sumXY = 0.0;
        double sumXX = 0.0;
        double sumYY = 0.0;

        for (int i = 0; i < analysisLength; ++i)
        {
            auto x = (double) window[i];
            auto y = (double) window[i + lag];
            sumXY += x * y;
            sumXX += x * x;
            sumYY += y * y;
        }

        auto denominator = std::sqrt(sumXX * sumYY);
        return denominator > 1.0e-9 ? sumXY / denominator : 0.0;
    };

    auto bestLag = -1;
    auto bestScore = 0.0;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        auto score = autocorrelationAt(lag);
        if (score > bestScore)
        {
            bestScore = score;
            bestLag = lag;
        }
    }

    if (bestLag < 0 || bestScore < confidenceThreshold)
        return result;

    // Parabolic interpolation around the peak for sub-sample lag precision - a fraction-of-a-
    // sample lag error translates directly into cents of tuning error, which matters a lot here.
    auto refinedLag = (double) bestLag;
    if (bestLag > minLag && bestLag < maxLag)
    {
        auto y0 = autocorrelationAt(bestLag - 1);
        auto y1 = bestScore;
        auto y2 = autocorrelationAt(bestLag + 1);
        auto denom = y0 - 2.0 * y1 + y2;
        if (std::abs(denom) > 1.0e-9)
        {
            auto offset = 0.5 * (y0 - y2) / denom;
            refinedLag = (double) bestLag + juce::jlimit(-1.0, 1.0, offset);
        }
    }

    result.detected = true;
    result.frequencyHz = sampleRate / refinedLag;
    result.confidence = (float) juce::jlimit(0.0, 1.0, bestScore);
    return result;
}
