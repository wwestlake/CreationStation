#pragma once

#include <JuceHeader.h>

class VstPluginCatalog final
{
public:
    enum class PluginType
    {
        Instrument,
        Effect,
        MIDI,
        Utility
    };

    enum class PluginFormat
    {
        VST3,
        VST  // Legacy VST2 support (WIP)
    };

    struct Entry
    {
        juce::String name;
        juce::File file;
        PluginType type = PluginType::Effect;
        juce::String category;  // e.g., "Delay", "EQ", "Dynamics"
        juce::String manufacturer;
        PluginFormat format = PluginFormat::VST3;
    };

    void setSearchPaths(const juce::StringArray& newPaths);
    const juce::StringArray& getSearchPaths() const noexcept { return searchPaths; }
    void rescan();
    const juce::Array<Entry>& getEntries() const noexcept { return entries; }
    juce::String describeSummary() const;

    // Categorization helpers
    juce::StringArray getCategories(PluginType type) const;
    juce::Array<Entry> getPluginsByCategory(PluginType type, const juce::String& category) const;
    juce::Array<Entry> getPluginsByType(PluginType type) const;
    juce::StringArray getManufacturers() const;
    juce::Array<Entry> getPluginsByManufacturer(const juce::String& manufacturer) const;

private:
    static juce::String makeDisplayName(const juce::File& file);
    static void parsePluginMetadata(Entry& entry, const juce::PluginDescription& description);
    static void parsePluginMetadataFallback(Entry& entry);

    juce::StringArray searchPaths;
    juce::Array<Entry> entries;
};
