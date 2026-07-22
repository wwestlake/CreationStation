#pragma once

#include <JuceHeader.h>

class ControlSurfaceMappingStore final
{
public:
    struct Binding
    {
        juce::String triggerType;
        juce::String actionId;
        juce::String targetId;
        juce::String behavior = "momentary";
        int channel = -1;
        int number = -1;
        int value = -1;
    };

    struct Profile
    {
        juce::String id;
        juce::String displayName;
        juce::String devicePattern;
        juce::String usage;
        juce::String description;
        juce::Array<Binding> bindings;

        bool matchesDevice(const juce::String& deviceName) const;
        bool matchesUsage(const juce::String& usageName) const;
    };

    void clear();
    void addProfile(Profile profile);

    juce::String getActivePresetId() const noexcept { return activePresetId; }
    void setActivePresetId(const juce::String& presetId) { activePresetId = presetId; }

    const juce::Array<Profile>& getProfiles() const noexcept { return profiles; }
    Profile* findProfileById(const juce::String& id);
    const Profile* findProfileById(const juce::String& id) const;
    juce::Array<Profile> findProfilesForDevice(const juce::String& deviceName,
                                               const juce::String& usageName = {}) const;

    bool loadFromFile(const juce::File& file, juce::String& errorMessage);
    bool saveToFile(const juce::File& file, juce::String& errorMessage) const;

    static ControlSurfaceMappingStore createDefaultLibrary();

private:
    static juce::var bindingToVar(const Binding& binding);
    static bool bindingFromVar(const juce::var& value, Binding& binding, juce::String& errorMessage);
    static juce::var profileToVar(const Profile& profile);
    static bool profileFromVar(const juce::var& value, Profile& profile, juce::String& errorMessage);
    static bool matchPattern(const juce::String& pattern, const juce::String& value);

    juce::Array<Profile> profiles;
    juce::String activePresetId = "behringer-studio";
};
