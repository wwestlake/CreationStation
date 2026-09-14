#pragma once

#include <JuceHeader.h>
#include "FeedbackMetricsClient.h"

// Owned by MainComponent. Call logEvent()/logFeatureUsage() from anywhere on
// the message thread; events are buffered in memory and flushed to the
// backend on a timer, on a burst-size trigger, and via an explicit flush()
// at shutdown -- only ever while the user has metrics opted in. When opted
// out, logEvent() is a no-op and drops the event immediately, so nothing is
// collected locally either, not just withheld from the network.
namespace creation_station
{
class MetricsCollector final : private juce::Timer
{
public:
    MetricsCollector();
    ~MetricsCollector() override;

    void setOptedIn(bool shouldCollect);

    // Cached, not re-read from the VFS on every flush -- a VFS round trip
    // must never happen from timerCallback()/flush() on the message thread.
    // Call this once, from wherever FeedbackSettingsStore::load() already
    // ran off the message thread.
    void setInstallId(const juce::String& id);

    // Called lazily, right before a flush -- so a token obtained after
    // startup (signing in mid-session) still gets attached to later batches.
    void setBearerTokenProvider(std::function<juce::String()> provider);

    void logEvent(const juce::String& eventType, const juce::String& eventName, const juce::String& payloadJson = {});
    void logFeatureUsage(const juce::String& featureName);

    // Best-effort, non-blocking: launches a background send and returns
    // immediately. Safe to call from the destructor path.
    void flush();

private:
    void timerCallback() override;

    juce::CriticalSection lock;
    juce::Array<MetricEvent> pending;
    bool optedIn = false;
    juce::String installId;
    std::function<juce::String()> bearerTokenProvider;
    FeedbackMetricsClient client;
};
}
