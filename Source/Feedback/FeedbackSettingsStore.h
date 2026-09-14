#pragma once

#include <JuceHeader.h>

// Opt-in state for the beta feedback/metrics system, VFS-backed per the
// suite's Storage Boundary Rule (never a raw OS file). Both opt-ins default
// off -- a tester must actively turn each one on.
namespace creation_station
{
class FeedbackSettingsStore
{
public:
    struct Settings
    {
        // A random, client-generated identifier -- never a real device ID,
        // never tied to hardware. Only used to group this install's rows.
        juce::String installId;
        bool feedbackOptIn = false;
        bool metricsOptIn = false;
    };

    // Generates and persists a new installId the first time this is called
    // on a given install; every call after that returns the same one.
    static Settings load();
    static bool save(const Settings& settings, juce::String& errorMessage);
};
}
