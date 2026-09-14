#pragma once

#include <JuceHeader.h>

// Talks to the lagdaemon.com backend's opt-in feedback/metrics endpoints
// (POST /api/products/creation-station/feedback and /metrics). Both work
// with or without a bearer token -- the backend accepts anonymous
// submissions, matching the two independent opt-ins in FeedbackSettingsStore.
// Network calls are synchronous; callers run these off the message thread
// (see MetricsCollector, and the feedback dialog's submit handler), same
// convention as ContentApiClient elsewhere in this app.
namespace creation_station
{
struct MetricEvent
{
    juce::String eventType;    // "session" | "feature_usage" | "performance" | "crash"
    juce::String eventName;
    juce::String payloadJson;  // a raw JSON object as text, or empty for "{}"
};

class FeedbackMetricsClient
{
public:
    static juce::String apiBase();

    bool submitFeedback(const juce::String& installId,
                        const juce::String& message,
                        const juce::String& category,
                        const juce::String& bearerToken,
                        juce::String& errorMessage) const;

    bool submitMetricsBatch(const juce::String& installId,
                            const juce::Array<MetricEvent>& events,
                            const juce::String& bearerToken,
                            juce::String& errorMessage) const;
};
}
