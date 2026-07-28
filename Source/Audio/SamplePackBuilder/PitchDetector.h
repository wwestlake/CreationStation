#pragma once

#include <JuceHeader.h>

// Estimates the fundamental frequency (f0) of a monophonic, pitched recording - built for the
// Sample Pack Builder's "organic capture, let the tool figure out the pitch" workflow (plucked
// strings, struck keys), not for polyphonic or heavily percussive/noisy material.
//
// Uses normalized autocorrelation rather than FFT peak-picking: a plucked string's loudest
// partial is often a harmonic, not the fundamental, which trips up naive spectral peak-picking.
// Autocorrelation instead looks for the periodicity itself, which stays anchored to the true f0
// regardless of which harmonic happens to be loudest.
struct PitchDetectionResult
{
    bool detected = false;
    double frequencyHz = 0.0;
    // 0..1, the normalized autocorrelation peak height at the detected lag - how strongly
    // periodic the signal is at that lag. Used both to reject noise/silence and, later, as the
    // "cleanest capture" tie-break when two takes land on the same MIDI note.
    float confidence = 0.0f;
};

class PitchDetector
{
public:
    // samples/numSamples: a single-channel (already mixed-down) buffer.
    // minFrequencyHz/maxFrequencyHz: the plausible pitch range to search - narrowing this both
    // speeds up the search and avoids octave errors from picking up sub-harmonic periodicity
    // outside the instrument's real range.
    static PitchDetectionResult detectPitch(const float* samples,
                                            int numSamples,
                                            double sampleRate,
                                            double minFrequencyHz = 60.0,
                                            double maxFrequencyHz = 2000.0);
};
