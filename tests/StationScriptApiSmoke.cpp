// Station's API for FRust scripts, through the real ScriptRunner and the real Station declarations
// (StationAgentApi.frust), against a fake host that records what a script did. The fake stands in for the
// project; what this proves is that every declared function reaches the host with the right arguments, that
// values come back correctly (numbers, decimals, text), and that failures carry their reason.

#include "../Source/Agent/StationScriptApi.h"

#include <BuiltInFrustData.h>

#include <cmath>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << std::endl;
    if (! ok)
        ++failures;
}

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

class FakeHost final : public StationAgentHost
{
public:
    struct Track { std::string name; double volumeDb = 0.0; double pan = 0.0; bool muted = false; bool soloed = false; };
    std::vector<Track> tracks;
    bool playing = false;
    int mainThreadCalls = 0;

    std::string projectSummary() override { return std::to_string(tracks.size()) + " track(s), " + (playing ? "playing" : "stopped"); }
    int trackCount() override { return (int) tracks.size(); }
    std::string trackName(int t) override { return has(t) ? tracks[(size_t) t - 1].name : ""; }
    double trackVolumeDb(int t) override { return has(t) ? tracks[(size_t) t - 1].volumeDb : -60.0; }
    double trackPan(int t) override { return has(t) ? tracks[(size_t) t - 1].pan : 0.0; }
    bool trackMuted(int t) override { return has(t) && tracks[(size_t) t - 1].muted; }
    bool trackSoloed(int t) override { return has(t) && tracks[(size_t) t - 1].soloed; }

    int addTrack(const std::string& name, std::string&) override
    {
        tracks.push_back({ name.empty() ? "Track " + std::to_string(tracks.size() + 1) : name });
        return (int) tracks.size();
    }
    bool setTrackName(int t, const std::string& n, std::string& e) override { if (! ok(t, e)) return false; tracks[(size_t) t - 1].name = n; return true; }
    bool setTrackVolumeDb(int t, double db, std::string& e) override
    {
        if (! ok(t, e)) return false;
        if (db < station_agent::minVolumeDb || db > station_agent::maxVolumeDb) { e = "Volume must be between -60 and 0 dB."; return false; }
        tracks[(size_t) t - 1].volumeDb = db;
        return true;
    }
    bool setTrackPan(int t, double p, std::string& e) override
    {
        if (! ok(t, e)) return false;
        if (p < -1.0 || p > 1.0) { e = "Pan must be between -1.0 and 1.0."; return false; }
        tracks[(size_t) t - 1].pan = p;
        return true;
    }
    bool setTrackMuted(int t, bool m, std::string& e) override { if (! ok(t, e)) return false; tracks[(size_t) t - 1].muted = m; return true; }
    bool setTrackSoloed(int t, bool s, std::string& e) override { if (! ok(t, e)) return false; tracks[(size_t) t - 1].soloed = s; return true; }
    bool transportPlay(std::string&) override { playing = true; return true; }
    bool transportStop(std::string&) override { playing = false; return true; }

private:
    bool has(int t) const { return t >= 1 && t <= (int) tracks.size(); }
    bool ok(int t, std::string& e) const
    {
        if (has(t)) return true;
        e = "There is no track " + std::to_string(t) + ".";
        return false;
    }
};

std::string apiText()
{
    int size = 0;
    const char* data = BuiltInFrustData::getNamedResource("StationAgentApi_frust", size);
    return data != nullptr ? std::string(data, (size_t) size) : std::string();
}
}

int main()
{
    FakeHost host;
    creation::frust::ScriptRunner runner(station_script::makeApi(host, apiText()),
                                         [](const std::function<void()>& work) { work(); return true; });

    check(! apiText().empty(), "Station's API declarations are embedded");

    // Build a small project with one script.
    auto result = runner.runBlocking(
        "pub fn run() -> String = {\n"
        "    let a = station_track_add(\"Bass\");\n"
        "    let b = station_track_add(\"\");\n"
        "    station_track_set_volume_db(a, -6.0);\n"
        "    station_track_set_pan(a, -0.5);\n"
        "    station_track_set_muted(b, 1);\n"
        "    station_track_set_soloed(a, 1);\n"
        "    station_track_set_name(b, \"Drums\");\n"
        "    station_transport_play();\n"
        "    station_log(station_project_summary());\n"
        "    station_track_name(a)\n"
        "}\n");
    check(result.ok, "a script using the Station API compiles and runs: " + result.error + (result.diagnostics.empty() ? "" : " / " + result.diagnostics[0].message));
    check(host.tracks.size() == 2 && host.tracks[0].name == "Bass" && host.tracks[1].name == "Drums", "tracks were added and renamed");
    check(host.tracks[0].volumeDb == -6.0 && host.tracks[0].pan == -0.5, "volume (decimal) and pan reached the host");
    check(host.tracks[1].muted && ! host.tracks[0].muted && host.tracks[0].soloed, "mute and solo reached the host");
    check(host.playing, "the transport was started");
    check(contains(result.output, "2 track(s), playing") && contains(result.output, "Bass"), "text comes back from the host: " + result.output);

    // Reading back.
    auto readBack = runner.runBlocking(
        "pub fn run() -> String = {\n"
        "    let n = station_track_count();\n"
        "    let db = station_track_volume_db(1);\n"
        "    let muted = station_track_muted(2);\n"
        "    if (n == 2) {\n"
        "        if (muted == 1) {\n"
        "            station_track_name(2)\n"
        "        } else {\n"
        "            \"not muted\"\n"
        "        }\n"
        "    } else {\n"
        "        \"wrong count\"\n"
        "    }\n"
        "}\n");
    check(readBack.ok && contains(readBack.output, "Drums"), "a script can read the project back: " + readBack.output + readBack.error);

    // Numbers can be reported without formatting them in FRust.
    auto logged = runner.runBlocking(
        "pub fn run() -> String = {\n"
        "    station_log_i64(\"tracks\", station_track_count());\n"
        "    station_log_f64(\"volume\", station_track_volume_db(1));\n"
        "    station_log_f64(\"pan\", station_track_pan(1));\n"
        "    \"done\"\n"
        "}\n");
    check(logged.ok && contains(logged.output, "tracks: 2") && contains(logged.output, "volume: -6") && contains(logged.output, "pan: -0.5"),
          "numbers are reported as label: value lines: " + logged.output + logged.error);

    // Failures carry their reason.
    auto failure = runner.runBlocking(
        "pub fn run() -> String = {\n"
        "    let ok = station_track_set_volume_db(9, -3.0);\n"
        "    if (ok == 0) {\n"
        "        station_last_error()\n"
        "    } else {\n"
        "        \"unexpectedly worked\"\n"
        "    }\n"
        "}\n");
    check(failure.ok && contains(failure.output, "no track 9"), "a failed call returns 0 and station_last_error says why: " + failure.output);

    auto outOfRange = runner.runBlocking(
        "pub fn run() -> String = {\n"
        "    let ok = station_track_set_volume_db(1, 12.0);\n"
        "    if (ok == 0) {\n"
        "        station_last_error()\n"
        "    } else {\n"
        "        \"unexpectedly worked\"\n"
        "    }\n"
        "}\n");
    check(outOfRange.ok && contains(outOfRange.output, "-60 and 0") && host.tracks[0].volumeDb == -6.0, "an out-of-range volume is refused and changes nothing: " + outOfRange.output);

    // dB and gain.
    check(station_agent::gainFromDb(0.0) == 1.0f && station_agent::gainFromDb(-60.0) == 0.0f, "0 dB is the top of the fader and -60 dB is silence");
    const float half = station_agent::gainFromDb(-6.0);
    check(half > 0.50f && half < 0.51f, "-6 dB is about half the fader's gain");
    check(std::abs(station_agent::dbFromGain(station_agent::gainFromDb(-12.0)) - (-12.0)) < 0.01, "gain and dB convert back and forth");

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
