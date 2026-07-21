#pragma once

#include <JuceHeader.h>

class ContentLibrary final
{
public:
    enum class Origin
    {
        builtIn,
        downloaded,
        user,
        remote
    };

    enum class AccessState
    {
        installed,
        available,
        locked
    };

    struct Item
    {
        juce::String id;
        juce::String name;
        juce::String type;
        juce::String category;
        juce::String description;
        juce::String requiredTier;
        juce::String version { "local" };
        Origin origin = Origin::user;
        AccessState accessState = AccessState::installed;
        juce::File file;
        int64 fileSizeBytes = 0;
    };

    bool loadFromStorage(const juce::File& builtInDirectory,
                         const juce::File& downloadedDirectory,
                         const juce::File& userDirectory,
                         const juce::File& manifestFile,
                         juce::String& errorMessage);

    const juce::Array<Item>& getItems() const noexcept { return items; }
    juce::String createSummaryText() const;

    static juce::String originName(Origin origin);
    static juce::String accessName(AccessState accessState);

private:
    static juce::String makeItemId(const juce::File& file, Origin origin);
    static juce::String inferTypeFromFile(const juce::File& file);
    static juce::String inferCategoryFromFile(const juce::File& file);
    static void appendFilesFromDirectory(juce::Array<Item>& destination, const juce::File& directory, Origin origin);
    bool writeManifestSnapshot(const juce::File& manifestFile, juce::String& errorMessage) const;

    juce::Array<Item> items;
};
