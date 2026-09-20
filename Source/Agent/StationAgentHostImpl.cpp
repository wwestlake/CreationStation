#include "StationAgentHost.h"

#include "../MainComponent.h"

// Station's host for scripts, over the real project. Each change goes through the same panel callback the user's
// own action would (the mixer fader, the pan knob, the track name field, the transport buttons), so the engine,
// the control surface, automation recording and the on-screen controls all follow, exactly as if the user had
// done it. Runs on the message thread.
class MainComponent::AgentHost final : public StationAgentHost
{
public:
    explicit AgentHost(MainComponent& ownerToUse) : owner(ownerToUse) {}

    std::string projectSummary() override
    {
        juce::String text;
        text << owner.engine.getTrackCount() << " track(s). Transport: "
             << (owner.engine.isPlaying() ? "playing" : "stopped") << ".";
        return text.toStdString();
    }

    int trackCount() override { return owner.engine.getTrackCount(); }

    std::string trackName(int track) override
    {
        return exists(track) ? owner.engine.getTrackName(track - 1).toStdString() : std::string();
    }

    double trackVolumeDb(int track) override
    {
        return exists(track) ? station_agent::dbFromGain(owner.engine.getTrackGain(track - 1)) : station_agent::minVolumeDb;
    }

    double trackPan(int track) override { return exists(track) ? (double) owner.engine.getTrackPan(track - 1) : 0.0; }
    bool trackMuted(int track) override { return exists(track) && owner.engine.isTrackMuted(track - 1); }
    bool trackSoloed(int track) override { return exists(track) && owner.engine.isTrackSoloed(track - 1); }

    int addTrack(const std::string& name, std::string& error) override
    {
        const int before = owner.engine.getTrackCount();
        owner.addTrack();
        const int after = owner.engine.getTrackCount();
        if (after <= before)
        {
            error = "Station could not add a track.";
            return 0;
        }

        if (! name.empty())
            owner.trackerPanel.onTrackNameChanged(after - 1, juce::String(name));
        return after;
    }

    bool setTrackName(int track, const std::string& name, std::string& error) override
    {
        if (! check(track, error))
            return false;
        owner.trackerPanel.onTrackNameChanged(track - 1, juce::String(name));
        return true;
    }

    bool setTrackVolumeDb(int track, double db, std::string& error) override
    {
        if (! check(track, error))
            return false;
        if (db < station_agent::minVolumeDb || db > station_agent::maxVolumeDb)
        {
            error = "Volume must be between -60 and 0 dB (0 is the top of the fader).";
            return false;
        }

        const int index = track - 1;
        const float gain = station_agent::gainFromDb(db);
        owner.mixerPanel.setChannelGain(index, gain);
        owner.mixerPanel.onGainChanged(index, gain);
        owner.trackerPanel.setTrackGain(index, owner.engine.getTrackGain(index));
        return true;
    }

    bool setTrackPan(int track, double pan, std::string& error) override
    {
        if (! check(track, error))
            return false;
        if (pan < -1.0 || pan > 1.0)
        {
            error = "Pan must be between -1.0 (left) and 1.0 (right).";
            return false;
        }

        const int index = track - 1;
        owner.mixerPanel.setChannelPan(index, (float) pan);
        owner.mixerPanel.onPanChanged(index, (float) pan);
        return true;
    }

    bool setTrackMuted(int track, bool muted, std::string& error) override
    {
        if (! check(track, error))
            return false;

        const int index = track - 1;
        owner.mixerPanel.setChannelMuted(index, muted);
        owner.mixerPanel.onMuteChanged(index, muted);
        owner.trackerPanel.setTrackMuted(index, owner.engine.isTrackMuted(index));
        return true;
    }

    bool setTrackSoloed(int track, bool soloed, std::string& error) override
    {
        if (! check(track, error))
            return false;

        const int index = track - 1;
        owner.mixerPanel.setChannelSoloed(index, soloed);
        owner.mixerPanel.onSoloChanged(index, soloed);
        owner.trackerPanel.setTrackSoloed(index, owner.engine.isTrackSoloed(index));
        return true;
    }

    bool transportPlay(std::string&) override
    {
        if (owner.transportBar.onPlay)
            owner.transportBar.onPlay();
        return true;
    }

    bool transportStop(std::string&) override
    {
        if (owner.transportBar.onStop)
            owner.transportBar.onStop();
        return true;
    }

private:
    bool exists(int track) const { return track >= 1 && track <= owner.engine.getTrackCount(); }

    bool check(int track, std::string& error) const
    {
        if (exists(track))
            return true;
        error = "There is no track " + std::to_string(track) + ". The project has " + std::to_string(owner.engine.getTrackCount()) + " track(s), numbered from 1.";
        return false;
    }

    MainComponent& owner;
};

StationAgentHost& MainComponent::getAgentHost()
{
    if (agentHost == nullptr)
        agentHost = std::make_unique<AgentHost>(*this);
    return *agentHost;
}
