#include "StationScriptApi.h"

#include <cstdint>

namespace
{
using creation::frust::ScriptRunner;
using Call = ScriptRunner::Call;

// The host the running script belongs to: the app's own object, carried with the run (ScriptApi::userData).
StationAgentHost* currentHost()
{
    return static_cast<StationAgentHost*>(Call::userData());
}

// Runs `work` against the host on the main thread and returns what it gives, or `fallback` if the
// script was stopped or the host is not connected.
template <typename R, typename Fn>
R fromHost(Fn&& work, R fallback)
{
    R result = fallback;
    auto* host = currentHost();
    if (host == nullptr)
    {
        Call::setLastError("Station is not connected to the script.");
        return fallback;
    }
    if (! Call::onMainThread([&] { result = work(*host); }))
        return fallback;
    return result;
}

// A change: 1 if it worked, else 0 with the reason kept for station_last_error().
template <typename Fn>
std::int64_t change(Fn&& work)
{
    std::string error;
    const auto ok = fromHost<std::int64_t>([&](StationAgentHost& host) -> std::int64_t { return work(host, error) ? 1 : 0; }, 0);
    if (ok == 0 && ! error.empty())
        Call::setLastError(error);
    else if (ok == 0 && Call::lastError().empty())
        Call::setLastError("That did not work.");
    return ok;
}

extern "C"
{
const char* station_project_summary()
{
    return Call::handBack(fromHost<std::string>([](StationAgentHost& h) { return h.projectSummary(); }, "(Station is not available)"));
}

std::int64_t station_track_count()
{
    return fromHost<std::int64_t>([](StationAgentHost& h) -> std::int64_t { return h.trackCount(); }, 0);
}

const char* station_track_name(std::int64_t track)
{
    return Call::handBack(fromHost<std::string>([=](StationAgentHost& h) { return h.trackName((int) track); }, ""));
}

double station_track_volume_db(std::int64_t track)
{
    return fromHost<double>([=](StationAgentHost& h) { return h.trackVolumeDb((int) track); }, 0.0);
}

double station_track_pan(std::int64_t track)
{
    return fromHost<double>([=](StationAgentHost& h) { return h.trackPan((int) track); }, 0.0);
}

std::int64_t station_track_muted(std::int64_t track)
{
    return fromHost<std::int64_t>([=](StationAgentHost& h) -> std::int64_t { return h.trackMuted((int) track) ? 1 : 0; }, 0);
}

std::int64_t station_track_soloed(std::int64_t track)
{
    return fromHost<std::int64_t>([=](StationAgentHost& h) -> std::int64_t { return h.trackSoloed((int) track) ? 1 : 0; }, 0);
}

std::int64_t station_track_add(const char* name)
{
    const std::string trackName = name != nullptr ? name : "";
    std::string error;
    const auto number = fromHost<std::int64_t>([&](StationAgentHost& h) -> std::int64_t { return h.addTrack(trackName, error); }, 0);
    if (number <= 0)
        Call::setLastError(error.empty() ? "The track could not be added." : error);
    return number > 0 ? number : 0;
}

std::int64_t station_track_set_name(std::int64_t track, const char* name)
{
    const std::string text = name != nullptr ? name : "";
    return change([&](StationAgentHost& h, std::string& error) { return h.setTrackName((int) track, text, error); });
}

std::int64_t station_track_set_volume_db(std::int64_t track, double db)
{
    return change([&](StationAgentHost& h, std::string& error) { return h.setTrackVolumeDb((int) track, db, error); });
}

std::int64_t station_track_set_pan(std::int64_t track, double pan)
{
    return change([&](StationAgentHost& h, std::string& error) { return h.setTrackPan((int) track, pan, error); });
}

std::int64_t station_track_set_muted(std::int64_t track, std::int64_t muted)
{
    return change([&](StationAgentHost& h, std::string& error) { return h.setTrackMuted((int) track, muted != 0, error); });
}

std::int64_t station_track_set_soloed(std::int64_t track, std::int64_t soloed)
{
    return change([&](StationAgentHost& h, std::string& error) { return h.setTrackSoloed((int) track, soloed != 0, error); });
}

std::int64_t station_transport_play()
{
    return change([](StationAgentHost& h, std::string& error) { return h.transportPlay(error); });
}

std::int64_t station_transport_stop()
{
    return change([](StationAgentHost& h, std::string& error) { return h.transportStop(error); });
}

std::int64_t station_log(const char* text)
{
    Call::log(text != nullptr ? text : "");
    return 1;
}

const char* station_last_error()
{
    return Call::handBack(Call::lastError());
}
}
}

namespace station_script
{
creation::frust::ScriptApi makeApi(StationAgentHost& host, std::string declarations)
{
    creation::frust::ScriptApi api;
    api.userData = &host;
    api.moduleName = "StationAgentApi";
    api.declarations = std::move(declarations);
    api.applicationName = "creation-station";
    api.functions = {
        { "station_project_summary", reinterpret_cast<void*>(&station_project_summary) },
        { "station_track_count", reinterpret_cast<void*>(&station_track_count) },
        { "station_track_name", reinterpret_cast<void*>(&station_track_name) },
        { "station_track_volume_db", reinterpret_cast<void*>(&station_track_volume_db) },
        { "station_track_pan", reinterpret_cast<void*>(&station_track_pan) },
        { "station_track_muted", reinterpret_cast<void*>(&station_track_muted) },
        { "station_track_soloed", reinterpret_cast<void*>(&station_track_soloed) },
        { "station_track_add", reinterpret_cast<void*>(&station_track_add) },
        { "station_track_set_name", reinterpret_cast<void*>(&station_track_set_name) },
        { "station_track_set_volume_db", reinterpret_cast<void*>(&station_track_set_volume_db) },
        { "station_track_set_pan", reinterpret_cast<void*>(&station_track_set_pan) },
        { "station_track_set_muted", reinterpret_cast<void*>(&station_track_set_muted) },
        { "station_track_set_soloed", reinterpret_cast<void*>(&station_track_set_soloed) },
        { "station_transport_play", reinterpret_cast<void*>(&station_transport_play) },
        { "station_transport_stop", reinterpret_cast<void*>(&station_transport_stop) },
        { "station_log", reinterpret_cast<void*>(&station_log) },
        { "station_last_error", reinterpret_cast<void*>(&station_last_error) },
    };
    return api;
}
}
