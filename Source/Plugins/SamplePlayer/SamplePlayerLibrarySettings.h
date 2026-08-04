#pragma once

#include <JuceHeader.h>

// Persists "where is my sample-pack library folder" independent of any host/project - a VST3
// plugin instance running inside a third-party host has no access to the main app's
// ProjectManager/settings scheme, so this uses a self-contained, OS-special-folder pattern
// (plain juce::File + ValueTree-as-XML, no juce::PropertiesFile/ApplicationProperties
// dependency) reimplemented here since a plugin target can't link against main-app-only
// sources. NOTE: this is one of the loose-file storage sites flagged in the suite-wide
// filesystem-write audit as a violation of the one-VFS model, deferred rather than fixed in
// this pass -- see docs/architecture/Suite-Shared-Project-Model.md.
namespace SamplePlayerLibrarySettings
{
    juce::File getLibraryPath();
    void setLibraryPath(const juce::File& folder);
}
