#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <vector>

// Turns a folder of scattered, unlabeled single-note recordings into a standardized 128-note
// chromatic sample pack: detects each capture's true pitch, corrects tuning drift, keeps the
// single cleanest capture when two land on the same note, fills every uncaptured note by
// transposing its nearest real neighbor, and exports Note_000.wav .. Note_127.wav.
//
// This is an offline batch tool (runs synchronously on whatever thread calls build()), not a
// real-time audio path - callers doing this from a UI should run it off the message thread.
class SamplePackBuilderEngine
{
public:
    struct FileReport
    {
        juce::String fileName;
        bool analyzed = false;        // false if the file couldn't be read or had no confident pitch
        juce::String skipReason;      // set when analyzed == false
        int midiNote = -1;
        double detectedFrequencyHz = 0.0;
        double centsOffset = 0.0;     // positive = was sharp, negative = was flat
        float confidence = 0.0f;
        bool keptAsCleanest = true;   // false if a better capture for the same note won instead
    };

    struct BuildResult
    {
        bool success = false;
        juce::String errorMessage;    // set when success == false
        std::vector<FileReport> fileReports;    // one per input file
        std::array<bool, 128> notesCaptured {}; // true if directly captured (not gap-filled)
        std::array<bool, 128> notesExported {}; // true if a file was written for this note at all
    };

    // Runs the full pipeline. progressCallback, if set, is invoked with a short human-readable
    // status line after each input file is analyzed and again for each gap-filled note - intended
    // for driving a UI progress log.
    static BuildResult build(const juce::File& inputFolder,
                             const juce::File& outputFolder,
                             std::function<void(const juce::String&)> progressCallback = nullptr);

private:
    struct AnalyzedCapture
    {
        juce::String sourceFileName;
        int midiNote = -1;
        double detectedFrequencyHz = 0.0;
        double centsOffset = 0.0;
        float confidence = 0.0f;
        float peakToNoiseRatio = 0.0f;
        juce::AudioBuffer<float> correctedBuffer; // mono, tuning-corrected, trimmed, normalized
        double sampleRate = 44100.0;
    };

    static bool readAndAnalyzeFile(const juce::File& file,
                                   juce::AudioFormatManager& formatManager,
                                   AnalyzedCapture& outCapture,
                                   juce::String& outError);

    // Resamples a mono buffer by the given pitch shift in cents (positive = up, negative = down).
    // Used both for small tuning-drift corrections and for whole-semitone gap-filling shifts -
    // same underlying math, just a different magnitude.
    static juce::AudioBuffer<float> resampleBySemitoneCents(const juce::AudioBuffer<float>& source,
                                                             double appliedCents);
    static void trimLeadingSilence(juce::AudioBuffer<float>& buffer);
    static void normalizePeak(juce::AudioBuffer<float>& buffer, float targetPeak = 0.95f);
    static bool writeNoteFile(const juce::File& outputFolder, int midiNote,
                             const juce::AudioBuffer<float>& buffer, double sampleRate);
};
