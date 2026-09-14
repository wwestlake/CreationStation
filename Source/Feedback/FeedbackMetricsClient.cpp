#include "FeedbackMetricsClient.h"

namespace creation_station
{
namespace
{
juce::String appVersionString()
{
    return ProjectInfo::versionString;
}

juce::String osInfoString()
{
    return juce::SystemStats::getOperatingSystemName();
}

bool postJson(const juce::String& url, const juce::String& body, const juce::String& bearerToken,
             juce::String& errorMessage)
{
    auto targetUrl = juce::URL(url).withPOSTData(body);

    auto headers = juce::String("Content-Type: application/json\r\nAccept: application/json\r\n");
    if (bearerToken.isNotEmpty())
        headers += "Authorization: Bearer " + bearerToken + "\r\n";

    int statusCode = 0;
    auto stream = targetUrl.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                                  .withHttpRequestCmd("POST")
                                                  .withConnectionTimeoutMs(15000)
                                                  .withStatusCode(&statusCode)
                                                  .withExtraHeaders(headers));
    if (stream == nullptr)
    {
        errorMessage = "Could not reach the feedback service.";
        return false;
    }

    auto responseText = stream->readEntireStreamAsString();
    if (statusCode < 200 || statusCode >= 300)
    {
        errorMessage = "Feedback service error (HTTP " + juce::String(statusCode) + "): " + responseText.substring(0, 200);
        return false;
    }

    return true;
}
}

juce::String FeedbackMetricsClient::apiBase()
{
    return "https://lagdaemon.com/djehuti";
}

bool FeedbackMetricsClient::submitFeedback(const juce::String& installId,
                                           const juce::String& message,
                                           const juce::String& category,
                                           const juce::String& bearerToken,
                                           juce::String& errorMessage) const
{
    if (installId.isEmpty())
    {
        errorMessage = "No install identity available yet.";
        return false;
    }

    if (message.trim().isEmpty())
    {
        errorMessage = "A message is required.";
        return false;
    }

    auto body = juce::String("{")
              + "\"installId\":" + juce::JSON::toString(installId) + ","
              + "\"message\":" + juce::JSON::toString(message) + ","
              + "\"category\":" + juce::JSON::toString(category.isNotEmpty() ? category : juce::String("general")) + ","
              + "\"appVersion\":" + juce::JSON::toString(appVersionString()) + ","
              + "\"osInfo\":" + juce::JSON::toString(osInfoString())
              + "}";

    return postJson(apiBase() + "/api/products/creation-station/feedback", body, bearerToken, errorMessage);
}

bool FeedbackMetricsClient::submitMetricsBatch(const juce::String& installId,
                                               const juce::Array<MetricEvent>& events,
                                               const juce::String& bearerToken,
                                               juce::String& errorMessage) const
{
    if (installId.isEmpty())
    {
        errorMessage = "No install identity available yet.";
        return false;
    }

    if (events.isEmpty())
    {
        errorMessage = "No events to send.";
        return false;
    }

    juce::StringArray eventsJson;
    for (const auto& ev : events)
    {
        eventsJson.add(juce::String("{")
            + "\"eventType\":" + juce::JSON::toString(ev.eventType) + ","
            + "\"eventName\":" + juce::JSON::toString(ev.eventName) + ","
            + "\"payloadJson\":" + juce::JSON::toString(ev.payloadJson.isNotEmpty() ? ev.payloadJson : juce::String("{}"))
            + "}");
    }

    auto body = juce::String("{")
              + "\"installId\":" + juce::JSON::toString(installId) + ","
              + "\"appVersion\":" + juce::JSON::toString(appVersionString()) + ","
              + "\"osInfo\":" + juce::JSON::toString(osInfoString()) + ","
              + "\"events\":[" + eventsJson.joinIntoString(",") + "]"
              + "}";

    return postJson(apiBase() + "/api/products/creation-station/metrics", body, bearerToken, errorMessage);
}
}
