#include "OpenAiChatClient.h"

namespace
{
juce::var makeMessage(const juce::String& role, const juce::String& content)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("role", role);
    object->setProperty("content", content);
    return juce::var(object);
}
}

juce::String OpenAiChatClient::normaliseBaseUrl(const juce::String& baseUrl)
{
    auto trimmed = baseUrl.trim();
    if (trimmed.isEmpty())
        return "https://api.openai.com/v1";

    return trimmed.endsWithChar('/') ? trimmed.dropLastCharacters(1) : trimmed;
}

juce::String OpenAiChatClient::buildHeaders(const juce::String& apiKey)
{
    return "Authorization: Bearer " + apiKey + "\r\n"
           "Content-Type: application/json\r\n"
           "Accept: application/json\r\n";
}

bool OpenAiChatClient::sendChatCompletion(const AiProviderSettings& settings,
                                          const juce::String& systemPrompt,
                                          const juce::String& userPrompt,
                                          ChatResult& result) const
{
    result = {};

    auto providerName = settings.providerName.toLowerCase();
    auto isOllama = providerName.contains("ollama");

    if (! isOllama && settings.apiKey.trim().isEmpty())
    {
        result.errorMessage = "Enter your OpenAI API key in Settings.";
        return false;
    }

    auto model = settings.modelName.trim();
    if (model.isEmpty())
        model = "gpt-4.1-mini";

    juce::Array<juce::var> messages;
    if (systemPrompt.isNotEmpty())
        messages.add(makeMessage("system", systemPrompt));
    messages.add(makeMessage("user", userPrompt));

    auto* root = new juce::DynamicObject();
    root->setProperty("model", model);
    root->setProperty("temperature", 0.4);

    auto urlPath = "/chat/completions";
    auto headers = buildHeaders(settings.apiKey);
    if (isOllama)
    {
        root->setProperty("messages", juce::var(messages));
        root->setProperty("stream", false);
        urlPath = "/api/chat";
        headers = "Content-Type: application/json\r\nAccept: application/json\r\n";
    }
    else
    {
        root->setProperty("messages", juce::var(messages));
    }

    auto body = juce::JSON::toString(juce::var(root), false);

    auto url = juce::URL(normaliseBaseUrl(settings.baseUrl) + urlPath).withPOSTData(body);
    int statusCode = 0;
    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                            .withHttpRequestCmd("POST")
                                            .withConnectionTimeoutMs(30000)
                                            .withStatusCode(&statusCode)
                                            .withExtraHeaders(headers));

    if (stream == nullptr)
    {
        result.errorMessage = "Could not connect to the OpenAI API.";
        return false;
    }

    result.statusCode = statusCode;
    result.rawResponse = stream->readEntireStreamAsString();

    if (statusCode < 200 || statusCode >= 300)
    {
        result.errorMessage = "OpenAI error (HTTP " + juce::String(statusCode) + "): " + result.rawResponse.substring(0, 300);
        return false;
    }

    auto parsed = juce::JSON::parse(result.rawResponse);
    if (! parsed.isObject())
    {
        result.errorMessage = "OpenAI returned invalid JSON.";
        return false;
    }

    auto* object = parsed.getDynamicObject();
    if (object == nullptr)
    {
        result.errorMessage = "OpenAI response was missing its root object.";
        return false;
    }

    if (isOllama)
    {
        auto message = object->getProperty("message");
        auto* messageObject = message.getDynamicObject();
        if (messageObject == nullptr)
        {
            result.errorMessage = "Ollama response was missing its message object.";
            return false;
        }

        result.text = messageObject->getProperty("content").toString().trim();
    }
    else
    {
        auto choices = object->getProperty("choices");
        if (! choices.isArray() || choices.getArray() == nullptr || choices.getArray()->size() == 0)
        {
            result.errorMessage = "OpenAI response did not include any choices.";
            return false;
        }

        auto firstChoice = choices.getArray()->getReference(0);
        auto* choiceObject = firstChoice.getDynamicObject();
        if (choiceObject == nullptr)
        {
            result.errorMessage = "OpenAI response choice was malformed.";
            return false;
        }

        auto message = choiceObject->getProperty("message");
        auto* messageObject = message.getDynamicObject();
        if (messageObject == nullptr)
        {
            result.errorMessage = "OpenAI response choice did not include a message.";
            return false;
        }

        result.text = messageObject->getProperty("content").toString().trim();
    }

    if (result.text.isEmpty())
        result.text = "(The model returned an empty response.)";

    return true;
}
