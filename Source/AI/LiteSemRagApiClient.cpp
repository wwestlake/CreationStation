#include "LiteSemRagApiClient.h"

juce::String LiteSemRagApiClient::apiBase()
{
    return "https://lagdaemon.com/djehuti";
}

juce::String LiteSemRagApiClient::buildAuthHeaders(const juce::String& bearerToken)
{
    return "Authorization: Bearer " + bearerToken + "\r\n"
           "Accept: application/json\r\n";
}

bool LiteSemRagApiClient::readJsonResponse(const juce::String& url,
                                           const juce::String& httpVerb,
                                           const juce::String& bearerToken,
                                           const juce::String& body,
                                           int& statusCode,
                                           juce::String& responseText,
                                           juce::var& parsed,
                                           juce::String& errorMessage)
{
    auto targetUrl = juce::URL(url);
    if (body.isNotEmpty())
        targetUrl = targetUrl.withPOSTData(body);

    auto parameterHandling = (httpVerb == "GET" && body.isEmpty())
        ? juce::URL::ParameterHandling::inAddress
        : juce::URL::ParameterHandling::inPostData;

    auto stream = targetUrl.createInputStream(juce::URL::InputStreamOptions(parameterHandling)
                                                  .withHttpRequestCmd(httpVerb)
                                                  .withConnectionTimeoutMs(15000)
                                                  .withStatusCode(&statusCode)
                                                  .withExtraHeaders(buildAuthHeaders(bearerToken)));

    if (stream == nullptr)
    {
        errorMessage = "Could not reach the LiteSemRAG service.";
        return false;
    }

    responseText = stream->readEntireStreamAsString();
    if (statusCode < 200 || statusCode >= 300)
    {
        errorMessage = "LiteSemRAG error (HTTP " + juce::String(statusCode) + "): " + responseText.substring(0, 200);
        return false;
    }

    parsed = juce::JSON::parse(responseText);
    if (parsed.isVoid())
    {
        errorMessage = "LiteSemRAG returned invalid JSON.";
        return false;
    }

    return true;
}

bool LiteSemRagApiClient::fetchAppContextInfo(const juce::String& bearerToken,
                                              const juce::String& appName,
                                              AppContextInfo& info,
                                              juce::String& errorMessage) const
{
    int statusCode = 0;
    juce::String responseText;
    juce::var parsed;

    if (! readJsonResponse(apiBase() + "/api/semantic/app-context/" + juce::URL::addEscapeChars(appName, false),
                           "GET",
                           bearerToken,
                           {},
                           statusCode,
                           responseText,
                           parsed,
                           errorMessage))
        return false;

    auto* object = parsed.getDynamicObject();
    if (object == nullptr)
    {
        errorMessage = "The app-context response was missing its data.";
        return false;
    }

    info.appName = object->getProperty("appName").toString();
    info.version = object->getProperty("version").toString();
    info.checksum = object->getProperty("checksum").toString();
    info.lastUpdated = object->getProperty("lastUpdated").toString();
    return info.appName.isNotEmpty();
}

bool LiteSemRagApiClient::publishAppContext(const juce::String& bearerToken,
                                            const juce::String& appName,
                                            const CreationStationAppManifest& manifest,
                                            AppContextInfo& info,
                                            juce::String& errorMessage) const
{
    auto body = manifest.toJson();

    int statusCode = 0;
    juce::String responseText;
    juce::var parsed;

    if (! readJsonResponse(apiBase() + "/api/semantic/app-context/" + juce::URL::addEscapeChars(appName, false),
                           "POST",
                           bearerToken,
                           body,
                           statusCode,
                           responseText,
                           parsed,
                           errorMessage))
        return false;

    auto* object = parsed.getDynamicObject();
    if (object == nullptr)
    {
        errorMessage = "The app-context upload response was missing its data.";
        return false;
    }

    info.appName = object->getProperty("appName").toString();
    info.version = object->getProperty("version").toString();
    info.checksum = object->getProperty("checksum").toString();
    info.lastUpdated = object->getProperty("timestamp").toString();
    return info.appName.isNotEmpty();
}

bool LiteSemRagApiClient::syncAppContext(const juce::String& bearerToken,
                                         const juce::String& appName,
                                         const CreationStationAppManifest& manifest,
                                         AppContextInfo& info,
                                         juce::String& errorMessage) const
{
    AppContextInfo remoteInfo;
    if (fetchAppContextInfo(bearerToken, appName, remoteInfo, errorMessage)
        && remoteInfo.checksum == manifest.checksum()
        && remoteInfo.version == manifest.version)
    {
        info = remoteInfo;
        return true;
    }

    if (! publishAppContext(bearerToken, appName, manifest, info, errorMessage))
        return false;

    return true;
}
