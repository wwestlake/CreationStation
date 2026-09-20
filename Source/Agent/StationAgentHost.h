#pragma once

#include <algorithm>
#include <cmath>
#include <string>

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
