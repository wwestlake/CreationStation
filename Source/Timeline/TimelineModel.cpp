#include "TimelineModel.h"
#include <algorithm>

namespace cs
{
namespace
{
juce::String makeDefaultTrackName(int trackIndex)
{
    return "Tk-" + juce::String(trackIndex + 1).paddedLeft('0', 4);
}
}

void TimelineModel::setTempo(double bpm, int numerator, int denominator)
{
    tempoBpm = juce::jlimit(20.0, 320.0, bpm);
    timeSignatureNumerator = juce::jlimit(1, 16, numerator);
    timeSignatureDenominator = juce::jlimit(1, 32, denominator);
}

void TimelineModel::setMusicalKey(const juce::String& key)
{
    auto trimmed = key.trim();
    musicalKey = trimmed.isNotEmpty() ? trimmed : "C";
}

void TimelineModel::setPixelsPerSecond(double newPixelsPerSecond)
{
    pixelsPerSecond = juce::jlimit(6.0, 3600.0, newPixelsPerSecond);
}

void TimelineModel::zoomIn()
{
    setPixelsPerSecond(pixelsPerSecond * 1.8);
}

void TimelineModel::zoomOut()
{
    setPixelsPerSecond(pixelsPerSecond / 1.8);
}

void TimelineModel::setTransportSeconds(double seconds)
{
    transportSeconds = juce::jmax(0.0, seconds);
}

double TimelineModel::getNextBoundarySeconds(double fromSeconds) const noexcept
{
    auto next = std::numeric_limits<double>::max();
    auto threshold = fromSeconds + 0.01;

    for (const auto& clip : clips)
    {
        if (clip.startSeconds > threshold)
            next = juce::jmin(next, clip.startSeconds);

        auto endSeconds = clip.startSeconds + clip.durationSeconds;
        if (endSeconds > threshold)
            next = juce::jmin(next, endSeconds);
    }

    if (next != std::numeric_limits<double>::max())
        return next;

    auto measureSeconds = beatToSeconds(timeSignatureNumerator);
    return fromSeconds + juce::jmax(0.25, measureSeconds);
}

double TimelineModel::getPreviousBoundarySeconds(double fromSeconds) const noexcept
{
    auto previous = 0.0;
    auto threshold = fromSeconds - 0.01;

    for (const auto& clip : clips)
    {
        if (clip.startSeconds < threshold)
            previous = juce::jmax(previous, clip.startSeconds);

        auto endSeconds = clip.startSeconds + clip.durationSeconds;
        if (endSeconds < threshold)
            previous = juce::jmax(previous, endSeconds);
    }

    return previous;
}

double TimelineModel::beatToSeconds(double beat) const noexcept
{
    return beat * 60.0 / tempoBpm;
}

double TimelineModel::secondsToBeat(double seconds) const noexcept
{
    return seconds * tempoBpm / 60.0;
}

int TimelineModel::beginRecordingClip(int trackIndex, const juce::File& file)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        setTrackCount(trackIndex + 1);

    if (! canTrackContainClip(getTrackKind(trackIndex), ClipKind::audio))
        return -1;

    TimelineClip clip;
    clip.id = juce::Uuid().toString();
    clip.kind = ClipKind::audio;
    clip.displayName = file.getFileNameWithoutExtension();
    clip.sourceTool = "recording";
    clip.trackIndex = trackIndex;
    clip.file = file;
    clip.startSeconds = transportSeconds;
    clip.durationSeconds = 0.0;
    clip.sourceStartSeconds = 0.0;
    clip.sourceDurationSeconds = 0.0;
    clip.sourceNumChannels = 0;
    clip.recording = true;

    clips.push_back(std::move(clip));
    auto clipIndex = static_cast<int>(clips.size()) - 1;
    activeRecordingClips.push_back(clipIndex);

    if (activeRecordingClips.size() == 1)
        recordingStartSeconds = transportSeconds;

    return clipIndex;
}

int TimelineModel::beginRecordingMidiClip(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        setTrackCount(trackIndex + 1);

    if (! canTrackContainClip(getTrackKind(trackIndex), ClipKind::midi))
        return -1;

    TimelineClip clip;
    clip.id = juce::Uuid().toString();
    clip.kind = ClipKind::midi;
    clip.displayName = "MIDI Recording";
    clip.sourceTool = "recording";
    clip.trackIndex = trackIndex;
    clip.startSeconds = transportSeconds;
    clip.durationSeconds = 0.0;
    clip.recording = true;

    clips.push_back(std::move(clip));
    auto clipIndex = static_cast<int>(clips.size()) - 1;
    activeRecordingClips.push_back(clipIndex);

    if (activeRecordingClips.size() == 1)
        recordingStartSeconds = transportSeconds;

    return clipIndex;
}

void TimelineModel::setRecordingClipMidiNotes(int trackIndex, std::vector<MidiNoteEvent> notes)
{
    for (auto clipIndex : activeRecordingClips)
    {
        if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
            continue;

        auto& clip = clips[(size_t) clipIndex];
        if (clip.trackIndex != trackIndex || clip.kind != ClipKind::midi)
            continue;

        clip.midiNotes = std::move(notes);
        return;
    }
}

void TimelineModel::updateRecordingClip(double nowSeconds)
{
    setTransportSeconds(nowSeconds);

    for (auto clipIndex : activeRecordingClips)
    {
        if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
            continue;

        auto& clip = clips[(size_t) clipIndex];
        clip.durationSeconds = juce::jmax(0.05, transportSeconds - clip.startSeconds);
    }
}

void TimelineModel::addRecordingPeak(int trackIndex, float peak)
{
    auto clampedPeak = juce::jlimit(0.0f, 1.0f, peak);

    for (auto clipIndex : activeRecordingClips)
    {
        if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
            continue;

        auto& clip = clips[(size_t) clipIndex];
        if (clip.trackIndex != trackIndex || ! clip.recording)
            continue;

        clip.peaks.push_back(clampedPeak);

        constexpr auto maxLivePeaks = 4096;
        if (clip.peaks.size() > maxLivePeaks)
            clip.peaks.erase(clip.peaks.begin(), clip.peaks.begin() + static_cast<std::ptrdiff_t>(clip.peaks.size() - maxLivePeaks));
    }
}

void TimelineModel::finishRecordingClip(double nowSeconds)
{
    updateRecordingClip(nowSeconds);

    auto clipsToAnalyze = activeRecordingClips;
    activeRecordingClips.clear();

    for (auto clipIndex : clipsToAnalyze)
    {
        if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
            continue;

        clips[(size_t) clipIndex].recording = false;

        if (clips[(size_t) clipIndex].kind == ClipKind::audio)
        {
            juce::String errorMessage;
            analyzeClipWaveform(clipIndex, errorMessage);
        }
    }
}

void TimelineModel::cancelRecordingClip()
{
    std::sort(activeRecordingClips.begin(), activeRecordingClips.end(), std::greater<int>());

    for (auto clipIndex : activeRecordingClips)
        if (juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
            clips.erase(clips.begin() + clipIndex);

    activeRecordingClips.clear();
}

int TimelineModel::addAudioClip(int trackIndex, const juce::File& file, double startSeconds, juce::String& errorMessage)
{
    return addClip(ClipKind::audio,
                   trackIndex,
                   file.getFileNameWithoutExtension(),
                   file.getFileName(),
                   "audio",
                   file,
                   startSeconds,
                   0.05,
                   errorMessage);
}

int TimelineModel::addClip(ClipKind kind,
                           int trackIndex,
                           const juce::String& displayName,
                           const juce::String& assetId,
                           const juce::String& sourceTool,
                           const juce::File& file,
                           double startSeconds,
                           double durationSeconds,
                           juce::String& errorMessage)
{
    if (trackIndex < 0)
    {
        errorMessage = "Choose a valid track before placing the sound.";
        return -1;
    }

    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        setTrackCount(trackIndex + 1);

    if (! canTrackContainClip(getTrackKind(trackIndex), kind))
    {
        errorMessage = "That clip type cannot live on the selected track.";
        return -1;
    }

    if ((kind == ClipKind::audio || kind == ClipKind::foley) && ! file.existsAsFile())
    {
        errorMessage = "The rendered sound file was not found.";
        return -1;
    }

    TimelineClip clip;
    clip.id = juce::Uuid().toString();
    clip.kind = kind;
    clip.displayName = displayName.trim().isNotEmpty() ? displayName.trim()
                                                       : (file.existsAsFile() ? file.getFileNameWithoutExtension()
                                                                              : toDisplayName(kind) + " Clip");
    clip.assetId = assetId.trim();
    clip.sourceTool = sourceTool.trim();
    clip.trackIndex = trackIndex;
    clip.file = file;
    clip.startSeconds = juce::jmax(0.0, startSeconds);
    const auto isFileBackedAudioClip = (kind == ClipKind::audio || kind == ClipKind::foley) && file.existsAsFile();
    clip.durationSeconds = isFileBackedAudioClip ? juce::jmax(0.0, durationSeconds)
                                                 : juce::jmax(0.05, durationSeconds);
    clip.sourceStartSeconds = 0.0;
    clip.sourceDurationSeconds = clip.durationSeconds;
    clip.recording = false;

    clips.push_back(std::move(clip));
    const auto clipIndex = static_cast<int>(clips.size()) - 1;

    if (file.existsAsFile() && ! analyzeClipWaveform(clipIndex, errorMessage))
    {
        clips.erase(clips.begin() + clipIndex);
        return -1;
    }

    return clipIndex;
}

void TimelineModel::setClipDisplayName(int clipIndex, const juce::String& displayName)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return;

    auto cleaned = displayName.trim();
    if (cleaned.isNotEmpty())
        clips[(size_t) clipIndex].displayName = cleaned;
}

void TimelineModel::setClipAssetReference(int clipIndex, const cs::AssetRef& assetRef)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return;

    auto& clip = clips[(size_t) clipIndex];
    clip.assetId = assetRef.id.trim();
    clip.assetVersionId = assetRef.versionId.trim();
    clip.assetReferenceMode = assetRef.mode;
}

void TimelineModel::setClipFile(int clipIndex, const juce::File& file)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return;

    clips[(size_t) clipIndex].file = file;
}

void TimelineModel::setClipDuration(int clipIndex, double newDurationSeconds)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return;

    clips[(size_t) clipIndex].durationSeconds = juce::jmax(0.05, newDurationSeconds);
}

bool TimelineModel::moveClip(int clipIndex, int trackIndex, double startSeconds)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())) || ! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return false;

    auto& clip = clips[(size_t) clipIndex];
    if (clip.recording)
        return false;

    if (! canTrackContainClip(getTrackKind(trackIndex), clip.kind))
        return false;

    clip.trackIndex = trackIndex;
    clip.startSeconds = juce::jmax(0.0, startSeconds);
    return true;
}

bool TimelineModel::duplicateClip(int clipIndex, double startOffsetSeconds)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return false;

    auto duplicate = clips[(size_t) clipIndex];
    if (duplicate.recording)
        return false;

    duplicate.id = juce::Uuid().toString();
    duplicate.displayName = duplicate.displayName.isNotEmpty() ? duplicate.displayName + " copy"
                                                               : duplicate.file.getFileNameWithoutExtension() + " copy";
    duplicate.startSeconds += juce::jmax(0.0, startOffsetSeconds);
    duplicate.recording = false;
    clips.push_back(std::move(duplicate));
    return true;
}

bool TimelineModel::deleteClip(int clipIndex)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return false;

    if (clips[(size_t) clipIndex].recording)
        return false;

    clips.erase(clips.begin() + clipIndex);
    return true;
}

bool TimelineModel::splitClip(int clipIndex, double splitSeconds)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return false;

    auto& clip = clips[(size_t) clipIndex];
    if (clip.recording)
        return false;

    constexpr auto minimumClipSeconds = 0.02;
    auto localSplitSeconds = splitSeconds - clip.startSeconds;
    if (localSplitSeconds <= minimumClipSeconds || localSplitSeconds >= clip.durationSeconds - minimumClipSeconds)
        return false;

    auto rightClip = clip;
    rightClip.id = juce::Uuid().toString();
    rightClip.startSeconds = splitSeconds;
    rightClip.sourceStartSeconds = clip.sourceStartSeconds + localSplitSeconds;
    rightClip.durationSeconds = clip.durationSeconds - localSplitSeconds;
    rightClip.displayName = clip.displayName.isNotEmpty() ? clip.displayName + " B"
                                                          : clip.file.getFileNameWithoutExtension() + " B";

    clip.durationSeconds = localSplitSeconds;
    if (clip.displayName.isNotEmpty())
        clip.displayName += " A";

    clips.insert(clips.begin() + clipIndex + 1, std::move(rightClip));
    return true;
}

void TimelineModel::setTrackCount(int count)
{
    count = juce::jmax(0, count);

    while (static_cast<int>(tracks.size()) < count)
        addTrack();

    while (static_cast<int>(tracks.size()) > count)
        removeTrack(static_cast<int>(tracks.size()) - 1);
}

void TimelineModel::addTrack(TrackKind kind, const juce::String& name)
{
    TimelineTrack track;
    track.id = juce::Uuid().toString();
    track.kind = kind;
    track.name = name.trim().isNotEmpty() ? name.trim()
                                          : makeDefaultTrackName(static_cast<int>(tracks.size()));
    tracks.push_back(std::move(track));
}

void TimelineModel::setTrackName(int trackIndex, const juce::String& name)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return;

    auto cleaned = name.trim();
    tracks[(size_t) trackIndex].name = cleaned.isNotEmpty() ? cleaned : makeDefaultTrackName(trackIndex);
}

juce::String TimelineModel::getTrackName(int trackIndex) const
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return {};

    return tracks[(size_t) trackIndex].name;
}

juce::String TimelineModel::getTrackId(int trackIndex) const
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return {};

    return tracks[(size_t) trackIndex].id;
}

void TimelineModel::setTrackKind(int trackIndex, TrackKind kind)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return;

    // A folder losing its folder-ness can't stay a routing destination - an orphaned bus
    // reference is worse than silently detaching its children back to top-level.
    if (tracks[(size_t) trackIndex].kind == TrackKind::folder && kind != TrackKind::folder)
    {
        for (auto& track : tracks)
            if (track.parentTrackIndex == trackIndex)
                track.parentTrackIndex = -1;
    }

    tracks[(size_t) trackIndex].kind = kind;
}

bool TimelineModel::setTrackParent(int trackIndex, int parentTrackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return false;

    if (parentTrackIndex < 0)
    {
        tracks[(size_t) trackIndex].parentTrackIndex = -1;
        return true;
    }

    if (! juce::isPositiveAndBelow(parentTrackIndex, getTrackCount()) || parentTrackIndex == trackIndex)
        return false;

    if (tracks[(size_t) parentTrackIndex].kind != TrackKind::folder)
        return false;

    // Reject if trackIndex is already an ancestor of parentTrackIndex - assigning would close a
    // loop. The guard bounds the walk even if a pre-existing (shouldn't-happen) cycle exists.
    auto ancestor = parentTrackIndex;
    for (int guard = 0; ancestor >= 0 && guard < getTrackCount(); ++guard)
    {
        if (ancestor == trackIndex)
            return false;

        ancestor = juce::isPositiveAndBelow(ancestor, getTrackCount()) ? tracks[(size_t) ancestor].parentTrackIndex : -1;
    }

    tracks[(size_t) trackIndex].parentTrackIndex = parentTrackIndex;
    return true;
}

int TimelineModel::getTrackParent(int trackIndex) const
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return -1;

    return tracks[(size_t) trackIndex].parentTrackIndex;
}

int TimelineModel::getFolderBlockLength(int folderIndex) const
{
    if (! juce::isPositiveAndBelow(folderIndex, getTrackCount()))
        return 0;

    std::vector<bool> inBlock((size_t) getTrackCount(), false);
    inBlock[(size_t) folderIndex] = true;
    auto length = 1;

    for (int i = folderIndex + 1; i < getTrackCount(); ++i)
    {
        auto parent = tracks[(size_t) i].parentTrackIndex;
        if (parent < 0 || ! inBlock[(size_t) parent])
            break;

        inBlock[(size_t) i] = true;
        ++length;
    }

    return length;
}

int TimelineModel::moveTrackRange(int startIndex, int length, int destinationIndex)
{
    auto trackCount = getTrackCount();
    if (length <= 0 || ! juce::isPositiveAndBelow(startIndex, trackCount) || startIndex + length > trackCount)
        return -1;

    destinationIndex = juce::jlimit(0, trackCount - length, destinationIndex);

    // Simulate the erase+insert on a plain index permutation first, then apply the resulting
    // old->new remap to every index-based cross-reference in one pass.
    std::vector<int> order;
    order.reserve((size_t) trackCount);
    for (int i = 0; i < trackCount; ++i)
        order.push_back(i);

    std::vector<int> moving(order.begin() + startIndex, order.begin() + startIndex + length);
    order.erase(order.begin() + startIndex, order.begin() + startIndex + length);

    auto insertAt = destinationIndex > startIndex ? destinationIndex - length : destinationIndex;
    insertAt = juce::jlimit(0, (int) order.size(), insertAt);
    order.insert(order.begin() + insertAt, moving.begin(), moving.end());

    std::vector<int> oldToNew((size_t) trackCount, -1);
    for (int newIndex = 0; newIndex < trackCount; ++newIndex)
        oldToNew[(size_t) order[(size_t) newIndex]] = newIndex;

    std::vector<TimelineTrack> reordered;
    reordered.reserve((size_t) trackCount);
    for (auto oldIndex : order)
        reordered.push_back(tracks[(size_t) oldIndex]);

    for (auto& track : reordered)
    {
        if (juce::isPositiveAndBelow(track.parentTrackIndex, trackCount))
            track.parentTrackIndex = oldToNew[(size_t) track.parentTrackIndex];
        if (juce::isPositiveAndBelow(track.automationTarget.targetTrackIndex, trackCount))
            track.automationTarget.targetTrackIndex = oldToNew[(size_t) track.automationTarget.targetTrackIndex];
    }

    tracks = std::move(reordered);

    for (auto& clip : clips)
        if (juce::isPositiveAndBelow(clip.trackIndex, trackCount))
            clip.trackIndex = oldToNew[(size_t) clip.trackIndex];

    return insertAt;
}

bool TimelineModel::moveTrackGroup(int trackIndex, int destinationIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return false;

    auto blockLength = getTrackKind(trackIndex) == TrackKind::folder ? getFolderBlockLength(trackIndex) : 1;
    auto newStartIndex = moveTrackRange(trackIndex, blockLength, destinationIndex);
    if (newStartIndex < 0)
        return false;

    // A lone track (not a folder block) that no longer sits directly inside its former folder's
    // contiguous block after the move gets detached, rather than keeping a routing relationship
    // the track list no longer visually shows.
    if (blockLength == 1)
    {
        auto parent = tracks[(size_t) newStartIndex].parentTrackIndex;
        if (parent >= 0)
        {
            auto stillContiguous = newStartIndex > parent && newStartIndex < parent + getFolderBlockLength(parent);
            if (! stillContiguous)
                setTrackParent(newStartIndex, -1);
        }
    }

    return true;
}

TrackKind TimelineModel::getTrackKind(int trackIndex) const
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return TrackKind::audio;

    return tracks[(size_t) trackIndex].kind;
}

void TimelineModel::setTrackChannelMode(int trackIndex, TrackChannelMode mode)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return;

    tracks[(size_t) trackIndex].channelMode = mode;
}

TrackChannelMode TimelineModel::getTrackChannelMode(int trackIndex) const
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return TrackChannelMode::mono;

    return tracks[(size_t) trackIndex].channelMode;
}

void TimelineModel::clear()
{
    tracks.clear();
    clips.clear();
    activeRecordingClips.clear();
    markers.clear();
    loopStartSeconds = 0.0;
    loopEndSeconds = 0.0;
    loopEnabled = false;
    transportSeconds = 0.0;
}

juce::String TimelineModel::addMarker(double seconds, const juce::String& name)
{
    TimelineMarker marker;
    marker.id = juce::Uuid().toString();
    marker.seconds = juce::jmax(0.0, seconds);
    marker.name = name.isNotEmpty() ? name : ("Marker " + juce::String(static_cast<int>(markers.size()) + 1));
    markers.push_back(marker);

    std::sort(markers.begin(), markers.end(), [](const TimelineMarker& a, const TimelineMarker& b)
    {
        return a.seconds < b.seconds;
    });

    return marker.id;
}

void TimelineModel::removeMarker(const juce::String& id)
{
    markers.erase(std::remove_if(markers.begin(), markers.end(), [&id](const TimelineMarker& marker)
    {
        return marker.id == id;
    }), markers.end());
}

void TimelineModel::renameMarker(const juce::String& id, const juce::String& name)
{
    auto cleaned = name.trim();
    if (cleaned.isEmpty())
        return;

    for (auto& marker : markers)
    {
        if (marker.id == id)
        {
            marker.name = cleaned;
            return;
        }
    }
}

void TimelineModel::setLoopRegion(double startSeconds, double endSeconds)
{
    loopStartSeconds = juce::jmax(0.0, juce::jmin(startSeconds, endSeconds));
    loopEndSeconds = juce::jmax(0.0, juce::jmax(startSeconds, endSeconds));
}

double TimelineModel::snapTimelineSeconds(double seconds) const noexcept
{
    if (! timelineSnapEnabled || timelineGridBeats <= 0.0)
        return seconds;

    auto beat = secondsToBeat(seconds);
    auto snappedBeat = std::round(beat / timelineGridBeats) * timelineGridBeats;
    return juce::jmax(0.0, beatToSeconds(snappedBeat));
}

double TimelineModel::getTotalDurationSeconds() const noexcept
{
    auto total = 8.0;
    for (const auto& clip : clips)
        total = juce::jmax(total, clip.startSeconds + clip.durationSeconds + 2.0);

    return total;
}

void TimelineModel::removeTrack(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return;

    tracks.erase(tracks.begin() + trackIndex);

    clips.erase(std::remove_if(clips.begin(),
                               clips.end(),
                               [trackIndex](const TimelineClip& clip)
                               {
                                   return clip.trackIndex == trackIndex;
                               }),
                clips.end());

    for (auto& clip : clips)
        if (clip.trackIndex > trackIndex)
            --clip.trackIndex;

    // Same reindexing clips already get above: a child pointing past the removed track shifts
    // down with it, and a child of the removed track itself falls back to top-level rather than
    // pointing at a now-stale (or worse, silently wrong) index.
    for (auto& track : tracks)
    {
        if (track.parentTrackIndex == trackIndex)
            track.parentTrackIndex = -1;
        else if (track.parentTrackIndex > trackIndex)
            --track.parentTrackIndex;
    }

    activeRecordingClips.clear();
}

bool TimelineModel::analyzeClipWaveform(int clipIndex, juce::String& errorMessage)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return false;

    auto& clip = clips[(size_t) clipIndex];
    clip.peaks.clear();
    clip.rightPeaks.clear();
    clip.sourceNumChannels = 0;

    if (! clip.file.existsAsFile())
    {
        errorMessage = "Recorded audio file does not exist yet.";
        return false;
    }

    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(clip.file));
    if (reader == nullptr)
    {
        errorMessage = "Could not read recorded audio for waveform display.";
        return false;
    }

    const auto fullDurationSeconds = reader->sampleRate > 0.0 ? static_cast<double>(reader->lengthInSamples) / reader->sampleRate
                                                              : clip.durationSeconds;
    clip.sourceDurationSeconds = fullDurationSeconds;
    clip.sourceNumChannels = (int) reader->numChannels;
    if (clip.durationSeconds <= 0.0)
        clip.durationSeconds = juce::jmax(0.0, fullDurationSeconds - clip.sourceStartSeconds);

    auto peakCount = juce::jlimit(32, 2048, static_cast<int>(fullDurationSeconds * 120.0));
    clip.peaks.resize((size_t) peakCount, 0.0f);
    if (reader->numChannels >= 2)
        clip.rightPeaks.resize((size_t) peakCount, 0.0f);

    juce::AudioBuffer<float> scratch((int) reader->numChannels, 4096);
    for (int peakIndex = 0; peakIndex < peakCount; ++peakIndex)
    {
        auto startSample = static_cast<juce::int64>((double) peakIndex * (double) reader->lengthInSamples / (double) peakCount);
        auto endSample = static_cast<juce::int64>((double) (peakIndex + 1) * (double) reader->lengthInSamples / (double) peakCount);
        auto samplesRemaining = static_cast<int>(juce::jmax<juce::int64>(1, endSample - startSample));
        auto readPosition = startSample;
        auto leftPeak = 0.0f;
        auto rightPeak = 0.0f;
        auto mergedPeak = 0.0f;

        while (samplesRemaining > 0)
        {
            auto samplesThisRead = juce::jmin(samplesRemaining, scratch.getNumSamples());
            scratch.clear();
            reader->read(&scratch, 0, samplesThisRead, readPosition, true, true);

            for (int channel = 0; channel < scratch.getNumChannels(); ++channel)
            {
                auto range = juce::FloatVectorOperations::findMinAndMax(scratch.getReadPointer(channel), samplesThisRead);
                auto channelPeak = juce::jmax(std::abs(range.getStart()), std::abs(range.getEnd()));
                mergedPeak = juce::jmax(mergedPeak, channelPeak);
                if (channel == 0)
                    leftPeak = juce::jmax(leftPeak, channelPeak);
                else if (channel == 1)
                    rightPeak = juce::jmax(rightPeak, channelPeak);
            }

            readPosition += samplesThisRead;
            samplesRemaining -= samplesThisRead;
        }

        clip.peaks[(size_t) peakIndex] = juce::jlimit(0.0f, 1.0f, clip.rightPeaks.empty() ? mergedPeak : leftPeak);
        if (! clip.rightPeaks.empty())
            clip.rightPeaks[(size_t) peakIndex] = juce::jlimit(0.0f, 1.0f, rightPeak);
    }

    return true;
}

const std::vector<MidiNoteEvent>* TimelineModel::getMidiNotes(int clipIndex) const
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return nullptr;

    return &clips[(size_t) clipIndex].midiNotes;
}

juce::String TimelineModel::addMidiNote(int clipIndex, int pitch, double startBeats, double lengthBeats, int velocity, int channel)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return {};

    MidiNoteEvent note;
    note.id = juce::Uuid().toString();
    note.pitch = juce::jlimit(0, 127, pitch);
    note.velocity = juce::jlimit(1, 127, velocity);
    note.startBeats = juce::jmax(0.0, startBeats);
    note.lengthBeats = juce::jmax(0.03125, lengthBeats);
    note.channel = juce::jlimit(1, 16, channel);

    clips[(size_t) clipIndex].midiNotes.push_back(note);
    return note.id;
}

bool TimelineModel::removeMidiNote(int clipIndex, const juce::String& noteId)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return false;

    auto& notes = clips[(size_t) clipIndex].midiNotes;
    auto it = std::find_if(notes.begin(), notes.end(), [&](const MidiNoteEvent& n) { return n.id == noteId; });
    if (it == notes.end())
        return false;

    notes.erase(it);
    return true;
}

bool TimelineModel::removeMidiNotes(int clipIndex, const juce::StringArray& noteIds)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return false;

    auto& notes = clips[(size_t) clipIndex].midiNotes;
    auto before = notes.size();
    notes.erase(std::remove_if(notes.begin(), notes.end(), [&](const MidiNoteEvent& n)
    {
        return noteIds.contains(n.id);
    }), notes.end());

    return notes.size() != before;
}

bool TimelineModel::updateMidiNote(int clipIndex, const juce::String& noteId, int pitch, double startBeats, double lengthBeats)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return false;

    auto& notes = clips[(size_t) clipIndex].midiNotes;
    auto it = std::find_if(notes.begin(), notes.end(), [&](const MidiNoteEvent& n) { return n.id == noteId; });
    if (it == notes.end())
        return false;

    it->pitch = juce::jlimit(0, 127, pitch);
    it->startBeats = juce::jmax(0.0, startBeats);
    it->lengthBeats = juce::jmax(0.03125, lengthBeats);
    return true;
}

bool TimelineModel::setMidiNoteVelocity(int clipIndex, const juce::String& noteId, int velocity)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return false;

    auto& notes = clips[(size_t) clipIndex].midiNotes;
    auto it = std::find_if(notes.begin(), notes.end(), [&](const MidiNoteEvent& n) { return n.id == noteId; });
    if (it == notes.end())
        return false;

    it->velocity = juce::jlimit(1, 127, velocity);
    return true;
}

void TimelineModel::quantizeMidiNotes(int clipIndex, const juce::StringArray& noteIds, double gridBeats, float strength)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())) || gridBeats <= 0.0)
        return;

    auto clampedStrength = juce::jlimit(0.0f, 1.0f, strength);
    auto& notes = clips[(size_t) clipIndex].midiNotes;
    for (auto& note : notes)
    {
        if (! noteIds.isEmpty() && ! noteIds.contains(note.id))
            continue;

        auto nearestGrid = std::round(note.startBeats / gridBeats) * gridBeats;
        note.startBeats = note.startBeats + (nearestGrid - note.startBeats) * (double) clampedStrength;
        note.startBeats = juce::jmax(0.0, note.startBeats);
    }
}

void TimelineModel::quantizeMidiNoteVelocities(int clipIndex, const juce::StringArray& noteIds, int targetVelocity, float strength)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return;

    auto clampedStrength = juce::jlimit(0.0f, 1.0f, strength);
    auto clampedTarget = juce::jlimit(1, 127, targetVelocity);
    auto& notes = clips[(size_t) clipIndex].midiNotes;
    for (auto& note : notes)
    {
        if (! noteIds.isEmpty() && ! noteIds.contains(note.id))
            continue;

        auto newVelocity = note.velocity + ((double) clampedTarget - note.velocity) * (double) clampedStrength;
        note.velocity = juce::jlimit(1, 127, (int) std::round(newVelocity));
    }
}

void TimelineModel::applySwing(int clipIndex, const juce::StringArray& noteIds, double gridBeats, float swingAmount)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())) || gridBeats <= 0.0)
        return;

    auto clampedSwing = juce::jlimit(-1.0f, 1.0f, swingAmount);
    auto& notes = clips[(size_t) clipIndex].midiNotes;
    for (auto& note : notes)
    {
        if (! noteIds.isEmpty() && ! noteIds.contains(note.id))
            continue;

        auto gridIndex = (long long) std::round(note.startBeats / gridBeats);
        if (gridIndex % 2 == 0)
            continue;

        note.startBeats = juce::jmax(0.0, note.startBeats + (double) clampedSwing * gridBeats * 0.5);
    }
}

void TimelineModel::humanizeMidiNotes(int clipIndex, const juce::StringArray& noteIds, double timingAmountBeats, int velocityAmount, float lengthAmountPercent)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return;

    auto& random = juce::Random::getSystemRandom();
    auto& notes = clips[(size_t) clipIndex].midiNotes;
    for (auto& note : notes)
    {
        if (! noteIds.isEmpty() && ! noteIds.contains(note.id))
            continue;

        if (timingAmountBeats > 0.0)
        {
            auto offset = (random.nextDouble() * 2.0 - 1.0) * timingAmountBeats;
            note.startBeats = juce::jmax(0.0, note.startBeats + offset);
        }

        if (velocityAmount > 0)
        {
            auto offset = random.nextInt(velocityAmount * 2 + 1) - velocityAmount;
            note.velocity = juce::jlimit(1, 127, note.velocity + offset);
        }

        if (lengthAmountPercent > 0.0f)
        {
            auto factor = 1.0 + (random.nextDouble() * 2.0 - 1.0) * (double) lengthAmountPercent;
            note.lengthBeats = juce::jmax(0.03125, note.lengthBeats * factor);
        }
    }
}

const std::vector<MidiCCEvent>* TimelineModel::getMidiCC(int clipIndex) const
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return nullptr;

    return &clips[(size_t) clipIndex].midiCC;
}

juce::String TimelineModel::addOrUpdateMidiCCPoint(int clipIndex, int controller, double beats, int value, double mergeToleranceBeats)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return {};

    auto clampedController = juce::jlimit(0, 127, controller);
    auto clampedValue = juce::jlimit(0, 127, value);
    auto clampedBeats = juce::jmax(0.0, beats);

    auto& ccEvents = clips[(size_t) clipIndex].midiCC;
    for (auto& point : ccEvents)
    {
        if (point.controller == clampedController && std::abs(point.beats - clampedBeats) <= mergeToleranceBeats)
        {
            point.value = clampedValue;
            return point.id;
        }
    }

    MidiCCEvent point;
    point.id = juce::Uuid().toString();
    point.controller = clampedController;
    point.value = clampedValue;
    point.beats = clampedBeats;
    ccEvents.push_back(point);

    std::sort(ccEvents.begin(), ccEvents.end(), [](const MidiCCEvent& a, const MidiCCEvent& b)
    {
        return a.beats < b.beats;
    });

    return point.id;
}

bool TimelineModel::removeMidiCCPoint(int clipIndex, const juce::String& pointId)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return false;

    auto& ccEvents = clips[(size_t) clipIndex].midiCC;
    auto it = std::find_if(ccEvents.begin(), ccEvents.end(), [&](const MidiCCEvent& p) { return p.id == pointId; });
    if (it == ccEvents.end())
        return false;

    ccEvents.erase(it);
    return true;
}

void TimelineModel::clearMidiCCLane(int clipIndex, int controller)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return;

    auto& ccEvents = clips[(size_t) clipIndex].midiCC;
    ccEvents.erase(std::remove_if(ccEvents.begin(), ccEvents.end(), [&](const MidiCCEvent& p)
    {
        return p.controller == controller;
    }), ccEvents.end());
}

int TimelineModel::getAutomationClipIndex(int trackIndex) const
{
    for (size_t i = 0; i < clips.size(); ++i)
        if (clips[i].trackIndex == trackIndex && clips[i].kind == ClipKind::automation)
            return static_cast<int>(i);

    return -1;
}

void TimelineModel::ensureAutomationClip(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return;

    if (getAutomationClipIndex(trackIndex) >= 0)
        return;

    TimelineClip clip;
    clip.id = juce::Uuid().toString();
    clip.kind = ClipKind::automation;
    clip.displayName = "Automation";
    clip.trackIndex = trackIndex;
    clip.startSeconds = 0.0;
    clip.durationSeconds = 0.05;
    clip.recording = false;
    clips.push_back(std::move(clip));
}

const std::vector<AutomationPoint>* TimelineModel::getAutomationPoints(int trackIndex) const
{
    auto clipIndex = getAutomationClipIndex(trackIndex);
    if (clipIndex < 0)
        return nullptr;

    return &clips[(size_t) clipIndex].automationPoints;
}

juce::String TimelineModel::addOrUpdateAutomationPoint(int trackIndex, double seconds, float value, double mergeToleranceSeconds)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return {};

    ensureAutomationClip(trackIndex);
    auto clipIndex = getAutomationClipIndex(trackIndex);
    if (clipIndex < 0)
        return {};

    auto clampedValue = juce::jlimit(0.0f, 1.0f, value);
    auto clampedSeconds = juce::jmax(0.0, seconds);

    auto& points = clips[(size_t) clipIndex].automationPoints;
    for (auto& point : points)
    {
        if (std::abs(point.seconds - clampedSeconds) <= mergeToleranceSeconds)
        {
            point.value = clampedValue;
            return point.id;
        }
    }

    AutomationPoint point;
    point.id = juce::Uuid().toString();
    point.seconds = clampedSeconds;
    point.value = clampedValue;
    points.push_back(point);

    std::sort(points.begin(), points.end(), [](const AutomationPoint& a, const AutomationPoint& b)
    {
        return a.seconds < b.seconds;
    });

    return point.id;
}

bool TimelineModel::moveAutomationPoint(int trackIndex, const juce::String& pointId, double newSeconds, float newValue)
{
    auto clipIndex = getAutomationClipIndex(trackIndex);
    if (clipIndex < 0)
        return false;

    auto& points = clips[(size_t) clipIndex].automationPoints;
    auto it = std::find_if(points.begin(), points.end(), [&](const AutomationPoint& p) { return p.id == pointId; });
    if (it == points.end())
        return false;

    it->seconds = juce::jmax(0.0, newSeconds);
    it->value = juce::jlimit(0.0f, 1.0f, newValue);

    std::sort(points.begin(), points.end(), [](const AutomationPoint& a, const AutomationPoint& b)
    {
        return a.seconds < b.seconds;
    });

    return true;
}

bool TimelineModel::setAutomationPointShape(int trackIndex, const juce::String& pointId, AutomationCurveShape shape, float tension)
{
    auto clipIndex = getAutomationClipIndex(trackIndex);
    if (clipIndex < 0)
        return false;

    auto& points = clips[(size_t) clipIndex].automationPoints;
    auto it = std::find_if(points.begin(), points.end(), [&](const AutomationPoint& p) { return p.id == pointId; });
    if (it == points.end())
        return false;

    it->shape = shape;
    it->tension = juce::jlimit(-1.0f, 1.0f, tension);
    return true;
}

bool TimelineModel::removeAutomationPoint(int trackIndex, const juce::String& pointId)
{
    auto clipIndex = getAutomationClipIndex(trackIndex);
    if (clipIndex < 0)
        return false;

    auto& points = clips[(size_t) clipIndex].automationPoints;
    auto it = std::find_if(points.begin(), points.end(), [&](const AutomationPoint& p) { return p.id == pointId; });
    if (it == points.end())
        return false;

    points.erase(it);
    return true;
}

void TimelineModel::clearAutomationLane(int trackIndex)
{
    auto clipIndex = getAutomationClipIndex(trackIndex);
    if (clipIndex < 0)
        return;

    clips[(size_t) clipIndex].automationPoints.clear();
}

float TimelineModel::evaluateAutomationValue(int trackIndex, double seconds) const
{
    auto clipIndex = getAutomationClipIndex(trackIndex);
    if (clipIndex < 0)
        return 0.5f;

    const auto& points = clips[(size_t) clipIndex].automationPoints;
    if (points.empty())
        return 0.5f;

    if (seconds <= points.front().seconds)
        return points.front().value;

    if (seconds >= points.back().seconds)
        return points.back().value;

    for (size_t i = 0; i + 1 < points.size(); ++i)
        if (seconds >= points[i].seconds && seconds <= points[i + 1].seconds)
            return evaluateAutomationSegment(points[i], points[i + 1], seconds);

    return points.back().value;
}

void TimelineModel::setAutomationTarget(int trackIndex, const AutomationTarget& target)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return;

    tracks[(size_t) trackIndex].automationTarget = target;
}

const AutomationTarget& TimelineModel::getAutomationTarget(int trackIndex) const
{
    static const AutomationTarget empty;
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return empty;

    return tracks[(size_t) trackIndex].automationTarget;
}

void TimelineModel::setAutomationRecordMode(int trackIndex, AutomationRecordMode mode)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return;

    tracks[(size_t) trackIndex].automationRecordMode = mode;
}

AutomationRecordMode TimelineModel::getAutomationRecordMode(int trackIndex) const
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return AutomationRecordMode::touch;

    return tracks[(size_t) trackIndex].automationRecordMode;
}

void TimelineModel::setAutomationRecordingRate(int trackIndex, int pointsPerSecond)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return;

    tracks[(size_t) trackIndex].automationRecordingPointsPerSecond = juce::jlimit(1, 120, pointsPerSecond);
}

int TimelineModel::getAutomationRecordingRate(int trackIndex) const
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return 10;

    return tracks[(size_t) trackIndex].automationRecordingPointsPerSecond;
}

juce::ValueTree TimelineModel::createState() const
{
    juce::ValueTree state("Timeline");
    state.setProperty("tempoBpm", tempoBpm, nullptr);
    state.setProperty("timeSignatureNumerator", timeSignatureNumerator, nullptr);
    state.setProperty("timeSignatureDenominator", timeSignatureDenominator, nullptr);
    state.setProperty("musicalKey", musicalKey, nullptr);
    state.setProperty("pixelsPerSecond", pixelsPerSecond, nullptr);
    state.setProperty("transportSeconds", transportSeconds, nullptr);
    state.setProperty("loopStartSeconds", loopStartSeconds, nullptr);
    state.setProperty("loopEndSeconds", loopEndSeconds, nullptr);
    state.setProperty("loopEnabled", loopEnabled, nullptr);
    state.setProperty("timelineSnapEnabled", timelineSnapEnabled, nullptr);
    state.setProperty("timelineGridBeats", timelineGridBeats, nullptr);

    juce::ValueTree markersState("Markers");
    for (const auto& marker : markers)
    {
        juce::ValueTree markerState("Marker");
        markerState.setProperty("id", marker.id, nullptr);
        markerState.setProperty("name", marker.name, nullptr);
        markerState.setProperty("seconds", marker.seconds, nullptr);
        markersState.addChild(markerState, -1, nullptr);
    }
    state.addChild(markersState, -1, nullptr);

    juce::ValueTree tracksState("Tracks");
    for (const auto& track : tracks)
    {
        juce::ValueTree trackState("Track");
        trackState.setProperty("id", track.id, nullptr);
        trackState.setProperty("name", track.name, nullptr);
        trackState.setProperty("kind", toStorageToken(track.kind), nullptr);
        trackState.setProperty("channelMode", toStorageToken(track.channelMode), nullptr);
        trackState.setProperty("parentTrackIndex", track.parentTrackIndex, nullptr);
        trackState.setProperty("folded", track.folded, nullptr);
        trackState.setProperty("automationTargetKind", toStorageToken(track.automationTarget.kind), nullptr);
        trackState.setProperty("automationTargetTrackIndex", track.automationTarget.targetTrackIndex, nullptr);
        trackState.setProperty("automationTargetPluginSlot", track.automationTarget.pluginSlotIndex, nullptr);
        trackState.setProperty("automationTargetParameterId", track.automationTarget.parameterId, nullptr);
        trackState.setProperty("automationTargetParameterIndex", track.automationTarget.pluginParameterIndex, nullptr);
        trackState.setProperty("automationTargetDisplayName", track.automationTarget.displayName, nullptr);
        trackState.setProperty("automationRecordMode", toStorageToken(track.automationRecordMode), nullptr);
        trackState.setProperty("automationRecordingPointsPerSecond", track.automationRecordingPointsPerSecond, nullptr);
        tracksState.addChild(trackState, -1, nullptr);
    }
    state.addChild(tracksState, -1, nullptr);

    juce::ValueTree clipsState("Clips");
    for (const auto& clip : clips)
    {
        juce::ValueTree clipState("Clip");
        clipState.setProperty("id", clip.id, nullptr);
        clipState.setProperty("kind", toStorageToken(clip.kind), nullptr);
        clipState.setProperty("displayName", clip.displayName, nullptr);
        clipState.setProperty("assetId", clip.assetId, nullptr);
        clipState.setProperty("assetVersionId", clip.assetVersionId, nullptr);
        clipState.setProperty("assetReferenceMode", cs::toStorageToken(clip.assetReferenceMode), nullptr);
        clipState.setProperty("sourceTool", clip.sourceTool, nullptr);
        clipState.setProperty("trackIndex", clip.trackIndex, nullptr);
        clipState.setProperty("file", clip.file.getFullPathName(), nullptr);
        clipState.setProperty("startSeconds", clip.startSeconds, nullptr);
        clipState.setProperty("durationSeconds", clip.durationSeconds, nullptr);
        clipState.setProperty("sourceStartSeconds", clip.sourceStartSeconds, nullptr);
        clipState.setProperty("sourceDurationSeconds", clip.sourceDurationSeconds, nullptr);
        clipState.setProperty("recording", false, nullptr);

        if (! clip.midiNotes.empty())
        {
            juce::ValueTree notesState("MidiNotes");
            for (const auto& note : clip.midiNotes)
            {
                juce::ValueTree noteState("Note");
                noteState.setProperty("id", note.id, nullptr);
                noteState.setProperty("pitch", note.pitch, nullptr);
                noteState.setProperty("velocity", note.velocity, nullptr);
                noteState.setProperty("startBeats", note.startBeats, nullptr);
                noteState.setProperty("lengthBeats", note.lengthBeats, nullptr);
                noteState.setProperty("channel", note.channel, nullptr);
                noteState.setProperty("muted", note.muted, nullptr);
                notesState.addChild(noteState, -1, nullptr);
            }
            clipState.addChild(notesState, -1, nullptr);
        }

        if (! clip.midiCC.empty())
        {
            juce::ValueTree ccState("MidiCC");
            for (const auto& point : clip.midiCC)
            {
                juce::ValueTree pointState("Point");
                pointState.setProperty("id", point.id, nullptr);
                pointState.setProperty("controller", point.controller, nullptr);
                pointState.setProperty("value", point.value, nullptr);
                pointState.setProperty("beats", point.beats, nullptr);
                ccState.addChild(pointState, -1, nullptr);
            }
            clipState.addChild(ccState, -1, nullptr);
        }

        if (! clip.automationPoints.empty())
        {
            juce::ValueTree automationState("AutomationPoints");
            for (const auto& point : clip.automationPoints)
            {
                juce::ValueTree pointState("Point");
                pointState.setProperty("id", point.id, nullptr);
                pointState.setProperty("seconds", point.seconds, nullptr);
                pointState.setProperty("value", (double) point.value, nullptr);
                pointState.setProperty("shape", toStorageToken(point.shape), nullptr);
                pointState.setProperty("tension", (double) point.tension, nullptr);
                automationState.addChild(pointState, -1, nullptr);
            }
            clipState.addChild(automationState, -1, nullptr);
        }

        clipsState.addChild(clipState, -1, nullptr);
    }

    state.addChild(clipsState, -1, nullptr);
    return state;
}

void TimelineModel::restoreState(const juce::ValueTree& state)
{
    clear();

    if (! state.isValid() || state.getType() != juce::Identifier("Timeline"))
        return;

    setTempo((double) state.getProperty("tempoBpm", tempoBpm),
             (int) state.getProperty("timeSignatureNumerator", timeSignatureNumerator),
             (int) state.getProperty("timeSignatureDenominator", timeSignatureDenominator));
    setMusicalKey(state.getProperty("musicalKey", musicalKey).toString());
    setPixelsPerSecond((double) state.getProperty("pixelsPerSecond", pixelsPerSecond));
    setTransportSeconds((double) state.getProperty("transportSeconds", 0.0));
    loopStartSeconds = (double) state.getProperty("loopStartSeconds", 0.0);
    loopEndSeconds = (double) state.getProperty("loopEndSeconds", 0.0);
    loopEnabled = (bool) state.getProperty("loopEnabled", false);
    timelineSnapEnabled = (bool) state.getProperty("timelineSnapEnabled", true);
    timelineGridBeats = juce::jmax(0.0078125, (double) state.getProperty("timelineGridBeats", 1.0));

    auto markersState = state.getChildWithName("Markers");
    if (markersState.isValid())
    {
        for (const auto child : markersState)
        {
            if (! child.hasType("Marker"))
                continue;

            TimelineMarker marker;
            marker.id = child.getProperty("id").toString();
            if (marker.id.isEmpty())
                marker.id = juce::Uuid().toString();
            marker.name = child.getProperty("name").toString();
            marker.seconds = (double) child.getProperty("seconds", 0.0);
            markers.push_back(std::move(marker));
        }
    }

    auto tracksState = state.getChildWithName("Tracks");
    if (tracksState.isValid())
    {
        for (const auto child : tracksState)
        {
            if (! child.hasType("Track"))
                continue;

            TimelineTrack track;
            track.id = child.getProperty("id").toString();
            if (track.id.isEmpty())
                track.id = juce::Uuid().toString();
            track.name = child.getProperty("name").toString();
            track.kind = trackKindFromStorageToken(child.getProperty("kind", "audio").toString());
            track.channelMode = trackChannelModeFromStorageToken(child.getProperty("channelMode", "mono").toString());
            track.parentTrackIndex = (int) child.getProperty("parentTrackIndex", -1);
            track.folded = (bool) child.getProperty("folded", false);
            track.automationTarget.kind = automationTargetKindFromStorageToken(child.getProperty("automationTargetKind", "none").toString());
            track.automationTarget.targetTrackIndex = (int) child.getProperty("automationTargetTrackIndex", -1);
            track.automationTarget.pluginSlotIndex = (int) child.getProperty("automationTargetPluginSlot", -1);
            track.automationTarget.parameterId = child.getProperty("automationTargetParameterId").toString();
            track.automationTarget.pluginParameterIndex = (int) child.getProperty("automationTargetParameterIndex", -1);
            track.automationTarget.displayName = child.getProperty("automationTargetDisplayName").toString();
            track.automationRecordMode = automationRecordModeFromStorageToken(child.getProperty("automationRecordMode", "touch").toString());
            track.automationRecordingPointsPerSecond = juce::jlimit(1, 120, (int) child.getProperty("automationRecordingPointsPerSecond", 10));
            if (track.name.trim().isEmpty())
                track.name = makeDefaultTrackName(static_cast<int>(tracks.size()));
            tracks.push_back(std::move(track));
        }
    }

    auto clipsState = state.getChildWithName("Clips");
    if (! clipsState.isValid())
        return;

    for (const auto child : clipsState)
    {
        if (! child.hasType("Clip"))
            continue;

        TimelineClip clip;
        clip.id = child.getProperty("id").toString();
        if (clip.id.isEmpty())
            clip.id = juce::Uuid().toString();

        clip.kind = clipKindFromStorageToken(child.getProperty("kind", "audio").toString());
        clip.displayName = child.getProperty("displayName").toString();
        clip.assetId = child.getProperty("assetId").toString();
        clip.assetVersionId = child.getProperty("assetVersionId").toString();
        clip.assetReferenceMode = cs::assetReferenceModeFromStorageToken(child.getProperty("assetReferenceMode", "exact").toString());
        clip.sourceTool = child.getProperty("sourceTool").toString();
        clip.trackIndex = (int) child.getProperty("trackIndex", -1);
        clip.file = juce::File(child.getProperty("file").toString());
        clip.startSeconds = (double) child.getProperty("startSeconds", 0.0);
        clip.durationSeconds = (double) child.getProperty("durationSeconds", 0.0);
        clip.sourceStartSeconds = (double) child.getProperty("sourceStartSeconds", 0.0);
        clip.sourceDurationSeconds = (double) child.getProperty("sourceDurationSeconds", 0.0);
        clip.recording = false;
        if (clip.displayName.trim().isEmpty())
            clip.displayName = clip.file.existsAsFile() ? clip.file.getFileNameWithoutExtension()
                                                        : toDisplayName(clip.kind) + " Clip";

        auto notesState = child.getChildWithName("MidiNotes");
        if (notesState.isValid())
        {
            for (const auto noteChild : notesState)
            {
                if (! noteChild.hasType("Note"))
                    continue;

                MidiNoteEvent note;
                note.id = noteChild.getProperty("id").toString();
                if (note.id.isEmpty())
                    note.id = juce::Uuid().toString();
                note.pitch = (int) noteChild.getProperty("pitch", 60);
                note.velocity = (int) noteChild.getProperty("velocity", 100);
                note.startBeats = (double) noteChild.getProperty("startBeats", 0.0);
                note.lengthBeats = (double) noteChild.getProperty("lengthBeats", 1.0);
                note.channel = (int) noteChild.getProperty("channel", 1);
                note.muted = (bool) noteChild.getProperty("muted", false);
                clip.midiNotes.push_back(std::move(note));
            }
        }

        auto ccState = child.getChildWithName("MidiCC");
        if (ccState.isValid())
        {
            for (const auto pointChild : ccState)
            {
                if (! pointChild.hasType("Point"))
                    continue;

                MidiCCEvent point;
                point.id = pointChild.getProperty("id").toString();
                if (point.id.isEmpty())
                    point.id = juce::Uuid().toString();
                point.controller = (int) pointChild.getProperty("controller", 1);
                point.value = (int) pointChild.getProperty("value", 0);
                point.beats = (double) pointChild.getProperty("beats", 0.0);
                clip.midiCC.push_back(std::move(point));
            }
        }

        auto automationState = child.getChildWithName("AutomationPoints");
        if (automationState.isValid())
        {
            for (const auto pointChild : automationState)
            {
                if (! pointChild.hasType("Point"))
                    continue;

                AutomationPoint point;
                point.id = pointChild.getProperty("id").toString();
                if (point.id.isEmpty())
                    point.id = juce::Uuid().toString();
                point.seconds = (double) pointChild.getProperty("seconds", 0.0);
                point.value = (float) (double) pointChild.getProperty("value", 0.5);
                point.shape = automationCurveShapeFromStorageToken(pointChild.getProperty("shape", "linear").toString());
                point.tension = (float) (double) pointChild.getProperty("tension", 0.0);
                clip.automationPoints.push_back(std::move(point));
            }
        }

        clips.push_back(std::move(clip));

        if (clips.back().trackIndex >= 0 && ! juce::isPositiveAndBelow(clips.back().trackIndex, getTrackCount()))
            setTrackCount(clips.back().trackIndex + 1);

        juce::String errorMessage;
        analyzeClipWaveform(static_cast<int>(clips.size()) - 1, errorMessage);
    }
}
}
