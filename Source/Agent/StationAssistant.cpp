#include "StationAssistant.h"

#include "StationScriptApi.h"

#include <creation/audio/DrumAnalysis.h>
#include <creation/audio/PitchAnalysis.h>
#include <creation/audio/TempoAnalysis.h>
#include <creation/services/SuiteAiProviderRuntime.h>

#include <chrono>
#include <cstdio>
#include <condition_variable>
#include <mutex>

namespace
{
using namespace creation::ai;

// Runs `work` on the message thread and waits for it, for a script on the assistant's thread. Unlike a plain
// blocking call, the wait can be abandoned: once the assistant is being destroyed (`alive` false) the wait ends
// and the queued work skips itself. Both the queued work and the destructor run on the message thread, so the
// work either finishes before the destructor starts or never touches anything afterwards.
bool runOnMessageThread(const std::shared_ptr<std::atomic<bool>>& alive, const std::function<void()>& work)
{
    auto* manager = juce::MessageManager::getInstanceWithoutCreating();
    if (manager == nullptr)
        return false;
    if (manager->isThisTheMessageThread())
    {
        work();
        return true;
    }

    struct State
    {
        std::mutex mutex;
        std::condition_variable finished;
        bool done = false;
        std::function<void()> work;
    };
    auto state = std::make_shared<State>();
    state->work = work;

    juce::MessageManager::callAsync([state, alive]
    {
        if (alive->load())
            state->work();
        std::lock_guard<std::mutex> lock(state->mutex);
        state->done = true;
        state->finished.notify_all();
    });

    std::unique_lock<std::mutex> lock(state->mutex);
    while (! state->done)
    {
        state->finished.wait_for(lock, std::chrono::milliseconds(20));
        if (! state->done && ! alive->load())
            return false;
    }
    return true;
}

// The model families whose chat API takes only the default temperature and the newer output-limit field.
bool isReasoningModel(const juce::String& model)
{
    const auto name = model.toLowerCase();
    return name.startsWith("o1") || name.startsWith("o3") || name.startsWith("o4") || name.startsWith("gpt-5");
}

// The FRust a tool call carried, for showing what was run.
juce::String codeOf(const ToolCallBlock& call)
{
    juce::var parsed;
    if (juce::JSON::parse(juce::String(call.argumentsJson), parsed).failed())
        return {};
    if (auto* object = parsed.getDynamicObject())
        return object->getProperty("code").toString();
    return {};
}

// Why a run failed, in a line or two for the transcript: the compile errors themselves (not the heading above
// them, and not the hints), so what is shown is what went wrong.
juce::String shortReason(const std::string& content)
{
    juce::StringArray kept;
    for (const auto& line : juce::StringArray::fromLines(juce::String(content)))
    {
        const auto trimmed = line.trim();
        if (trimmed.isEmpty() || trimmed.startsWith("The script did not compile") || trimmed.startsWith("Hint:"))
            continue;
        kept.add(trimmed);
        if (kept.size() == 2)
            break;
    }
    return kept.joinIntoString("; ").substring(0, 240);
}
}

StationAssistant::StationAssistant(StationAgentHost& hostToUse, std::string declarations, std::string guideText,
                                   std::function<std::string(const std::string&)> lookup)
    : host(hostToUse), apiDeclarations(std::move(declarations)), guide(std::move(guideText))
{
    auto api = station_script::makeApi(host, apiDeclarations);
    runner = std::make_unique<creation::frust::ScriptRunner>(
        api, [alivePtr = alive](const std::function<void()>& work) { return runOnMessageThread(alivePtr, work); });

    tools.add(makeRunFrustTool(*runner));
    tools.add(makeCheckFrustTool(*runner));
    tools.add(makeFrustApiReferenceTool(runner->api()));
    // The Producer does not write scripts; what it looks up is how to read what it measures.
    producerTools.add(std::make_shared<FunctionTool>(
        ToolSpec { "music_knowledge",
                   "Look up how to interpret a measurement: what a bend, slide, scoop, vibrato, blue note, swing or triplet feel, laid-back or "
                   "pushed timing, rubato, ghost notes, or humanized MIDI drums look like in the numbers, and how to tell a mistake from an "
                   "expressive choice. Use it BEFORE you judge a measurement, and ask a specific question.",
                   R"({"type":"object","properties":{"query":{"type":"string","description":"What you want to know, e.g. how to tell a bend from being flat, or what a triplet feel looks like."}},"required":["query"]})" },
        Effect::read,
        [search = lookup](const std::string& args, ToolContext&) -> ToolResult
        {
            juce::var parsed;
            std::string query;
            if (! juce::JSON::parse(juce::String(args), parsed).failed())
                if (auto* object = parsed.getDynamicObject())
                    query = object->getProperty("query").toString().toStdString();
            if (query.empty())
                return ToolResult::failure("Give a query: what do you want to know?");
            const auto found = search ? search(query) : std::string();
            return ToolResult::success(found.empty() ? std::string("Nothing in the help matches that. Try different words.") : found);
        }));
    tools.add(makeFrustLookupTool(std::move(lookup)));

    // ---- The Producer's tools: reading and measuring, nothing that changes the project ----
    auto alivePtr = alive;

    producerTools.add(std::make_shared<FunctionTool>(
        ToolSpec { "project_overview",
                   "List the project's tracks with their names, volume, pan, mute and solo, and whether the transport is playing. "
                   "Use it to know what is in the session before talking about it.",
                   R"({"type":"object","properties":{}})" },
        Effect::read,
        [this, alivePtr](const std::string&, ToolContext&) -> ToolResult
        {
            std::string text;
            const bool ran = runOnMessageThread(alivePtr, [&]
            {
                text = host.projectSummary() + "\n";
                for (int track = 1; track <= host.trackCount(); ++track)
                {
                    char line[256];
                    std::snprintf(line, sizeof(line), "Track %d \"%s\": volume %.1f dB, pan %+.2f, %s%s\n", track,
                                  host.trackName(track).c_str(), host.trackVolumeDb(track), host.trackPan(track),
                                  host.trackMuted(track) ? "muted" : "not muted", host.trackSoloed(track) ? ", soloed" : "");
                    text += line;
                }
            });
            return ran ? ToolResult::success(text) : ToolResult::failure("Station is not available.");
        }));

    producerTools.add(std::make_shared<FunctionTool>(
        ToolSpec { "analyze_pitch",
                   "Measure the pitch of one track: which notes it plays, exactly how many cents sharp or flat each is, how much the "
                   "pitch wobbles, and when. Works on a single melodic line (a vocal, a lead, a bass); it does not measure chords or "
                   "full mixes and will say so. Returns a short report you can discuss.",
                   R"({"type":"object","properties":{
                       "track":{"type":"integer","minimum":1,"description":"The track number, from 1."},
                       "start_seconds":{"type":"number","minimum":0,"description":"Where to start on the timeline. Default 0."},
                       "duration_seconds":{"type":"number","minimum":0.5,"maximum":120,"description":"How much to measure. Default 30."},
                       "reference_a":{"type":"number","minimum":400,"maximum":480,"description":"The tuning of A4 in Hz. Default 440."}
                     },"required":["track"]})" },
        Effect::read,
        [this, alivePtr](const std::string& args, ToolContext& ctx) -> ToolResult
        {
            juce::var parsed;
            if (juce::JSON::parse(juce::String(args), parsed).failed() || parsed.getDynamicObject() == nullptr)
                return ToolResult::failure("The arguments must be an object with a track number.");
            auto* object = parsed.getDynamicObject();
            const int track = (int) object->getProperty("track");
            const double start = object->hasProperty("start_seconds") ? (double) object->getProperty("start_seconds") : 0.0;
            const double duration = object->hasProperty("duration_seconds") ? (double) object->getProperty("duration_seconds") : 30.0;
            const double referenceA = object->hasProperty("reference_a") ? (double) object->getProperty("reference_a") : 440.0;

            std::vector<float> mono;
            double sampleRate = 0.0;
            std::string error, name;
            bool got = false;
            const bool ran = runOnMessageThread(alivePtr, [&]
            {
                got = host.trackAudio(track, start, duration, mono, sampleRate, error);
                name = host.trackName(track);
            });
            if (! ran)
                return ToolResult::failure("Station is not available.");
            if (! got)
                return ToolResult::failure(error);
            if (ctx.cancelled())
                return ToolResult::failure("Stopped.");

            // The measuring runs here, on the assistant's thread, not the message thread.
            creation::audio::PitchOptions options;
            options.referenceA = referenceA;
            const auto notes = creation::audio::analyzeNotes(mono.data(), mono.size(), sampleRate, options);
            return ToolResult::success(creation::audio::describeNotes(
                notes, "track " + std::to_string(track) + " \"" + name + "\"", start, start + (double) mono.size() / sampleRate, referenceA));
        },
        180.0));

    producerTools.add(std::make_shared<FunctionTool>(
        ToolSpec { "analyze_tempo",
                   "Measure the tempo and beat of a track or of the whole mix: the tempo in BPM, where the beats fall, how steady it is "
                   "(does it rush, drag or drift), and how tightly the playing sits on the beat, compared with the project's tempo. "
                   "It measures the attacks in the sound, so it works best on drums and other percussive playing; a smooth vocal or a "
                   "pad gives little to measure and it will say so. Use track 0 for the whole mix.",
                   R"({"type":"object","properties":{
                       "track":{"type":"integer","minimum":0,"description":"The track number from 1, or 0 for the whole mix. Default 0."},
                       "start_seconds":{"type":"number","minimum":0,"description":"Where to start on the timeline. Default 0."},
                       "duration_seconds":{"type":"number","minimum":5,"maximum":120,"description":"How much to measure. Default 30."}
                     }})" },
        Effect::read,
        [this, alivePtr](const std::string& args, ToolContext& ctx) -> ToolResult
        {
            juce::var parsed;
            if (juce::JSON::parse(juce::String(args.empty() ? "{}" : args), parsed).failed() || parsed.getDynamicObject() == nullptr)
                return ToolResult::failure("The arguments must be an object.");
            auto* object = parsed.getDynamicObject();
            const int track = object->hasProperty("track") ? (int) object->getProperty("track") : 0;
            const double start = object->hasProperty("start_seconds") ? (double) object->getProperty("start_seconds") : 0.0;
            const double duration = object->hasProperty("duration_seconds") ? (double) object->getProperty("duration_seconds") : 30.0;

            std::vector<float> mono;
            double sampleRate = 0.0, projectBpm = 0.0;
            std::string error, name;
            bool got = false;
            const bool ran = runOnMessageThread(alivePtr, [&]
            {
                got = host.trackAudio(track, start, duration, mono, sampleRate, error);
                name = track > 0 ? host.trackName(track) : std::string();
                projectBpm = host.projectTempoBpm();
            });
            if (! ran)
                return ToolResult::failure("Station is not available.");
            if (! got)
                return ToolResult::failure(error);
            if (ctx.cancelled())
                return ToolResult::failure("Stopped.");

            creation::audio::TempoOptions options;
            options.expectedBpm = projectBpm;
            const auto result = creation::audio::analyzeTempo(mono.data(), mono.size(), sampleRate, options);
            const std::string label = track > 0 ? "track " + std::to_string(track) + " \"" + name + "\"" : std::string("the whole mix");
            return ToolResult::success(creation::audio::describeTempo(result, label, start, start + (double) mono.size() / sampleRate, projectBpm));
        },
        180.0));

    producerTools.add(std::make_shared<FunctionTool>(
        ToolSpec { "analyze_midi_drums",
                   "Read a MIDI drum track exactly: for each drum (kick, snare, hi-hat...) how many hits, how far each sits from the "
                   "beat grid in milliseconds (ahead or behind), and how hard it is hit (velocity). This is read straight from the "
                   "MIDI, so it is exact, not an estimate. Use it on a MIDI drum track instead of analyze_tempo, which only listens "
                   "to audio.",
                   R"({"type":"object","properties":{
                       "track":{"type":"integer","minimum":1,"description":"The track number, from 1."}
                     },"required":["track"]})" },
        Effect::read,
        [this, alivePtr](const std::string& args, ToolContext&) -> ToolResult
        {
            juce::var parsed;
            if (juce::JSON::parse(juce::String(args), parsed).failed() || parsed.getDynamicObject() == nullptr)
                return ToolResult::failure("The arguments must be an object with a track number.");
            const int track = (int) parsed.getDynamicObject()->getProperty("track");

            std::vector<StationAgentHost::MidiNoteInfo> notes;
            std::string error, name;
            double bpm = 0.0;
            bool got = false;
            const bool ran = runOnMessageThread(alivePtr, [&]
            {
                got = host.trackMidiNotes(track, notes, error);
                name = got ? host.trackName(track) : std::string();
                bpm = host.projectTempoBpm();
            });
            if (! ran)
                return ToolResult::failure("Station is not available.");
            if (! got)
                return ToolResult::failure(error);

            std::vector<creation::audio::DrumHit> hits;
            hits.reserve(notes.size());
            for (const auto& n : notes)
                hits.push_back({ n.pitch, n.velocity, n.beat, n.lengthBeats });
            return ToolResult::success(creation::audio::describeDrums(
                creation::audio::analyzeDrums(hits, bpm), "track " + std::to_string(track) + " \"" + name + "\""));
        }));
}

StationAssistant::~StationAssistant()
{
    *alive = false;
    cancel.cancel();
    runner->requestStop();

    // A script waiting on the message thread stops waiting now that `alive` is false, so the worker ends and the
    // join cannot deadlock.
    if (worker.joinable())
        worker.join();
}

bool StationAssistant::supportsProvider(const juce::String& providerId)
{
    const auto profile = creation::services::SuiteAiProviderRuntime::resolveProfile(providerId);
    return profile.supportsOpenAiChatStyle && ! profile.isOllamaStyle();
}

void StationAssistant::clearConversation()
{
    if (! running)
        history.clear();
}

void StationAssistant::stop()
{
    cancel.cancel();
    runner->requestStop();
}

juce::String StationAssistant::codingGuidance() const
{
    juce::String text;
    text << "\n\nYou can act in Station by writing FRust scripts and running them with the run_frust tool (check_frust only "
            "compiles a script; frust_api_reference shows the API again). Write scripts exactly as the guide below says.\n\n"
         << juce::String(guide)
         << "\n\n## Station's script API (already available to every script; do not declare it)\n\n```\n"
         << juce::String(apiDeclarations) << "```\n";
    return text;
}

juce::String StationAssistant::producerGuidance() const
{
    juce::String text;
    text << "\n\nFor this conversation you are working as the PRODUCER, not the Engineer. You are a thoughtful music producer: "
            "you discuss the arrangement, performance, tuning, timing, tone, balance and feel of the song, give concrete "
            "artistic ideas, and ask what the artist is going for. You do not change the project: you have no tool that can, "
            "and you must not claim to have changed anything. If something should be changed, describe it and offer to hand "
            "it to the Engineer (the user can switch the role).\n\n"
            "You listen through tools. project_overview shows what is in the session. analyze_pitch measures a track's notes: "
            "the exact frequency, and how many cents sharp or flat each is (a cent is a hundredth of a semitone: under about "
            "10 is inaudible to most listeners, 10 to 25 is noticeable to a trained ear, over 25 is clearly out of tune; "
            "a steady lean one way is worth mentioning, and wobble of a few tens of cents can be deliberate vibrato). It "
            "measures one melodic line at a time; it cannot measure chords or a full mix, and it will say so. "
            "analyze_tempo measures the tempo and beat of a track or the whole mix (track 0): the BPM and how sure it is, "
            "whether the playing rushes, drags or drifts, and how tightly the hits sit on the beat (under about 10 ms is very "
            "tight, 10 to 25 ms is a natural human feel, over about 30 ms is audibly loose; leaning ahead of the beat feels "
            "urgent, behind feels laid back). It works from attacks, so it suits drums and plucked or struck sounds; on a smooth "
            "vocal expect low confidence and say so. Compare what it finds with the project's tempo. "
            "For a MIDI drum track use analyze_midi_drums instead: it reads the notes exactly, drum by drum, giving each drum's "
            "distance from the grid in milliseconds, its lean ahead of or behind the beat, and its velocity range. A drum that "
            "leans consistently (snare a little behind, hats a little ahead) is a chosen feel, velocity that varies in a pattern is "
            "a groove, and identical velocity and exact position on every hit means an unhumanized part.\n\n"
            "Numbers alone never say WHY: a bend, a slide, a triplet or swing feel, a laid-back or pushed groove, and vibrato are "
            "expressive, while wobble, sag, drift and slips are faults. Before you judge a measurement, call music_knowledge to check "
            "how to read it, then weigh consistency, context and whether it resolves, offer both readings honestly, and ask the artist "
            "what they meant. Every track is a single instrument, except a MIDI drum track: its timing variation usually comes from "
            "Station's Humanize edit and is deliberate, so judge whether the amount suits the style, never call it sloppy.\n\n"
            "Ground what you say in what you measured: quote the numbers, and point at specific moments and notes (\"the held "
            "note at 0:12 is about 30 cents flat\", \"the fill after 0:20 rushes\") rather than only summarizing. Say which "
            "track and which time, and be honest about how sure you are. Never invent notes, tempos or problems you did not measure. Measure before you judge a "
            "performance's tuning, and tell the user when you have not measured something. Keep a conversational, musical tone; "
            "you are talking with an artist.\n";
    return text;
}

bool StationAssistant::start(const creation::services::SuiteAiResolvedRuntimeSettings& account,
                             const juce::String& systemPrompt,
                             const juce::String& userMessage,
                             const juce::String& rawQuestion,
                             std::function<void(const juce::String&)> onProgress,
                             std::function<void(const Outcome&)> onFinished)
{
    if (running)
        return false;

    if (worker.joinable())
        worker.join();

    // Your suite account becomes a provider.
    const auto profile = creation::services::SuiteAiProviderRuntime::resolveProfile(account.providerId);
    OpenAiProvider::Settings settings;
    settings.displayName = profile.displayName.toStdString();
    settings.baseUrl = creation::services::SuiteAiProviderRuntime::normalizeBaseUrl(account.baseUrl, profile).toStdString();
    settings.chatPath = profile.chatCompletionsPath.toStdString();
    settings.apiKey = account.apiKey.toStdString();
    settings.model = (account.modelName.isNotEmpty() ? account.modelName : creation::services::SuiteAiProviderRuntime::defaultModelName(profile)).toStdString();
    settings.wire.useMaxCompletionTokens = profile.providerId == "openai";
    settings.wire.sendTemperature = ! isReasoningModel(juce::String(settings.model));
    auto provider = std::make_shared<OpenAiProvider>(settings, std::make_shared<JuceHttpClient>());

    AgentRequest request;
    request.model = settings.model;
    const Role runRole = role;   // the role this request runs as, even if the user switches meanwhile
    request.system = (systemPrompt + (runRole == Role::producer ? producerGuidance() : codingGuidance())).toStdString();
    request.history = history;
    request.userMessage = userMessage.toStdString();
    request.limits.maxSteps = 12;
    request.limits.maxSeconds = 300.0;

    // What the user is shown while it works, and what was run (kept for the final message).
    struct Shared
    {
        std::mutex mutex;
        std::vector<juce::String> scripts;      // code of each run_frust call, in order
        std::vector<juce::String> results;      // "ok" or the first error line, for each
    };
    auto shared = std::make_shared<Shared>();
    auto alivePtr = alive;

    auto post = [alivePtr](std::function<void()> work)
    {
        juce::MessageManager::callAsync([alivePtr, work = std::move(work)]
        {
            if (alivePtr->load())
                work();
        });
    };

    request.events.onToolCall = [post, onProgress](const ToolCallBlock& call)
    {
        const juce::String what = call.name == "run_frust" ? "Running a script..."
                                  : call.name == "check_frust" ? "Checking a script..."
                                  : call.name == "analyze_pitch" ? "Listening to the track..."
                                  : call.name == "analyze_tempo" ? "Listening for the beat..."
                                  : call.name == "analyze_midi_drums" ? "Reading the drum notes..."
                                  : call.name == "project_overview" ? "Looking at the project..."
                                                                    : "Looking things up...";
        post([onProgress, what] { if (onProgress) onProgress(what); });
    };
    request.events.onToolResult = [shared, post, onProgress](const ToolCallBlock& call, const ToolResult& result)
    {
        if (call.name == "run_frust")
        {
            std::lock_guard<std::mutex> lock(shared->mutex);
            shared->scripts.push_back(codeOf(call));
            shared->results.push_back(result.ok ? juce::String("ran") : juce::String("failed: ") + shortReason(result.content));
        }
        post([onProgress, ok = result.ok] { if (onProgress) onProgress(ok ? "Working..." : "That did not work; trying again..."); });
    };
    request.events.onNotice = [post, onProgress](const std::string& notice)
    {
        post([onProgress, text = juce::String(notice)] { if (onProgress) onProgress(text); });
    };

    cancel.reset();
    running = true;

    const auto question = rawQuestion;
    worker = std::thread([this, provider, request, shared, post, question, onFinished, runRole]() mutable
    {
        ToolRegistry& registry = runRole == Role::producer ? producerTools : tools;
        AgentEngine engine(*provider, registry);
        const auto result = engine.run(request, cancel);

        Outcome outcome;
        outcome.steps = result.steps;
        outcome.toolCalls = result.toolCalls;
        outcome.finished = result.reason == EndReason::finished;

        juce::String text = juce::String(result.finalText);
        switch (result.reason)
        {
            case EndReason::finished: break;
            case EndReason::stopped: text = "Stopped."; break;
            case EndReason::providerError:
            case EndReason::unsupported:
                outcome.error = juce::String(result.detail);
                text = outcome.error;
                break;
            default:
                text = (text.isNotEmpty() ? text + "\n\n" : juce::String()) + "I stopped early: " + juce::String(result.detail);
                break;
        }

        // Show what was run, so nothing the assistant did is hidden.
        {
            std::lock_guard<std::mutex> lock(shared->mutex);
            if (! shared->scripts.empty())
            {
                text << "\n\n**Scripts run**\n";
                for (size_t i = 0; i < shared->scripts.size(); ++i)
                {
                    text << "\n```\n" << shared->scripts[i].trim() << "\n```\n";
                    text << (shared->results[i] == "ran" ? "-> ran" : "-> " + shared->results[i]) << "\n";
                }
            }
        }
        outcome.text = text;

        // Remember the conversation: what the user actually typed, not the context added to it.
        {
            auto messages = result.messages;
            if (! messages.empty() && messages[0].role == creation::ai::Role::user)
                messages[0] = creation::ai::Message::text(creation::ai::Role::user, question.toStdString());
            const bool keep = result.reason == EndReason::finished || result.reason == EndReason::stepLimit;
            if (keep)
            {
                // Only whole exchanges are kept: never leave a tool call without its result.
                history.insert(history.end(), messages.begin(), messages.end());
                AgentEngine::trimToFit(history, 120000);
            }
        }

        running = false;
        post([onFinished, outcome] { if (onFinished) onFinished(outcome); });
    });
    return true;
}
