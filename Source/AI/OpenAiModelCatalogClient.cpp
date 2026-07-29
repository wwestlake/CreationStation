#include "OpenAiModelCatalogClient.h"
#include <creation/services/SuiteAiProviderRuntime.h>

juce::String OpenAiModelCatalogClient::normaliseBaseUrl(const juce::String& baseUrl)
{
    return creation::services::SuiteAiProviderRuntime::normalizeBaseUrl(
        baseUrl, creation::services::SuiteAiProviderRuntime::resolveProfile("openai"));
}

bool OpenAiModelCatalogClient::fetchModelIds(const juce::String& baseUrl,
                                             const juce::String& providerName,
                                             const juce::String& apiKey,
                                             juce::StringArray& modelIds,
                                             juce::String& errorMessage) const
{
    modelIds.clear();

    const auto profile = creation::services::SuiteAiProviderRuntime::resolveProfile(providerName);

    if (creation::services::SuiteAiProviderRuntime::requiresApiKey(profile, apiKey))
    {
        errorMessage = "Enter your provider API key before fetching models.";
        return false;
    }

    auto url = juce::URL(creation::services::SuiteAiProviderRuntime::normalizeBaseUrl(baseUrl, profile)
                         + profile.modelCatalogPath);
    int statusCode = 0;
    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                            .withHttpRequestCmd("GET")
                                            .withConnectionTimeoutMs(15000)
                                            .withStatusCode(&statusCode)
                                            .withExtraHeaders(creation::services::SuiteAiProviderRuntime::buildAuthHeaders(profile, apiKey)));

    if (stream == nullptr)
    {
        errorMessage = "Could not reach the model list endpoint.";
        return false;
    }

    auto responseText = stream->readEntireStreamAsString();
    if (statusCode < 200 || statusCode >= 300)
    {
        errorMessage = "Model list request failed (HTTP " + juce::String(statusCode) + "): "
                       + responseText.substring(0, 200);
        return false;
    }

    auto parsed = juce::JSON::parse(responseText);
    if (! parsed.isObject())
    {
        errorMessage = "Model list response was not valid JSON.";
        return false;
    }

    auto* object = parsed.getDynamicObject();
    if (object == nullptr)
    {
        errorMessage = "Model list response was missing its root object.";
        return false;
    }

    auto listProperty = object->getProperty(profile.modelCatalogArrayProperty);
    if (! listProperty.isArray())
    {
        errorMessage = "Model list response was missing the model array.";
        return false;
    }

    if (auto* array = listProperty.getArray())
    {
        for (const auto& item : *array)
        {
            if (auto* itemObject = item.getDynamicObject())
            {
                auto id = itemObject->getProperty(profile.modelCatalogIdProperty).toString().trim();
                if (id.isNotEmpty())
                    modelIds.addIfNotAlreadyThere(id);
            }
        }
    }

    modelIds.sort(false);
    return modelIds.size() > 0;
}
