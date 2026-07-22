#pragma once

#include <JuceHeader.h>

class ContentApiClient final
{
public:
    struct LibraryItem
    {
        juce::String id;
        juce::String name;
        juce::String itemType;
        juce::String version;
        juce::String description;
        juce::StringArray tags;
        juce::String requiredTier;
        juce::String accessState;
        juce::String minAppVersion;
        juce::String fileType;
        int64 sizeBytes = 0;
        juce::String updatedAt;
    };

    struct AdminUploadRequest
    {
        juce::String productSlug;
        juce::String name;
        juce::String itemType;
        juce::String version;
        juce::String description;
        juce::StringArray tags;
        juce::String requiredTierId;
        juce::String minAppVersion;
        juce::String fileType;
        juce::File packageFile;
    };

    bool fetchLibrary(const juce::String& bearerToken,
                      const juce::String& productSlug,
                      juce::Array<LibraryItem>& items,
                      juce::String& errorMessage) const;

    bool createAdminContent(const juce::String& bearerToken,
                            const AdminUploadRequest& request,
                            juce::String& createdItemId,
                            juce::String& errorMessage) const;

    bool uploadAdminContentFile(const juce::String& bearerToken,
                                const juce::String& contentId,
                                const juce::File& file,
                                juce::String& errorMessage) const;

    bool downloadContentItem(const juce::String& bearerToken,
                             const juce::String& contentId,
                             const juce::File& destinationFile,
                             juce::String& errorMessage) const;

private:
    static juce::String apiBase();
    static juce::String buildAuthHeaders(const juce::String& bearerToken, const juce::String& extraHeaders = {});
    static bool readJsonResponse(const juce::String& url,
                                 const juce::String& httpVerb,
                                 const juce::String& bearerToken,
                                 const juce::String& body,
                                 const juce::String& contentType,
                                 int& statusCode,
                                 juce::String& responseText,
                                 juce::var& parsed,
                                 juce::String& errorMessage);
};
