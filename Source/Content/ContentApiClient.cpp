#include "ContentApiClient.h"

namespace
{
juce::String joinTagsJson(const juce::StringArray& tags)
{
    juce::StringArray encoded;
    for (const auto& tag : tags)
        encoded.add(juce::JSON::toString(tag));
    return "[" + encoded.joinIntoString(",") + "]";
}
}

juce::String ContentApiClient::apiBase()
{
    return "https://lagdaemon.com/djehuti";
}

juce::String ContentApiClient::buildAuthHeaders(const juce::String& bearerToken, const juce::String& extraHeaders)
{
    auto headers = "Authorization: Bearer " + bearerToken + "\r\nAccept: application/json\r\n";
    if (extraHeaders.isNotEmpty())
        headers += extraHeaders;
    return headers;
}

bool ContentApiClient::readJsonResponse(const juce::String& url,
                                        const juce::String& httpVerb,
                                        const juce::String& bearerToken,
                                        const juce::String& body,
                                        const juce::String& contentType,
                                        int& statusCode,
                                        juce::String& responseText,
                                        juce::var& parsed,
                                        juce::String& errorMessage)
{
    auto targetUrl = juce::URL(url);
    if (body.isNotEmpty())
        targetUrl = targetUrl.withPOSTData(body);

    auto extraHeaders = contentType.isNotEmpty() ? ("Content-Type: " + contentType + "\r\n") : juce::String{};
    auto parameterHandling = (httpVerb == "GET" && body.isEmpty())
        ? juce::URL::ParameterHandling::inAddress
        : juce::URL::ParameterHandling::inPostData;

    auto stream = targetUrl.createInputStream(juce::URL::InputStreamOptions(parameterHandling)
                                                  .withHttpRequestCmd(httpVerb)
                                                  .withConnectionTimeoutMs(20000)
                                                  .withStatusCode(&statusCode)
                                                  .withExtraHeaders(buildAuthHeaders(bearerToken, extraHeaders)));

    if (stream == nullptr)
    {
        errorMessage = "Could not reach the LagDaemon content service.";
        return false;
    }

    responseText = stream->readEntireStreamAsString();
    if (statusCode < 200 || statusCode >= 300)
    {
        errorMessage = "Content service error (HTTP " + juce::String(statusCode) + "): " + responseText.substring(0, 200);
        return false;
    }

    parsed = juce::JSON::parse(responseText);
    if (parsed.isVoid())
    {
        errorMessage = "The content service returned invalid JSON.";
        return false;
    }

    return true;
}

bool ContentApiClient::fetchLibrary(const juce::String& bearerToken,
                                    const juce::String& productSlug,
                                    juce::Array<LibraryItem>& items,
                                    juce::String& errorMessage) const
{
    int statusCode = 0;
    juce::String responseText;
    juce::var parsed;
    if (! readJsonResponse(apiBase() + "/api/content/library?product=" + juce::URL::addEscapeChars(productSlug, false),
                           "GET",
                           bearerToken,
                           {},
                           {},
                           statusCode,
                           responseText,
                           parsed,
                           errorMessage))
        return false;

    items.clearQuick();
    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
    {
        errorMessage = "The library response was missing its item list.";
        return false;
    }

    if (auto* array = root->getProperty("items").getArray())
    {
        for (const auto& value : *array)
        {
            if (auto* object = value.getDynamicObject())
            {
                LibraryItem item;
                item.id = object->getProperty("id").toString();
                item.name = object->getProperty("name").toString();
                item.itemType = object->getProperty("itemType").toString();
                item.version = object->getProperty("version").toString();
                item.description = object->getProperty("description").toString();
                item.requiredTier = object->getProperty("requiredTier").toString();
                item.accessState = object->getProperty("accessState").toString();
                item.minAppVersion = object->getProperty("minAppVersion").toString();
                item.fileType = object->getProperty("fileType").toString();
                item.sizeBytes = (int64) object->getProperty("sizeBytes");
                item.updatedAt = object->getProperty("updatedAt").toString();

                if (auto* tags = object->getProperty("tags").getArray())
                    for (const auto& tag : *tags)
                        item.tags.add(tag.toString());

                items.add(item);
            }
        }
    }

    return true;
}

bool ContentApiClient::createAdminContent(const juce::String& bearerToken,
                                          const AdminUploadRequest& request,
                                          juce::String& createdItemId,
                                          juce::String& errorMessage) const
{
    auto requiredTierJson = request.requiredTierId.isNotEmpty() ? juce::JSON::toString(request.requiredTierId) : "null";
    auto minAppVersionJson = request.minAppVersion.isNotEmpty() ? juce::JSON::toString(request.minAppVersion) : "null";

    auto body = "{"
              "\"productSlug\":" + juce::JSON::toString(request.productSlug) + ","
              "\"name\":" + juce::JSON::toString(request.name) + ","
              "\"itemType\":" + juce::JSON::toString(request.itemType) + ","
              "\"version\":" + juce::JSON::toString(request.version) + ","
              "\"description\":" + juce::JSON::toString(request.description) + ","
              "\"tags\":" + joinTagsJson(request.tags) + ","
              "\"requiredTierId\":" + requiredTierJson + ","
              "\"minAppVersion\":" + minAppVersionJson + ","
              "\"fileType\":" + juce::JSON::toString(request.fileType) +
              "}";

    int statusCode = 0;
    juce::String responseText;
    juce::var parsed;
    if (! readJsonResponse(apiBase() + "/api/admin/content",
                           "POST",
                           bearerToken,
                           body,
                           "application/json",
                           statusCode,
                           responseText,
                           parsed,
                           errorMessage))
        return false;

    if (auto* object = parsed.getDynamicObject())
    {
        createdItemId = object->getProperty("id").toString();
        return createdItemId.isNotEmpty();
    }

    errorMessage = "The admin content create response was missing the new item id.";
    return false;
}

bool ContentApiClient::uploadAdminContentFile(const juce::String& bearerToken,
                                              const juce::String& contentId,
                                              const juce::File& file,
                                              juce::String& errorMessage) const
{
    if (! file.existsAsFile())
    {
        errorMessage = "The selected package file does not exist.";
        return false;
    }

    juce::MemoryBlock bytes;
    if (! file.loadFileAsData(bytes))
    {
        errorMessage = "Could not read the selected package file.";
        return false;
    }
    auto url = juce::URL(apiBase() + "/api/admin/content/" + contentId + "/file?fileName="
                         + juce::URL::addEscapeChars(file.getFileName(), false))
                   .withPOSTData(bytes);

    int statusCode = 0;
    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                            .withHttpRequestCmd("POST")
                                            .withConnectionTimeoutMs(30000)
                                            .withStatusCode(&statusCode)
                                            .withExtraHeaders(buildAuthHeaders(bearerToken, "Content-Type: application/octet-stream\r\n")));

    if (stream == nullptr)
    {
        errorMessage = "Could not upload the content package.";
        return false;
    }

    juce::ignoreUnused(stream->readEntireStreamAsString());
    if (statusCode < 200 || statusCode >= 300)
    {
        errorMessage = "Package upload failed (HTTP " + juce::String(statusCode) + ").";
        return false;
    }

    return true;
}

bool ContentApiClient::downloadContentItem(const juce::String& bearerToken,
                                           const juce::String& contentId,
                                           const juce::File& destinationFile,
                                           juce::String& errorMessage) const
{
    destinationFile.getParentDirectory().createDirectory();

    auto url = juce::URL(apiBase() + "/api/content/" + contentId + "/download");
    int statusCode = 0;
    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                            .withHttpRequestCmd("POST")
                                            .withConnectionTimeoutMs(30000)
                                            .withStatusCode(&statusCode)
                                            .withExtraHeaders(buildAuthHeaders(bearerToken)));

    if (stream == nullptr)
    {
        errorMessage = "Could not reach the LagDaemon download service.";
        return false;
    }

    if (statusCode < 200 || statusCode >= 300)
    {
        auto responseText = stream->readEntireStreamAsString();
        errorMessage = "Download failed (HTTP " + juce::String(statusCode) + "): " + responseText.substring(0, 200);
        return false;
    }

    juce::TemporaryFile tempFile(destinationFile);
    {
        juce::FileOutputStream output(tempFile.getFile());
        if (! output.openedOk())
        {
            errorMessage = "Could not create the local download file.";
            return false;
        }

        output << *stream;
        output.flush();
    }

    if (! tempFile.overwriteTargetFileWithTemporary())
    {
        errorMessage = "Could not finalize the downloaded content file.";
        return false;
    }

    return true;
}
