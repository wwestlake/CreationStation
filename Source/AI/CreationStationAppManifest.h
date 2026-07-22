#pragma once

#include <JuceHeader.h>

class CreationStationAppManifest final
{
public:
    juce::String version;
    juce::String instructions;
    juce::StringArray examples;

    static CreationStationAppManifest createDefault(const juce::String& version);

    juce::String toJson() const;
    juce::String checksum() const;

private:
    static juce::String canonicalJsonForChecksum(const CreationStationAppManifest& manifest);
};
