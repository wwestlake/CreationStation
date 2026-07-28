#pragma once

#include <JuceHeader.h>

// Persists "where is my sample-pack library folder" independent of any host/project - a VST3
// plugin instance running inside a third-party host has no access to the main app's
// ProjectManager/settings scheme, so this mirrors the same self-contained, OS-special-folder
// pattern Source/Auth/DesktopAuthSession.cpp already uses in the main app (plain juce::File +
// ValueTree-as-XML, no juce::PropertiesFile/ApplicationProperties dependency), reimplemented here
// since a plugin target can't link against main-app-only sources.
namespace SamplePlayerLibrarySettings
{
    juce::File getLibraryPath();
    void setLibraryPath(const juce::File& folder);
}
