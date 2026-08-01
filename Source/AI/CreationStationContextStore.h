#pragma once

#include <JuceHeader.h>
#include <creation/assets/ProjectSession.h>
#include <creation/suite/SuiteSettings.h>
#include "CreationStationContextEngine.h"
#include "../Content/ContentLibrary.h"

class CreationStationContextStore final
{
public:
    struct Snapshot
    {
        juce::Array<CreationStationContextEngine::SourceDocument> documents;
        juce::String generatedAt;
    };

    bool rebuild(const creation::assets::ProjectSession& session,
                 const creation::suite::SuiteSettings& suiteSettings,
                 const ContentLibrary& contentLibrary,
                 const juce::String& workspaceMode,
                 const juce::String& patinaSource,
                 juce::String& errorMessage);

    bool load(const juce::File& snapshotFile, juce::String& errorMessage);

    const juce::Array<CreationStationContextEngine::SourceDocument>& getDocuments() const noexcept { return documents; }
    juce::File getSnapshotFile() const noexcept { return snapshotFile; }

private:
    static juce::String readTextPreview(const juce::File& file, int maxCharacters);
    static juce::String makeAssetSummary(const juce::File& file);
    static juce::String documentIdForFile(const juce::String& prefix, const juce::File& file);
    static juce::String joinTags(const juce::StringArray& tags);

    bool writeSnapshot(const juce::File& file, juce::String& errorMessage) const;

    juce::Array<CreationStationContextEngine::SourceDocument> documents;
    juce::File snapshotFile;
};
