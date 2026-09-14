#include "MetricsCollector.h"
#include <thread>

namespace creation_station
{
namespace
{
constexpr int kFlushIntervalMs = 5 * 60 * 1000; // 5 minutes
constexpr int kMaxBatchSize = 200; // matches the backend's per-request cap
}

MetricsCollector::MetricsCollector()
{
    startTimer(kFlushIntervalMs);
}

MetricsCollector::~MetricsCollector()
{
    stopTimer();
}

void MetricsCollector::setOptedIn(bool shouldCollect)
{
    const juce::ScopedLock sl(lock);
    optedIn = shouldCollect;
    if (! optedIn)
        pending.clearQuick();
}

void MetricsCollector::setInstallId(const juce::String& id)
{
    const juce::ScopedLock sl(lock);
    installId = id;
}

void MetricsCollector::setBearerTokenProvider(std::function<juce::String()> provider)
{
    bearerTokenProvider = std::move(provider);
}

void MetricsCollector::logEvent(const juce::String& eventType, const juce::String& eventName, const juce::String& payloadJson)
{
    bool shouldFlush = false;
    {
        const juce::ScopedLock sl(lock);
        if (! optedIn)
            return;

        MetricEvent event;
        event.eventType = eventType;
        event.eventName = eventName;
        event.payloadJson = payloadJson;
        pending.add(event);
        shouldFlush = pending.size() >= kMaxBatchSize;
    }

    if (shouldFlush)
        flush();
}

void MetricsCollector::logFeatureUsage(const juce::String& featureName)
{
    logEvent("feature_usage", featureName);
}

void MetricsCollector::flush()
{
    juce::Array<MetricEvent> batch;
    juce::String installIdSnapshot;
    {
        const juce::ScopedLock sl(lock);
        if (! optedIn || pending.isEmpty() || installId.isEmpty())
            return;
        batch = pending;
        pending.clearQuick();
        installIdSnapshot = installId;
    }

    auto token = bearerTokenProvider != nullptr ? bearerTokenProvider() : juce::String();

    // Captures `client` by value, not `this` -- so this send stays safe even
    // if MetricsCollector (and the MainComponent that owns it) is destroyed
    // before this detached thread finishes, e.g. a flush firing right as the
    // app closes.
    std::thread([client = this->client, batch, installIdSnapshot, token]
    {
        juce::String errorMessage;
        client.submitMetricsBatch(installIdSnapshot, batch, token, errorMessage);
        // Best-effort: metrics aren't user-facing, so a failure here is just
        // dropped rather than reported or re-queued -- re-queuing risks
        // unbounded growth if the backend is unreachable for a while.
    }).detach();
}

void MetricsCollector::timerCallback()
{
    flush();
}
}
