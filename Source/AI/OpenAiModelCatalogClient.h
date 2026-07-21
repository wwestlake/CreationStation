#pragma once

#include <JuceHeader.h>

class OpenAiModelCatalogClient final
{
public:
    bool fetchModelIds(const juce::String& baseUrl,
                       const juce::String& providerName,
                       const juce::String& apiKey,
                       juce::StringArray& modelIds,
                       juce::String& errorMessage) const;

private:
    static juce::String normaliseBaseUrl(const juce::String& baseUrl);
};
