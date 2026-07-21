#include "OpenAiModelCatalogClient.h"

namespace
{
juce::String buildHeaders(const juce::String& apiKey)
{
    if (apiKey.trim().isEmpty())
        return "Accept: application/json\r\n";

    return "Authorization: Bearer " + apiKey + "\r\n"
           "Accept: application/json\r\n";
}
}

juce::String OpenAiModelCatalogClient::normaliseBaseUrl(const juce::String& baseUrl)
{
    auto trimmed = baseUrl.trim();
    if (trimmed.isEmpty())
        return "https://api.openai.com/v1";

    return trimmed.endsWithChar('/') ? trimmed.dropLastCharacters(1) : trimmed;
}

bool OpenAiModelCatalogClient::fetchModelIds(const juce::String& baseUrl,
                                             const juce::String& providerName,
                                             const juce::String& apiKey,
                                             juce::StringArray& modelIds,
                                             juce::String& errorMessage) const
{
    modelIds.clear();

    auto isOllama = providerName.toLowerCase().contains("ollama");

    if (! isOllama && apiKey.trim().isEmpty())
    {
        errorMessage = "Enter your OpenAI API key before fetching models.";
        return false;
    }

    auto path = isOllama ? "/api/tags" : "/models";
    auto url = juce::URL(normaliseBaseUrl(baseUrl) + path);
    int statusCode = 0;
    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                            .withHttpRequestCmd("GET")
                                            .withConnectionTimeoutMs(15000)
                                            .withStatusCode(&statusCode)
                                            .withExtraHeaders(buildHeaders(apiKey)));

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

    auto listProperty = object->getProperty(isOllama ? "models" : "data");
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
                auto id = itemObject->getProperty(isOllama ? "name" : "id").toString().trim();
                if (id.isNotEmpty())
                    modelIds.addIfNotAlreadyThere(id);
            }
        }
    }

    modelIds.sort(false);
    return modelIds.size() > 0;
}
