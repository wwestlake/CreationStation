#pragma once

#include <JuceHeader.h>
#include <vector>
#include "TimelineTypes.h"

namespace cs
{
struct TimelineClip
{
    juce::String id;
    ClipKind kind = ClipKind::audio;
    juce::String displayName;
    juce::String assetId;
    juce::String sourceTool;
    int trackIndex = -1;
    juce::File file;
    double startSeconds = 0.0;
    double durationSeconds = 0.0;
    double sourceStartSeconds = 0.0;
    double sourceDurationSeconds = 0.0;
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
    void setMusicalKey(const juce::String& key);
    juce::String getMusicalKey() const { return musicalKey; }

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
    int addClip(ClipKind kind,
                int trackIndex,
                const juce::String& displayName,
                const juce::String& assetId,
                const juce::String& sourceTool,
                const juce::File& file,
                double startSeconds,
                double durationSeconds,
                juce::String& errorMessage);
    bool moveClip(int clipIndex, int trackIndex, double startSeconds);
    void setClipDisplayName(int clipIndex, const juce::String& displayName);
    bool duplicateClip(int clipIndex, double startOffsetSeconds = 0.25);
    bool deleteClip(int clipIndex);
    bool splitClip(int clipIndex, double splitSeconds);
    bool hasActiveRecordingClip() const noexcept { return ! activeRecordingClips.empty(); }

    juce::String addMarker(double seconds, const juce::String& name = {});
    void removeMarker(const juce::String& id);
    void renameMarker(const juce::String& id, const juce::String& name);
    const std::vector<TimelineMarker>& getMarkers() const noexcept { return markers; }

    void setLoopRegion(double startSeconds, double endSeconds);
    double getLoopStartSeconds() const noexcept { return loopStartSeconds; }
    double getLoopEndSeconds() const noexcept { return loopEndSeconds; }
    void setLoopEnabled(bool shouldEnable) noexcept { loopEnabled = shouldEnable; }
    bool isLoopEnabled() const noexcept { return loopEnabled; }

    const std::vector<TimelineClip>& getClips() const noexcept { return clips; }
    const std::vector<TimelineTrack>& getTracks() const noexcept { return tracks; }
    int getTrackCount() const noexcept { return static_cast<int>(tracks.size()); }
    void setTrackCount(int count);
    void addTrack(TrackKind kind = TrackKind::audio, const juce::String& name = {});
    void setTrackName(int trackIndex, const juce::String& name);
    juce::String getTrackName(int trackIndex) const;
    void setTrackKind(int trackIndex, TrackKind kind);
    TrackKind getTrackKind(int trackIndex) const;
    void setTrackChannelMode(int trackIndex, TrackChannelMode mode);
    TrackChannelMode getTrackChannelMode(int trackIndex) const;
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
    juce::String musicalKey = "C";
    double pixelsPerSecond = 120.0;
    double transportSeconds = 0.0;
    double recordingStartSeconds = 0.0;
    std::vector<int> activeRecordingClips;
    std::vector<TimelineTrack> tracks;
    std::vector<TimelineClip> clips;
    std::vector<TimelineMarker> markers;
    double loopStartSeconds = 0.0;
    double loopEndSeconds = 0.0;
    bool loopEnabled = false;

    juce::AudioFormatManager formatManager;
};
}
