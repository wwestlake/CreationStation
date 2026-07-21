#pragma once

#include <JuceHeader.h>
#include <vector>

namespace cs
{
struct TimelineClip
{
    juce::String id;
    int trackIndex = -1;
    juce::File file;
    double startSeconds = 0.0;
    double durationSeconds = 0.0;
    bool recording = false;
    std::vector<float> peaks;
};

class TimelineModel final
{
public:
    void setTempo(double bpm, int numerator, int denominator);
    double getTempoBpm() const noexcept { return tempoBpm; }
    int getTimeSignatureNumerator() const noexcept { return timeSignatureNumerator; }
    int getTimeSignatureDenominator() const noexcept { return timeSignatureDenominator; }

    void setPixelsPerSecond(double newPixelsPerSecond);
    double getPixelsPerSecond() const noexcept { return pixelsPerSecond; }
    void zoomIn();
    void zoomOut();

    void setTransportSeconds(double seconds);
    double getTransportSeconds() const noexcept { return transportSeconds; }
    double getNextBoundarySeconds(double fromSeconds) const noexcept;
    double getPreviousBoundarySeconds(double fromSeconds) const noexcept;
    double beatToSeconds(double beat) const noexcept;
    double secondsToBeat(double seconds) const noexcept;

    int beginRecordingClip(int trackIndex, const juce::File& file);
    void updateRecordingClip(double nowSeconds);
    void addRecordingPeak(int trackIndex, float peak);
    void finishRecordingClip(double nowSeconds);
    void cancelRecordingClip();
    int addAudioClip(int trackIndex, const juce::File& file, double startSeconds, juce::String& errorMessage);
    bool moveClip(int clipIndex, int trackIndex, double startSeconds);
    bool hasActiveRecordingClip() const noexcept { return ! activeRecordingClips.empty(); }

    const std::vector<TimelineClip>& getClips() const noexcept { return clips; }
    double getTotalDurationSeconds() const noexcept;
    void removeTrack(int trackIndex);
    void clear();

    bool analyzeClipWaveform(int clipIndex, juce::String& errorMessage);
    juce::ValueTree createState() const;
    void restoreState(const juce::ValueTree& state);

private:
    double tempoBpm = 120.0;
    int timeSignatureNumerator = 4;
    int timeSignatureDenominator = 4;
    double pixelsPerSecond = 120.0;
    double transportSeconds = 0.0;
    double recordingStartSeconds = 0.0;
    std::vector<int> activeRecordingClips;
    std::vector<TimelineClip> clips;

    juce::AudioFormatManager formatManager;
};
}
