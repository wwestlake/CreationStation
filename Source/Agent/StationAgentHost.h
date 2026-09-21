#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// What Station offers the scripts the Virtual Engineer writes (see StationAgentApi.frust). MainComponent
// implements this over the real project; a test implements it over a recording fake. Every method is
// called on the message thread, one at a time, and reports failure through `error`.
//
// Tracks are numbered from 1, as the user sees them.
class StationAgentHost
{
public:
    virtual ~StationAgentHost() = default;

    virtual std::string projectSummary() = 0;
    virtual int trackCount() = 0;
    virtual std::string trackName(int track) = 0;
    virtual double trackVolumeDb(int track) = 0;
    virtual double trackPan(int track) = 0;
    virtual bool trackMuted(int track) = 0;
    virtual bool trackSoloed(int track) = 0;

    // Returns the new track's number, or 0 on failure.
    virtual int addTrack(const std::string& name, std::string& error) = 0;
    virtual bool setTrackName(int track, const std::string& name, std::string& error) = 0;
    virtual bool setTrackVolumeDb(int track, double db, std::string& error) = 0;
    virtual bool setTrackPan(int track, double pan, std::string& error) = 0;
    virtual bool setTrackMuted(int track, bool muted, std::string& error) = 0;
    virtual bool setTrackSoloed(int track, bool soloed, std::string& error) = 0;

    virtual bool transportPlay(std::string& error) = 0;
    virtual bool transportStop(std::string& error) = 0;

    // ---- The rest of what a track's controls can do. Optional: a host that cannot do one leaves it as it is. ----

    // "audio", "midi", "automation", "signal", "foley", "video", "folder" or "marker" ("" if there is no such track).
    virtual std::string trackKind(int track) { (void) track; return {}; }
    virtual bool setTrackKind(int track, const std::string& kind, std::string& error)
    {
        (void) track; (void) kind;
        error = "Changing a track's kind is not available.";
        return false;
    }

    virtual bool trackArmed(int track) { (void) track; return false; }
    virtual bool setTrackArmed(int track, bool armed, std::string& error) { (void) track; (void) armed; error = "Arming is not available."; return false; }
    virtual bool trackMonitored(int track) { (void) track; return false; }
    virtual bool setTrackMonitored(int track, bool monitored, std::string& error) { (void) track; (void) monitored; error = "Monitoring is not available."; return false; }
    virtual bool trackStereo(int track) { (void) track; return false; }
    virtual bool setTrackStereo(int track, bool stereo, std::string& error) { (void) track; (void) stereo; error = "Mono/stereo is not available."; return false; }

    // Moves a track so it ends up at position `destination` (from 1).
    virtual bool moveTrack(int track, int destination, std::string& error) { (void) track; (void) destination; error = "Moving tracks is not available."; return false; }

    // ---- Automation. An automation track (kind "automation") drives one control of another track. ----

    // Point `automationTrack` at `control` ("volume" or "pan") of `targetTrack`.
    virtual bool setAutomationTarget(int automationTrack, int targetTrack, const std::string& control, std::string& error)
    {
        (void) automationTrack; (void) targetTrack; (void) control;
        error = "Automation is not available.";
        return false;
    }

    // Adds a point to the lane at `seconds` on the timeline. `value` is in the target's own units: decibels (-60 to 0) for a
    // volume lane, -1 (left) to 1 (right) for a pan lane.
    virtual bool addAutomationPoint(int automationTrack, double seconds, double value, std::string& error)
    {
        (void) automationTrack; (void) seconds; (void) value;
        error = "Automation is not available.";
        return false;
    }

    virtual bool clearAutomation(int automationTrack, std::string& error) { (void) automationTrack; error = "Automation is not available."; return false; }

    // The tempo the project is set to, in beats per minute (0 if unknown).
    virtual double projectTempoBpm() { return 0.0; }

    // The audio of one track as mono samples, for measuring: `durationSeconds` of it starting `startSeconds` along the
    // timeline. Track 0 means the whole mix (every audio clip). False, with the reason, if there is no audio to measure or
    // it cannot be rendered. Optional: a host that cannot supply audio leaves this as it is.
    virtual bool trackAudio(int track, double startSeconds, double durationSeconds, std::vector<float>& mono,
                            double& sampleRate, std::string& error)
    {
        (void) track; (void) startSeconds; (void) durationSeconds; (void) mono; (void) sampleRate;
        error = "Audio is not available.";
        return false;
    }

    // One note of a MIDI track, positioned in beats from the start of the timeline.
    struct MidiNoteInfo
    {
        int pitch = 60;
        int velocity = 100;
        double beat = 0.0;
        double lengthBeats = 0.0;
    };

    // Every MIDI note on one track (all its MIDI clips), for reading exactly what was played. False, with the reason, if the
    // track has no MIDI notes. Optional.
    virtual bool trackMidiNotes(int track, std::vector<MidiNoteInfo>& notes, std::string& error)
    {
        (void) track; (void) notes;
        error = "MIDI is not available.";
        return false;
    }
};

// The mixer fader is a linear gain from 0 to 1, where 1 is the top of the fader. The API speaks decibels, from
// -60 up to 0.
namespace station_agent
{
constexpr double minVolumeDb = -60.0;
constexpr double maxVolumeDb = 0.0;

inline float gainFromDb(double db)
{
    return db <= minVolumeDb ? 0.0f : (float) std::pow(10.0, std::min(db, maxVolumeDb) / 20.0);
}

inline double dbFromGain(float gain)
{
    return gain <= 0.0f ? minVolumeDb : std::max(minVolumeDb, 20.0 * std::log10((double) gain));
}
}
