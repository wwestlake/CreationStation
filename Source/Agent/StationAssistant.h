#pragma once

#include "StationAgentHost.h"

#include <JuceHeader.h>

#include <creation/ai/AgentEngine.h>
#include <creation/ai/FrustTools.h>
#include <creation/ai/OpenAiProvider.h>
#include <creation/frust/ScriptRunner.h>
#include <creation/services/SuiteAiSettings.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

// The Virtual Engineer as a coder. It joins the pieces: your suite AI account becomes a provider, the agent
// engine runs the conversation, the model writes FRust and runs it through run_frust against Station's script API,
// and what happens is reported back to the panel. The engine runs on a thread of its own; Station's project is only
// ever touched on the message thread.
class StationAssistant
{
public:
    struct Outcome
    {
        bool finished = false;          // the model gave a final answer (not stopped, not a limit or an error)
        juce::String text;              // what to show: the answer, with the scripts that were run
        juce::String error;             // set when the run could not start or a provider error ended it
        int steps = 0;
        int toolCalls = 0;
    };

    // `apiDeclarations` is the text of StationAgentApi.frust and `guide` the text of StationScriptGuide.md (how to
    // write FRust for Station). The host must outlive the assistant.
    StationAssistant(StationAgentHost& host, std::string apiDeclarations, std::string guide);
    ~StationAssistant();

    // Whether the agent path can talk to this provider yet (the OpenAI chat-completions protocol, which OpenAI and
    // many compatible providers use).
    static bool supportsProvider(const juce::String& providerId);

    // Starts a request on the assistant's own thread. `onProgress` (a short status, on the message thread) and
    // `onFinished` (on the message thread) are called as it goes. `rawQuestion` is what the user typed, kept in the
    // conversation in place of `userMessage`, which also carries help and project context for this turn only.
    // False if a request is already running.
    bool start(const creation::services::SuiteAiResolvedRuntimeSettings& account,
               const juce::String& systemPrompt,
               const juce::String& userMessage,
               const juce::String& rawQuestion,
               std::function<void(const juce::String& status)> onProgress,
               std::function<void(const Outcome& outcome)> onFinished);

    void stop();
    bool isRunning() const noexcept { return running; }
    void clearConversation();

    // What the model is told about writing FRust for Station, and Station's API. Added to the system prompt.
    juce::String codingGuidance() const;

private:
    StationAgentHost& host;
    std::string apiDeclarations;
    std::string guide;
    std::unique_ptr<creation::frust::ScriptRunner> runner;
    creation::ai::ToolRegistry tools;
    creation::ai::CancelToken cancel;
    std::vector<creation::ai::Message> history;
    std::thread worker;
    std::atomic<bool> running { false };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};
