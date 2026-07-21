#pragma once

#include <JuceHeader.h>

class VstPluginCatalog final
{
public:
    struct Entry
    {
        juce::String name;
        juce::File file;
    };

    void setSearchPaths(const juce::StringArray& newPaths);
    const juce::StringArray& getSearchPaths() const noexcept { return searchPaths; }
    void rescan();
    const juce::Array<Entry>& getEntries() const noexcept { return entries; }
    juce::String describeSummary() const;

private:
    static juce::String makeDisplayName(const juce::File& file);

    juce::StringArray searchPaths;
    juce::Array<Entry> entries;
};
