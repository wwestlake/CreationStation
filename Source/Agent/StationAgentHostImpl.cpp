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

    // ---- The rest of a track's controls: each calls the same handler the track's own button or menu calls ----

    std::string trackKind(int track) override
    {
        return exists(track) ? kindName(owner.timelineModel.getTrackKind(track - 1)) : std::string();
    }

    bool setTrackKind(int track, const std::string& kind, std::string& error) override
    {
        if (! check(track, error))
            return false;

        cs::TrackKind wanted = cs::TrackKind::audio;
        if (! kindFromName(kind, wanted))
        {
            error = "'" + kind + "' is not a track kind. Use audio, midi, automation, signal, foley, video, folder or marker.";
            return false;
        }
        if (owner.trackerPanel.onTrackKindChanged)
            owner.trackerPanel.onTrackKindChanged(track - 1, wanted);
        return true;
    }

    bool trackArmed(int track) override
    {
        return exists(track) && juce::isPositiveAndBelow(track - 1, (int) owner.armedTracks.size()) && owner.armedTracks[(size_t) track - 1];
    }

    bool setTrackArmed(int track, bool armed, std::string& error) override
    {
        if (! check(track, error))
            return false;
        if (owner.trackerPanel.onTrackArmChanged)
            owner.trackerPanel.onTrackArmChanged(track - 1, armed);
        return true;
    }

    bool trackMonitored(int track) override
    {
        return exists(track) && juce::isPositiveAndBelow(track - 1, (int) owner.monitoredTracks.size()) && owner.monitoredTracks[(size_t) track - 1];
    }

    bool setTrackMonitored(int track, bool monitored, std::string& error) override
    {
        if (! check(track, error))
            return false;
        if (owner.trackerPanel.onTrackMonitorChanged)
            owner.trackerPanel.onTrackMonitorChanged(track - 1, monitored);
        return true;
    }

    bool trackStereo(int track) override { return exists(track) && owner.engine.isTrackStereoEnabled(track - 1); }

    bool setTrackStereo(int track, bool stereo, std::string& error) override
    {
        if (! check(track, error))
            return false;
        if (owner.trackerPanel.onTrackStereoChanged)
            owner.trackerPanel.onTrackStereoChanged(track - 1, stereo);
        return true;
    }

    bool moveTrack(int track, int destination, std::string& error) override
    {
        if (! check(track, error))
            return false;
        if (destination < 1 || destination > owner.engine.getTrackCount())
        {
            error = "There is no position " + std::to_string(destination) + ". The project has " + std::to_string(owner.engine.getTrackCount()) + " track(s).";
            return false;
        }
        if (! owner.performTrackMove(track - 1, destination - 1))
        {
            error = "That track could not be moved there.";
            return false;
        }
        owner.syncTrackViews();
        owner.saveSessionToDisk();
        return true;
    }

    bool setAutomationTarget(int automationTrack, int targetTrack, const std::string& control, std::string& error) override
    {
        if (! checkAutomation(automationTrack, error) || ! check(targetTrack, error))
            return false;
        if (targetTrack == automationTrack || owner.timelineModel.getTrackKind(targetTrack - 1) == cs::TrackKind::automation)
        {
            error = "An automation track must control another track that is not itself an automation track.";
            return false;
        }

        cs::AutomationTarget target;
        target.targetTrackIndex = targetTrack - 1;
        auto trackName = owner.timelineModel.getTrackName(targetTrack - 1);
        if (trackName.isEmpty())
            trackName = "Track " + juce::String(targetTrack);
        if (control == "volume")
        {
            target.kind = cs::AutomationTargetKind::trackVolume;
            target.displayName = trackName + " \xe2\x86\x92 Volume";
        }
        else if (control == "pan")
        {
            target.kind = cs::AutomationTargetKind::trackPan;
            target.displayName = trackName + " \xe2\x86\x92 Pan";
        }
        else
        {
            error = "'" + control + "' is not a control an automation track can drive from a script. Use volume or pan.";
            return false;
        }

        owner.timelineModel.setAutomationTarget(automationTrack - 1, target);
        owner.pushAutomationDataToEngine(automationTrack - 1);
        owner.trackerPanel.setAutomationTargetLabel(automationTrack - 1, target.displayName);
        owner.saveSessionToDisk();
        return true;
    }

    bool addAutomationPoint(int automationTrack, double seconds, double value, std::string& error) override
    {
        if (! checkAutomation(automationTrack, error))
            return false;
        const auto target = owner.timelineModel.getAutomationTarget(automationTrack - 1);
        float normalized = 0.0f;
        if (target.kind == cs::AutomationTargetKind::trackVolume)
        {
            if (value < station_agent::minVolumeDb || value > station_agent::maxVolumeDb)
            {
                error = "A volume point must be between -60 and 0 dB.";
                return false;
            }
            normalized = station_agent::gainFromDb(value);
        }
        else if (target.kind == cs::AutomationTargetKind::trackPan)
        {
            if (value < -1.0 || value > 1.0)
            {
                error = "A pan point must be between -1.0 (left) and 1.0 (right).";
                return false;
            }
            normalized = (float) ((value + 1.0) * 0.5);
        }
        else
        {
            error = "Point this automation track at a control first (station_automation_set_target).";
            return false;
        }
        if (seconds < 0.0)
        {
            error = "A point's time cannot be before the start (0 seconds).";
            return false;
        }

        owner.timelineModel.addOrUpdateAutomationPoint(automationTrack - 1, seconds, normalized, 0.001);
        owner.pushAutomationDataToEngine(automationTrack - 1);
        owner.trackerPanel.refreshTimelineView();
        owner.saveSessionToDisk();
        return true;
    }

    bool clearAutomation(int automationTrack, std::string& error) override
    {
        if (! checkAutomation(automationTrack, error))
            return false;
        owner.timelineModel.clearAutomationLane(automationTrack - 1);
        owner.pushAutomationDataToEngine(automationTrack - 1);
        owner.trackerPanel.refreshTimelineView();
        owner.saveSessionToDisk();
        return true;
    }

    double projectTempoBpm() override { return owner.timelineModel.getTempoBpm(); }

    bool trackMidiNotes(int track, std::vector<MidiNoteInfo>& notes, std::string& error) override
    {
        if (! check(track, error))
            return false;

        const double bpm = owner.timelineModel.getTempoBpm();
        notes.clear();
        for (const auto& clip : owner.timelineModel.getClips())
        {
            if (clip.kind != cs::ClipKind::midi || clip.trackIndex != track - 1)
                continue;
            const double clipStartBeats = clip.startSeconds * bpm / 60.0;
            for (const auto& note : clip.midiNotes)
                if (! note.muted)
                    notes.push_back({ note.pitch, note.velocity, clipStartBeats + note.startBeats, note.lengthBeats });
        }
        if (notes.empty())
        {
            error = "Track " + std::to_string(track) + " has no MIDI notes.";
            return false;
        }
        return true;
    }

    bool trackAudio(int track, double startSeconds, double durationSeconds, std::vector<float>& mono, double& sampleRate,
                    std::string& error) override
    {
        // Track 0 is the whole mix.
        if (track != 0 && ! check(track, error))
            return false;

        // The audio clips on this track, rendered on their own through the engine's own offline path.
        juce::Array<WorkstationAudioEngine::PlaybackClipTarget> all;
        double lastClipEnd = 0.0;
        juce::String message;
        if (! owner.buildTrackerPlaybackTargets(all, lastClipEnd, message, false))
        {
            error = message.isNotEmpty() ? message.toStdString() : "The project's audio could not be prepared.";
            return false;
        }

        juce::Array<WorkstationAudioEngine::PlaybackClipTarget> mine;
        for (const auto& target : all)
            if (track == 0 || target.trackIndex == track - 1)
                mine.add(target);
        if (mine.isEmpty())
        {
            error = track == 0 ? std::string("The project has no audio clips to measure.")
                               : "Track " + std::to_string(track) + " has no audio clips to measure.";
            return false;
        }

        WorkstationAudioEngine::RenderSettings settings;
        settings.sampleRate = 48000.0;
        settings.startSeconds = juce::jmax(0.0, startSeconds);
        const double duration = juce::jlimit(0.5, 120.0, durationSeconds);

        juce::AudioBuffer<float> buffer;
        juce::String renderError;
        if (! owner.engine.renderTrackerMixToBuffer(mine, duration, settings, buffer, renderError))
        {
            error = renderError.isNotEmpty() ? renderError.toStdString() : "The track could not be rendered.";
            return false;
        }

        const int channels = buffer.getNumChannels();
        const int samples = buffer.getNumSamples();
        if (channels == 0 || samples == 0)
        {
            error = "The track produced no audio in that time range.";
            return false;
        }

        mono.assign((size_t) samples, 0.0f);
        for (int c = 0; c < channels; ++c)
        {
            const float* in = buffer.getReadPointer(c);
            for (int i = 0; i < samples; ++i)
                mono[(size_t) i] += in[i] / (float) channels;
        }
        sampleRate = settings.sampleRate;
        return true;
    }

private:
    static std::string kindName(cs::TrackKind kind)
    {
        switch (kind)
        {
            case cs::TrackKind::audio: return "audio";
            case cs::TrackKind::midi: return "midi";
            case cs::TrackKind::automation: return "automation";
            case cs::TrackKind::signal: return "signal";
            case cs::TrackKind::foley: return "foley";
            case cs::TrackKind::video: return "video";
            case cs::TrackKind::folder: return "folder";
            case cs::TrackKind::marker: return "marker";
        }
        return "audio";
    }

    static bool kindFromName(const std::string& name, cs::TrackKind& kind)
    {
        static const cs::TrackKind all[] = { cs::TrackKind::audio, cs::TrackKind::midi, cs::TrackKind::automation, cs::TrackKind::signal,
                                             cs::TrackKind::foley, cs::TrackKind::video, cs::TrackKind::folder, cs::TrackKind::marker };
        for (auto candidate : all)
            if (kindName(candidate) == name)
            {
                kind = candidate;
                return true;
            }
        return false;
    }

    bool checkAutomation(int track, std::string& error) const
    {
        if (! check(track, error))
            return false;
        if (owner.timelineModel.getTrackKind(track - 1) != cs::TrackKind::automation)
        {
            error = "Track " + std::to_string(track) + " is not an automation track. Change its kind to \"automation\" first (station_track_set_kind).";
            return false;
        }
        return true;
    }

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
