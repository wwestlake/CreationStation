#pragma once

#include <JuceHeader.h>
#include <cmath>

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

enum class AutomationCurveShape
{
    linear,
    step,
    sCurve,
    power,
    bezier
};

// Describes the segment leading OUT of this point to the next one (Reaper convention).
// value is normalized 0..1, matching juce::AudioProcessorParameter's convention, so a lane's
// drawn shape is meaningful no matter which control it is currently targeting.
struct AutomationPoint
{
    juce::String id;
    double seconds = 0.0;
    float value = 0.5f;
    AutomationCurveShape shape = AutomationCurveShape::linear;
    float tension = 0.0f; // -1..1, meaningful only for power/bezier
};

enum class AutomationTargetKind
{
    none,
    trackVolume,
    trackPan,
    pluginParameter,
    pluginBypass
};

enum class AutomationValueMode
{
    continuous,
    toggle,
    stepped
};

struct AutomationTarget
{
    AutomationTargetKind kind = AutomationTargetKind::none;
    int targetTrackIndex = -1;
    int pluginSlotIndex = -1;
    // parameterId is kept for display/persistence robustness (re-matching a parameter by name
    // if plugin parameter order ever shifts on reload); pluginParameterIndex is what the
    // realtime audio-thread automation pass actually uses, since resolving a parameter by ID
    // string is message-thread work, not something to do inside the audio callback.
    juce::String parameterId;
    int pluginParameterIndex = -1;
    juce::String displayName;
    AutomationValueMode valueMode = AutomationValueMode::continuous;
    int stepCount = 0; // 0/1 = not stepped, 2 = on/off, N = N discrete choices
};

// How arming an automation track for recording behaves once you manually move its target's
// control during playback ("riding the fader"):
//  - touch: records only while actively moved; on release, subsequent playback resumes reading
//    whatever the curve already has from that point onward.
//  - latch: same as touch on first move, but once engaged it keeps recording (holding the last
//    value if untouched) for the rest of the pass, ignoring any pre-existing points beyond it.
//  - write: records continuously for the whole pass from the moment playback starts, whether
//    touched or not, overwriting existing automation as it goes.
enum class AutomationRecordMode
{
    touch,
    latch,
    write
};

struct TimelineTrack
{
    juce::String id;
    juce::String name;
    TrackKind kind = TrackKind::audio;
    TrackChannelMode channelMode = TrackChannelMode::mono;
    int parentTrackIndex = -1;
    bool folded = false;
    AutomationTarget automationTarget; // meaningful only when kind == automation
    AutomationRecordMode automationRecordMode = AutomationRecordMode::touch;
    // How many points/second get written while manually recording into this lane (Touch/Latch/
    // Write). Higher captures faster moves more precisely but bloats the curve with points a
    // human's own motion couldn't have made that finely; lower keeps the curve editable by hand.
    int automationRecordingPointsPerSecond = 10;
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

inline juce::String toStorageToken(AutomationCurveShape shape)
{
    switch (shape)
    {
        case AutomationCurveShape::linear: return "linear";
        case AutomationCurveShape::step: return "step";
        case AutomationCurveShape::sCurve: return "sCurve";
        case AutomationCurveShape::power: return "power";
        case AutomationCurveShape::bezier: return "bezier";
    }

    return "linear";
}

inline AutomationCurveShape automationCurveShapeFromStorageToken(const juce::String& token)
{
    auto normalized = token.trim();
    if (normalized.equalsIgnoreCase("step")) return AutomationCurveShape::step;
    if (normalized.equalsIgnoreCase("sCurve")) return AutomationCurveShape::sCurve;
    if (normalized.equalsIgnoreCase("power")) return AutomationCurveShape::power;
    if (normalized.equalsIgnoreCase("bezier")) return AutomationCurveShape::bezier;
    return AutomationCurveShape::linear;
}

inline juce::String toDisplayName(AutomationCurveShape shape)
{
    switch (shape)
    {
        case AutomationCurveShape::linear: return "Linear";
        case AutomationCurveShape::step: return "Step";
        case AutomationCurveShape::sCurve: return "S-Curve";
        case AutomationCurveShape::power: return "Power";
        case AutomationCurveShape::bezier: return "Bezier";
    }

    return "Linear";
}

inline juce::String toStorageToken(AutomationTargetKind kind)
{
    switch (kind)
    {
        case AutomationTargetKind::none: return "none";
        case AutomationTargetKind::trackVolume: return "trackVolume";
        case AutomationTargetKind::trackPan: return "trackPan";
        case AutomationTargetKind::pluginParameter: return "pluginParameter";
        case AutomationTargetKind::pluginBypass: return "pluginBypass";
    }

    return "none";
}

inline AutomationTargetKind automationTargetKindFromStorageToken(const juce::String& token)
{
    auto normalized = token.trim();
    if (normalized.equalsIgnoreCase("trackVolume")) return AutomationTargetKind::trackVolume;
    if (normalized.equalsIgnoreCase("trackPan")) return AutomationTargetKind::trackPan;
    if (normalized.equalsIgnoreCase("pluginParameter")) return AutomationTargetKind::pluginParameter;
    if (normalized.equalsIgnoreCase("pluginBypass")) return AutomationTargetKind::pluginBypass;
    return AutomationTargetKind::none;
}

inline juce::String toStorageToken(AutomationValueMode mode)
{
    switch (mode)
    {
        case AutomationValueMode::continuous: return "continuous";
        case AutomationValueMode::toggle: return "toggle";
        case AutomationValueMode::stepped: return "stepped";
    }

    return "continuous";
}

inline AutomationValueMode automationValueModeFromStorageToken(const juce::String& token)
{
    auto normalized = token.trim();
    if (normalized.equalsIgnoreCase("toggle")) return AutomationValueMode::toggle;
    if (normalized.equalsIgnoreCase("stepped")) return AutomationValueMode::stepped;
    return AutomationValueMode::continuous;
}

inline float quantizeAutomationValueForTarget(const AutomationTarget& target, float value)
{
    auto clampedValue = juce::jlimit(0.0f, 1.0f, value);

    switch (target.valueMode)
    {
        case AutomationValueMode::toggle:
            return clampedValue >= 0.5f ? 1.0f : 0.0f;

        case AutomationValueMode::stepped:
        {
            const auto steps = juce::jmax(2, target.stepCount);
            const auto maxIndex = steps - 1;
            const auto index = juce::jlimit(0, maxIndex, juce::roundToInt(clampedValue * (float) maxIndex));
            return maxIndex > 0 ? (float) index / (float) maxIndex : 0.0f;
        }

        case AutomationValueMode::continuous:
        default:
            return clampedValue;
    }
}

inline juce::String toStorageToken(AutomationRecordMode mode)
{
    switch (mode)
    {
        case AutomationRecordMode::touch: return "touch";
        case AutomationRecordMode::latch: return "latch";
        case AutomationRecordMode::write: return "write";
    }

    return "touch";
}

inline AutomationRecordMode automationRecordModeFromStorageToken(const juce::String& token)
{
    auto normalized = token.trim();
    if (normalized.equalsIgnoreCase("latch")) return AutomationRecordMode::latch;
    if (normalized.equalsIgnoreCase("write")) return AutomationRecordMode::write;
    return AutomationRecordMode::touch;
}

inline juce::String toDisplayName(AutomationRecordMode mode)
{
    switch (mode)
    {
        case AutomationRecordMode::touch: return "Touch";
        case AutomationRecordMode::latch: return "Latch";
        case AutomationRecordMode::write: return "Write";
    }

    return "Touch";
}

// Evaluates the segment from point `a` to point `b` at an absolute timeline position.
// The segment's shape/tension live on the start point `a` (Reaper convention). Shared by the
// timeline model (editing/preview) and the audio engine (realtime playback) so the curve drawn
// in the tracker is exactly the curve that plays back.
inline float evaluateAutomationSegment(const AutomationPoint& a, const AutomationPoint& b, double seconds)
{
    const auto span = b.seconds - a.seconds;
    const float t = span > 0.0 ? (float) juce::jlimit(0.0, 1.0, (seconds - a.seconds) / span) : 1.0f;

    switch (a.shape)
    {
        case AutomationCurveShape::step:
            return t < 1.0f ? a.value : b.value;

        case AutomationCurveShape::sCurve:
        {
            const float eased = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * t);
            return a.value + (b.value - a.value) * eased;
        }

        case AutomationCurveShape::power:
        {
            const float k = std::pow(2.0f, juce::jlimit(-1.0f, 1.0f, a.tension) * 4.0f);
            const float shaped = std::pow(t, k);
            return a.value + (b.value - a.value) * shaped;
        }

        case AutomationCurveShape::bezier:
        {
            const float midValue = (a.value + b.value) * 0.5f;
            const float controlValue = juce::jlimit(0.0f, 1.0f, midValue + juce::jlimit(-1.0f, 1.0f, a.tension) * 0.5f);
            const float oneMinusT = 1.0f - t;
            return oneMinusT * oneMinusT * a.value
                 + 2.0f * oneMinusT * t * controlValue
                 + t * t * b.value;
        }

        case AutomationCurveShape::linear:
        default:
            return a.value + (b.value - a.value) * t;
    }
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
