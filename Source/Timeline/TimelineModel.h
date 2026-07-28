#pragma once

#include <JuceHeader.h>
#include <vector>
#include "TimelineTypes.h"
#include "../Project/ProjectStorage.h"

namespace cs
{
struct MidiNoteEvent
{
    juce::String id;
    int pitch = 60;            // MIDI note number, 0-127
    int velocity = 100;        // 1-127
    double startBeats = 0.0;   // relative to clip start
    double lengthBeats = 1.0;
    int channel = 1;           // MIDI channel, 1-16
    bool muted = false;
};

struct MidiCCEvent
{
    juce::String id;
    int controller = 1;        // CC number, 0-127
    int value = 0;             // 0-127
    double beats = 0.0;        // relative to clip start
};

struct TimelineClip
{
    juce::String id;
    ClipKind kind = ClipKind::audio;
    juce::String displayName;
    juce::String assetId;
    cs::AssetVersionId assetVersionId;
    cs::AssetReferenceMode assetReferenceMode = cs::AssetReferenceMode::exact;
    juce::String sourceTool;
    int trackIndex = -1;
    juce::File file;
    double startSeconds = 0.0;
    double durationSeconds = 0.0;
    double sourceStartSeconds = 0.0;
    double sourceDurationSeconds = 0.0;
    int sourceNumChannels = 0;
    bool recording = false;
    std::vector<float> peaks;
    std::vector<float> rightPeaks;
    std::vector<MidiNoteEvent> midiNotes;
    std::vector<MidiCCEvent> midiCC;
    std::vector<AutomationPoint> automationPoints;
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
    int beginRecordingMidiClip(int trackIndex);
    void updateRecordingClip(double nowSeconds);
    void addRecordingPeak(int trackIndex, float peak);
    // Replaces the notes of the in-progress MIDI recording clip on the given track (if any) - the
    // caller pairs raw note-on/off events into notes itself, so this just installs the result.
    void setRecordingClipMidiNotes(int trackIndex, std::vector<MidiNoteEvent> notes);
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
    void setClipAssetReference(int clipIndex, const cs::AssetRef& assetRef);
    void setClipFile(int clipIndex, const juce::File& file);
    void setClipDuration(int clipIndex, double newDurationSeconds);
    bool duplicateClip(int clipIndex, double startOffsetSeconds = 0.25);
    bool deleteClip(int clipIndex);
    bool splitClip(int clipIndex, double splitSeconds);
    bool hasActiveRecordingClip() const noexcept { return ! activeRecordingClips.empty(); }

    juce::String addMarker(double seconds, const juce::String& name = {});
    void removeMarker(const juce::String& id);
    void renameMarker(const juce::String& id, const juce::String& name);
    const std::vector<TimelineMarker>& getMarkers() const noexcept { return markers; }

    void setLoopRegion(double startSeconds, double endSeconds);
    // Collapses the region to nothing (removes the highlight) without touching the separate
    // loop-enabled toggle - matches how playback already guards on end > start, so a cleared
    // region just stops being drawn and stops affecting playback, with no other state to fix up.
    void clearLoopRegion() noexcept { loopStartSeconds = 0.0; loopEndSeconds = 0.0; }
    double getLoopStartSeconds() const noexcept { return loopStartSeconds; }
    double getLoopEndSeconds() const noexcept { return loopEndSeconds; }
    void setLoopEnabled(bool shouldEnable) noexcept { loopEnabled = shouldEnable; }
    bool isLoopEnabled() const noexcept { return loopEnabled; }

    // Timeline (tracker) snap-to-grid - separate from the MIDI piano-roll editor's own grid,
    // since they're different views with independent zoom/resolution needs.
    void setTimelineSnapEnabled(bool shouldSnap) noexcept { timelineSnapEnabled = shouldSnap; }
    bool isTimelineSnapEnabled() const noexcept { return timelineSnapEnabled; }
    // In beats (1.0 = a quarter note), matching the convention used by beatToSeconds/secondsToBeat.
    void setTimelineGridBeats(double beats) noexcept { timelineGridBeats = juce::jmax(0.0078125, beats); }
    double getTimelineGridBeats() const noexcept { return timelineGridBeats; }
    double snapTimelineSeconds(double seconds) const noexcept;

    const std::vector<TimelineClip>& getClips() const noexcept { return clips; }
    const std::vector<TimelineTrack>& getTracks() const noexcept { return tracks; }
    int getTrackCount() const noexcept { return static_cast<int>(tracks.size()); }
    void setTrackCount(int count);
    void addTrack(TrackKind kind = TrackKind::audio, const juce::String& name = {});
    void setTrackName(int trackIndex, const juce::String& name);
    juce::String getTrackName(int trackIndex) const;
    // Stable across reorders (unlike the index itself) - useful for anything that needs to key
    // off "this specific track" rather than "whatever's currently at this position" (e.g. a
    // folder's derived accent colour, which shouldn't change just because tracks got reordered).
    juce::String getTrackId(int trackIndex) const;
    void setTrackKind(int trackIndex, TrackKind kind);
    TrackKind getTrackKind(int trackIndex) const;
    void setTrackChannelMode(int trackIndex, TrackChannelMode mode);
    TrackChannelMode getTrackChannelMode(int trackIndex) const;
    // Assigns trackIndex as a child of the given folder track (parentTrackIndex == -1 detaches
    // it back to top-level). Rejects (returns false, no change) if parentTrackIndex isn't a
    // TrackKind::folder track, or if trackIndex is already an ancestor of it (would create a
    // cycle) - the caller should surface that as a status message rather than silently no-op.
    bool setTrackParent(int trackIndex, int parentTrackIndex);
    int getTrackParent(int trackIndex) const;
    // How many tracks starting at folderIndex form its contiguous group block (the folder itself
    // plus every child/grandchild/... that the group-order invariant keeps directly after it).
    // Returns 0 if folderIndex is out of range, 1 if it has no children (or isn't a folder).
    int getFolderBlockLength(int folderIndex) const;
    // Moves the contiguous range [startIndex, startIndex+length) to sit at destinationIndex
    // (clamped to a valid position), remapping every index-based reference (clip.trackIndex,
    // every track's parentTrackIndex, every track's automationTarget.targetTrackIndex) so nothing
    // silently starts pointing at the wrong track. Returns the range's new start index, or -1.
    int moveTrackRange(int startIndex, int length, int destinationIndex);
    // The one entry point track-reorder UI should call: moves trackIndex to destinationIndex,
    // automatically carrying its whole folder block along if it's a folder, or - if moving a
    // single track breaks contiguity with its former folder - detaching it (parent -> -1) rather
    // than leaving a routing relationship the track list no longer visually shows.
    bool moveTrackGroup(int trackIndex, int destinationIndex);
    double getTotalDurationSeconds() const noexcept;
    void removeTrack(int trackIndex);
    void clear();

    bool analyzeClipWaveform(int clipIndex, juce::String& errorMessage);
    juce::ValueTree createState() const;
    void restoreState(const juce::ValueTree& state);

    // MIDI note editing
    const std::vector<MidiNoteEvent>* getMidiNotes(int clipIndex) const;
    juce::String addMidiNote(int clipIndex, int pitch, double startBeats, double lengthBeats, int velocity = 100, int channel = 1);
    bool removeMidiNote(int clipIndex, const juce::String& noteId);
    bool removeMidiNotes(int clipIndex, const juce::StringArray& noteIds);
    bool updateMidiNote(int clipIndex, const juce::String& noteId, int pitch, double startBeats, double lengthBeats);
    bool setMidiNoteVelocity(int clipIndex, const juce::String& noteId, int velocity);
    void quantizeMidiNotes(int clipIndex, const juce::StringArray& noteIds, double gridBeats, float strength);
    void quantizeMidiNoteVelocities(int clipIndex, const juce::StringArray& noteIds, int targetVelocity, float strength);
    void applySwing(int clipIndex, const juce::StringArray& noteIds, double gridBeats, float swingAmount);
    void humanizeMidiNotes(int clipIndex, const juce::StringArray& noteIds, double timingAmountBeats, int velocityAmount, float lengthAmountPercent);

    // MIDI CC lane editing
    const std::vector<MidiCCEvent>* getMidiCC(int clipIndex) const;
    juce::String addOrUpdateMidiCCPoint(int clipIndex, int controller, double beats, int value, double mergeToleranceBeats = 0.02);
    bool removeMidiCCPoint(int clipIndex, const juce::String& pointId);
    void clearMidiCCLane(int clipIndex, int controller);

    // Automation lane editing. Each automation track owns exactly one auto-created automation
    // clip (see ensureAutomationClip); callers work in terms of trackIndex, not clipIndex, since
    // that clip's own start/duration are not load-bearing - only the points' absolute seconds are.
    void ensureAutomationClip(int trackIndex);
    const std::vector<AutomationPoint>* getAutomationPoints(int trackIndex) const;
    juce::String addOrUpdateAutomationPoint(int trackIndex, double seconds, float value, double mergeToleranceSeconds = 0.02);
    // Moves a specific existing point (found by id) to a new position, preserving its shape/
    // tension - unlike addOrUpdateAutomationPoint, which merges by proximity to the given
    // seconds and would otherwise leave a trail of points behind as one is dragged.
    bool moveAutomationPoint(int trackIndex, const juce::String& pointId, double newSeconds, float newValue);
    bool setAutomationPointShape(int trackIndex, const juce::String& pointId, AutomationCurveShape shape, float tension);
    bool removeAutomationPoint(int trackIndex, const juce::String& pointId);
    void clearAutomationLane(int trackIndex);
    float evaluateAutomationValue(int trackIndex, double seconds) const;
    void setAutomationTarget(int trackIndex, const AutomationTarget& target);
    const AutomationTarget& getAutomationTarget(int trackIndex) const;
    void setAutomationRecordMode(int trackIndex, AutomationRecordMode mode);
    AutomationRecordMode getAutomationRecordMode(int trackIndex) const;
    void setAutomationRecordingRate(int trackIndex, int pointsPerSecond);
    int getAutomationRecordingRate(int trackIndex) const;

private:
    int getAutomationClipIndex(int trackIndex) const;

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
    bool timelineSnapEnabled = true;
    double timelineGridBeats = 1.0;

    juce::AudioFormatManager formatManager;
};
}
