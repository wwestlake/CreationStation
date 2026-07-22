#pragma once

#include <JuceHeader.h>

struct AiProviderSettings
{
    juce::String providerName { "openai" };
    juce::String baseUrl { "https://api.openai.com/v1" };
    juce::String modelName { "gpt-4.1-mini" };
    juce::String apiKey;
};
