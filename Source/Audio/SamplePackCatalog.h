#pragma once

#include <JuceHeader.h>

// Scans a library folder for Sample Pack Builder outputs (subfolders containing Note_NNN.wav
// files) and lists them as loadable packs - the Sample Player plugin's preset menu is populated
// from this. Modeled directly on VstPluginCatalog's directory-scan pattern (Source/Audio/
// VstPluginCatalog.h/.cpp), reused as-is here since a sample pack folder is conceptually the
// same kind of "thing to discover and list" as a VST3 bundle.
//
// Deliberately dependency-free (only juce::File/juce::Array) so it can be compiled into both the
// main app and the Sample Player VST3 plugin target, which can't link against main-app-only code.
class SamplePackCatalog
{
public:
    struct Entry
    {
        juce::String name;
        juce::File folder;
        int noteCount = 0;
    };

    void setLibraryPath(const juce::File& folder);
    juce::File getLibraryPath() const noexcept { return libraryPath; }
    void rescan();
    const juce::Array<Entry>& getEntries() const noexcept { return entries; }

private:
    juce::File libraryPath;
    juce::Array<Entry> entries;
};
