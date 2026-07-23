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
    pixelsPerSecond = juce::jlimit(30.0, 900.0, newPixelsPerSecond);
}

void TimelineModel::zoomIn()
{
    setPixelsPerSecond(pixelsPerSecond * 1.35);
}

void TimelineModel::zoomOut()
{
    setPixelsPerSecond(pixelsPerSecond / 1.35);
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
    clip.recording = true;

    clips.push_back(std::move(clip));
    auto clipIndex = static_cast<int>(clips.size()) - 1;
    activeRecordingClips.push_back(clipIndex);

    if (activeRecordingClips.size() == 1)
        recordingStartSeconds = transportSeconds;

    return clipIndex;
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

        juce::String errorMessage;
        analyzeClipWaveform(clipIndex, errorMessage);
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
    clip.durationSeconds = juce::jmax(0.05, durationSeconds);
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

void TimelineModel::setTrackKind(int trackIndex, TrackKind kind)
{
    if (! juce::isPositiveAndBelow(trackIndex, getTrackCount()))
        return;

    tracks[(size_t) trackIndex].kind = kind;
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

    activeRecordingClips.clear();
}

bool TimelineModel::analyzeClipWaveform(int clipIndex, juce::String& errorMessage)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(clips.size())))
        return false;

    auto& clip = clips[(size_t) clipIndex];
    clip.peaks.clear();

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
    if (clip.durationSeconds <= 0.0)
        clip.durationSeconds = juce::jmax(0.0, fullDurationSeconds - clip.sourceStartSeconds);

    auto peakCount = juce::jlimit(32, 2048, static_cast<int>(fullDurationSeconds * 120.0));
    clip.peaks.resize((size_t) peakCount, 0.0f);

    juce::AudioBuffer<float> scratch((int) reader->numChannels, 4096);
    for (int peakIndex = 0; peakIndex < peakCount; ++peakIndex)
    {
        auto startSample = static_cast<juce::int64>((double) peakIndex * (double) reader->lengthInSamples / (double) peakCount);
        auto endSample = static_cast<juce::int64>((double) (peakIndex + 1) * (double) reader->lengthInSamples / (double) peakCount);
        auto samplesRemaining = static_cast<int>(juce::jmax<juce::int64>(1, endSample - startSample));
        auto readPosition = startSample;
        auto peak = 0.0f;

        while (samplesRemaining > 0)
        {
            auto samplesThisRead = juce::jmin(samplesRemaining, scratch.getNumSamples());
            scratch.clear();
            reader->read(&scratch, 0, samplesThisRead, readPosition, true, true);

            for (int channel = 0; channel < scratch.getNumChannels(); ++channel)
            {
                auto range = juce::FloatVectorOperations::findMinAndMax(scratch.getReadPointer(channel), samplesThisRead);
                peak = juce::jmax(peak, std::abs(range.getStart()), std::abs(range.getEnd()));
            }

            readPosition += samplesThisRead;
            samplesRemaining -= samplesThisRead;
        }

        clip.peaks[(size_t) peakIndex] = juce::jlimit(0.0f, 1.0f, peak);
    }

    return true;
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
        clipState.setProperty("sourceTool", clip.sourceTool, nullptr);
        clipState.setProperty("trackIndex", clip.trackIndex, nullptr);
        clipState.setProperty("file", clip.file.getFullPathName(), nullptr);
        clipState.setProperty("startSeconds", clip.startSeconds, nullptr);
        clipState.setProperty("durationSeconds", clip.durationSeconds, nullptr);
        clipState.setProperty("sourceStartSeconds", clip.sourceStartSeconds, nullptr);
        clipState.setProperty("sourceDurationSeconds", clip.sourceDurationSeconds, nullptr);
        clipState.setProperty("recording", false, nullptr);
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
        clips.push_back(std::move(clip));

        if (clips.back().trackIndex >= 0 && ! juce::isPositiveAndBelow(clips.back().trackIndex, getTrackCount()))
            setTrackCount(clips.back().trackIndex + 1);

        juce::String errorMessage;
        analyzeClipWaveform(static_cast<int>(clips.size()) - 1, errorMessage);
    }
}
}
