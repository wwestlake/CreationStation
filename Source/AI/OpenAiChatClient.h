#pragma once

#include <JuceHeader.h>
#include "AiProviderSettings.h"

class OpenAiChatClient final
{
public:
    struct ChatResult
    {
        juce::String text;
        juce::String rawResponse;
        juce::String errorMessage;
        int statusCode = 0;
    };

    bool sendChatCompletion(const AiProviderSettings& settings,
                            const juce::String& systemPrompt,
                            const juce::String& userPrompt,
                            ChatResult& result) const;

private:
    static juce::String normaliseBaseUrl(const juce::String& baseUrl);
    static juce::String buildHeaders(const juce::String& apiKey);
};
