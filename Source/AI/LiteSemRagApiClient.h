#pragma once

#include <JuceHeader.h>
#include "CreationStationAppManifest.h"

class LiteSemRagApiClient final
{
public:
    struct AppContextInfo
    {
        juce::String appName;
        juce::String version;
        juce::String checksum;
        juce::String lastUpdated;
    };

    bool fetchAppContextInfo(const juce::String& bearerToken,
                             const juce::String& appName,
                             AppContextInfo& info,
                             juce::String& errorMessage) const;

    bool publishAppContext(const juce::String& bearerToken,
                           const juce::String& appName,
                           const CreationStationAppManifest& manifest,
                           AppContextInfo& info,
                           juce::String& errorMessage) const;

    bool syncAppContext(const juce::String& bearerToken,
                        const juce::String& appName,
                        const CreationStationAppManifest& manifest,
                        AppContextInfo& info,
                        juce::String& errorMessage) const;

private:
    static juce::String apiBase();
    static juce::String buildAuthHeaders(const juce::String& bearerToken);
    static bool readJsonResponse(const juce::String& url,
                                 const juce::String& httpVerb,
                                 const juce::String& bearerToken,
                                 const juce::String& body,
                                 int& statusCode,
                                 juce::String& responseText,
                                 juce::var& parsed,
                                 juce::String& errorMessage);
};
