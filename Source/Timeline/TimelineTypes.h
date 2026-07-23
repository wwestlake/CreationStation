#pragma once

#include <JuceHeader.h>

namespace cs
{
enum class TrackKind
{
    audio,
    midi,
    automation,
    signal,
    foley,
    folder,
    marker
};

enum class ClipKind
{
    audio,
    midi,
    automation,
    signal,
    foley,
    marker
};

enum class TrackChannelMode
{
    mono,
    stereo
};

struct TimelineTrack
{
    juce::String id;
    juce::String name;
    TrackKind kind = TrackKind::audio;
    TrackChannelMode channelMode = TrackChannelMode::mono;
    int parentTrackIndex = -1;
    bool folded = false;
};

struct TimelineMarker
{
    juce::String id;
    juce::String name;
    double seconds = 0.0;
};

inline juce::String toStorageToken(TrackKind kind)
{
    switch (kind)
    {
        case TrackKind::audio: return "audio";
        case TrackKind::midi: return "midi";
        case TrackKind::automation: return "automation";
        case TrackKind::signal: return "signal";
        case TrackKind::foley: return "foley";
        case TrackKind::folder: return "folder";
        case TrackKind::marker: return "marker";
    }

    return "audio";
}

inline juce::String toStorageToken(ClipKind kind)
{
    switch (kind)
    {
        case ClipKind::audio: return "audio";
        case ClipKind::midi: return "midi";
        case ClipKind::automation: return "automation";
        case ClipKind::signal: return "signal";
        case ClipKind::foley: return "foley";
        case ClipKind::marker: return "marker";
    }

    return "audio";
}

inline juce::String toStorageToken(TrackChannelMode mode)
{
    switch (mode)
    {
        case TrackChannelMode::mono: return "mono";
        case TrackChannelMode::stereo: return "stereo";
    }

    return "mono";
}

inline TrackKind trackKindFromStorageToken(const juce::String& token)
{
    auto normalized = token.trim().toLowerCase();
    if (normalized == "midi") return TrackKind::midi;
    if (normalized == "automation") return TrackKind::automation;
    if (normalized == "signal") return TrackKind::signal;
    if (normalized == "foley") return TrackKind::foley;
    if (normalized == "folder") return TrackKind::folder;
    if (normalized == "marker") return TrackKind::marker;
    return TrackKind::audio;
}

inline ClipKind clipKindFromStorageToken(const juce::String& token)
{
    auto normalized = token.trim().toLowerCase();
    if (normalized == "midi") return ClipKind::midi;
    if (normalized == "automation") return ClipKind::automation;
    if (normalized == "signal") return ClipKind::signal;
    if (normalized == "foley") return ClipKind::foley;
    if (normalized == "marker") return ClipKind::marker;
    return ClipKind::audio;
}

inline TrackChannelMode trackChannelModeFromStorageToken(const juce::String& token)
{
    return token.trim().toLowerCase() == "stereo" ? TrackChannelMode::stereo
                                                  : TrackChannelMode::mono;
}

inline juce::String toDisplayName(TrackKind kind)
{
    switch (kind)
    {
        case TrackKind::audio: return "Audio";
        case TrackKind::midi: return "MIDI";
        case TrackKind::automation: return "Automation";
        case TrackKind::signal: return "Signal";
        case TrackKind::foley: return "Foley";
        case TrackKind::folder: return "Folder";
        case TrackKind::marker: return "Marker";
    }

    return "Audio";
}

inline juce::String toDisplayName(ClipKind kind)
{
    switch (kind)
    {
        case ClipKind::audio: return "Audio";
        case ClipKind::midi: return "MIDI";
        case ClipKind::automation: return "Automation";
        case ClipKind::signal: return "Signal";
        case ClipKind::foley: return "Foley";
        case ClipKind::marker: return "Marker";
    }

    return "Audio";
}

inline ClipKind defaultClipKindForTrack(TrackKind kind)
{
    switch (kind)
    {
        case TrackKind::midi: return ClipKind::midi;
        case TrackKind::automation: return ClipKind::automation;
        case TrackKind::signal: return ClipKind::signal;
        case TrackKind::foley: return ClipKind::foley;
        case TrackKind::marker: return ClipKind::marker;
        case TrackKind::folder:
        case TrackKind::audio:
            return ClipKind::audio;
    }

    return ClipKind::audio;
}

inline bool canTrackContainClip(TrackKind trackKind, ClipKind clipKind)
{
    switch (trackKind)
    {
        case TrackKind::audio:
            return clipKind == ClipKind::audio || clipKind == ClipKind::foley;
        case TrackKind::midi:
            return clipKind == ClipKind::midi;
        case TrackKind::automation:
            return clipKind == ClipKind::automation;
        case TrackKind::signal:
            return clipKind == ClipKind::signal || clipKind == ClipKind::audio;
        case TrackKind::foley:
            return clipKind == ClipKind::foley || clipKind == ClipKind::audio;
        case TrackKind::marker:
            return clipKind == ClipKind::marker;
        case TrackKind::folder:
            return false;
    }

    return false;
}
}
