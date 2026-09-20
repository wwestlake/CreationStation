#include "StationAssistant.h"

#include "StationScriptApi.h"

#include <creation/services/SuiteAiProviderRuntime.h>

#include <chrono>
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

juce::String firstLine(const std::string& text)
{
    return juce::String(text).upToFirstOccurrenceOf("\n", false, false).trim();
}
}

StationAssistant::StationAssistant(StationAgentHost& hostToUse, std::string declarations)
    : host(hostToUse), apiDeclarations(std::move(declarations))
{
    auto api = station_script::makeApi(host, apiDeclarations);
    runner = std::make_unique<creation::frust::ScriptRunner>(
        api, [alivePtr = alive](const std::function<void()>& work) { return runOnMessageThread(alivePtr, work); });

    tools.add(makeRunFrustTool(*runner));
    tools.add(makeCheckFrustTool(*runner));
    tools.add(makeFrustApiReferenceTool(runner->api()));
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
            "compiles; frust_api_reference shows the API again). FRust is the suite's language. A script looks like this:\n\n"
            "```\npub fn run() -> String = {\n    let t = station_track_add(\"Bass\");\n    station_track_set_volume_db(t, -6.0);\n"
            "    station_log_i64(\"track\", t);\n    \"Added Bass\"\n}\n```\n\n"
            "Rules that matter:\n"
            "- The script must define `pub fn run() -> String`. The last expression is its result. Statements end with `;`.\n"
            "- A `while (condition) { ... }` block needs a `;` after its closing brace. `if (condition) { a } else { b }` "
            "gives a value. Comments start with `//`.\n"
            "- Whole numbers are i64 (`3`); decimals are f64 and must be written with a point (`-6.0`, `0.5`).\n"
            "- Tracks are numbered from 1. Functions that change something return 1 when they worked and 0 when they did "
            "not; call `station_last_error()` after a 0 to learn why.\n"
            "- To report a number use station_log_i64 or station_log_f64; to report text use station_log or return it.\n"
            "- If run_frust returns compile errors, they name the line and column of your script: fix that and run again. "
            "Do not repeat a call that failed the same way.\n"
            "- Do the work, then check it by reading the project back if it matters, and tell the user briefly what "
            "you did. Never say something worked unless a run confirmed it. Only change what was asked.\n\n"
            "Station's API for scripts (already available to every script; you do not need to declare it):\n\n"
         << juce::String(apiDeclarations);
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
    request.system = (systemPrompt + codingGuidance()).toStdString();
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
                                                               : "Looking things up...";
        post([onProgress, what] { if (onProgress) onProgress(what); });
    };
    request.events.onToolResult = [shared, post, onProgress](const ToolCallBlock& call, const ToolResult& result)
    {
        if (call.name == "run_frust")
        {
            std::lock_guard<std::mutex> lock(shared->mutex);
            shared->scripts.push_back(codeOf(call));
            shared->results.push_back(result.ok ? juce::String("ran") : juce::String("failed: ") + firstLine(result.content).substring(0, 160));
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
    worker = std::thread([this, provider, request, shared, post, question, onFinished]() mutable
    {
        ToolRegistry& registry = tools;
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
            if (! messages.empty() && messages[0].role == Role::user)
                messages[0] = creation::ai::Message::text(Role::user, question.toStdString());
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
