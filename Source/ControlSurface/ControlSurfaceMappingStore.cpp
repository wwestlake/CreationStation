#include "ControlSurfaceMappingStore.h"

namespace
{
juce::StringArray splitPatternList(const juce::String& pattern)
{
    juce::StringArray parts;
    parts.addTokens(pattern, ",;|", "");
    parts.trim();
    parts.removeEmptyStrings();
    return parts;
}
}

bool ControlSurfaceMappingStore::Profile::matchesDevice(const juce::String& deviceName) const
{
    return ControlSurfaceMappingStore::matchPattern(devicePattern, deviceName);
}

bool ControlSurfaceMappingStore::Profile::matchesUsage(const juce::String& usageName) const
{
    if (usage.isEmpty() || usage == "*")
        return true;

    return usage.equalsIgnoreCase(usageName);
}

void ControlSurfaceMappingStore::clear()
{
    profiles.clear();
    activePresetId = "behringer-studio";
}

void ControlSurfaceMappingStore::addProfile(Profile profile)
{
    profiles.add(std::move(profile));
}

ControlSurfaceMappingStore::Profile* ControlSurfaceMappingStore::findProfileById(const juce::String& id)
{
    for (auto& profile : profiles)
        if (profile.id.equalsIgnoreCase(id))
            return &profile;

    return nullptr;
}

const ControlSurfaceMappingStore::Profile* ControlSurfaceMappingStore::findProfileById(const juce::String& id) const
{
    return const_cast<ControlSurfaceMappingStore*>(this)->findProfileById(id);
}

juce::Array<ControlSurfaceMappingStore::Profile> ControlSurfaceMappingStore::findProfilesForDevice(const juce::String& deviceName,
                                                                                                   const juce::String& usageName) const
{
    juce::Array<Profile> matches;

    for (const auto& profile : profiles)
    {
        if (profile.matchesDevice(deviceName) && profile.matchesUsage(usageName))
            matches.add(profile);
    }

    return matches;
}

juce::var ControlSurfaceMappingStore::bindingToVar(const Binding& binding)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("triggerType", binding.triggerType);
    object->setProperty("actionId", binding.actionId);
    object->setProperty("targetId", binding.targetId);
    object->setProperty("behavior", binding.behavior);
    object->setProperty("channel", binding.channel);
    object->setProperty("number", binding.number);
    object->setProperty("value", binding.value);
    return juce::var(object);
}

bool ControlSurfaceMappingStore::bindingFromVar(const juce::var& value, Binding& binding, juce::String& errorMessage)
{
    if (! value.isObject())
    {
        errorMessage = "A binding entry was not an object.";
        return false;
    }

    auto* object = value.getDynamicObject();
    if (object == nullptr)
    {
        errorMessage = "A binding entry could not be read.";
        return false;
    }

    binding.triggerType = object->getProperty("triggerType").toString();
    binding.actionId = object->getProperty("actionId").toString();
    binding.targetId = object->getProperty("targetId").toString();
    binding.behavior = object->getProperty("behavior").toString();
    if (binding.behavior.isEmpty())
        binding.behavior = "momentary";
    binding.channel = static_cast<int>(object->getProperty("channel"));
    binding.number = static_cast<int>(object->getProperty("number"));
    binding.value = static_cast<int>(object->getProperty("value"));
    return true;
}

juce::var ControlSurfaceMappingStore::profileToVar(const Profile& profile)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", profile.id);
    object->setProperty("displayName", profile.displayName);
    object->setProperty("devicePattern", profile.devicePattern);
    object->setProperty("usage", profile.usage);
    object->setProperty("description", profile.description);

    juce::Array<juce::var> bindings;
    for (const auto& binding : profile.bindings)
        bindings.add(bindingToVar(binding));

    object->setProperty("bindings", juce::var(bindings));
    return juce::var(object);
}

bool ControlSurfaceMappingStore::profileFromVar(const juce::var& value, Profile& profile, juce::String& errorMessage)
{
    if (! value.isObject())
    {
        errorMessage = "A profile entry was not an object.";
        return false;
    }

    auto* object = value.getDynamicObject();
    if (object == nullptr)
    {
        errorMessage = "A profile entry could not be read.";
        return false;
    }

    profile.id = object->getProperty("id").toString();
    profile.displayName = object->getProperty("displayName").toString();
    profile.devicePattern = object->getProperty("devicePattern").toString();
    profile.usage = object->getProperty("usage").toString();
    profile.description = object->getProperty("description").toString();
    profile.bindings.clear();

    auto bindingsValue = object->getProperty("bindings");
    if (bindingsValue.isArray())
    {
        for (const auto& bindingValue : *bindingsValue.getArray())
        {
            Binding binding;
            if (! bindingFromVar(bindingValue, binding, errorMessage))
                return false;

            profile.bindings.add(binding);
        }
    }

    return true;
}

bool ControlSurfaceMappingStore::matchPattern(const juce::String& pattern, const juce::String& value)
{
    auto loweredValue = value.toLowerCase();
    auto candidates = splitPatternList(pattern.toLowerCase());

    if (candidates.isEmpty())
        return pattern.isEmpty() || pattern == "*";

    for (const auto& candidate : candidates)
    {
        if (candidate == "*" || loweredValue.contains(candidate))
            return true;
    }

    return false;
}

bool ControlSurfaceMappingStore::loadFromFile(const juce::File& file, juce::String& errorMessage)
{
    clear();

    if (! file.existsAsFile())
        return true;

    auto parsed = juce::JSON::parse(file.loadFileAsString());
    if (! parsed.isObject())
    {
        errorMessage = "The control surface mappings file is not valid JSON.";
        return false;
    }

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
    {
        errorMessage = "The control surface mappings file could not be read.";
        return false;
    }

    activePresetId = root->getProperty("activePresetId").toString();
    if (activePresetId.isEmpty())
        activePresetId = "behringer-studio";

    auto profilesValue = root->getProperty("profiles");
    if (! profilesValue.isArray())
        return true;

    for (const auto& item : *profilesValue.getArray())
    {
        Profile profile;
        if (! profileFromVar(item, profile, errorMessage))
            return false;

        if (profile.id.isEmpty())
            profile.id = profile.displayName.isNotEmpty() ? profile.displayName.toLowerCase().replaceCharacter(' ', '-') : "profile";

        profiles.add(profile);
    }

    return true;
}

bool ControlSurfaceMappingStore::saveToFile(const juce::File& file, juce::String& errorMessage) const
{
    auto parent = file.getParentDirectory();
    if (! parent.exists() && ! parent.createDirectory())
    {
        errorMessage = "Could not create the control surface mappings folder.";
        return false;
    }

    auto* root = new juce::DynamicObject();
    root->setProperty("version", 1);
    root->setProperty("activePresetId", activePresetId);

    juce::Array<juce::var> profileValues;
    for (const auto& profile : profiles)
        profileValues.add(profileToVar(profile));

    root->setProperty("profiles", juce::var(profileValues));

    if (! file.replaceWithText(juce::JSON::toString(juce::var(root), true)))
    {
        errorMessage = "Could not save the control surface mappings file.";
        return false;
    }

    return true;
}

ControlSurfaceMappingStore ControlSurfaceMappingStore::createDefaultLibrary()
{
    ControlSurfaceMappingStore store;

    Profile xTouchMix;
    xTouchMix.id = "xtouch-mix";
    xTouchMix.displayName = "X-Touch Mix";
    xTouchMix.devicePattern = "x-touch,xtouch,x touch,mackie";
    xTouchMix.usage = "mix";
    xTouchMix.description = "Hands-on track mixing and transport control.";
    xTouchMix.bindings.add({ "transport", "play", "transport_play", "momentary", -1, -1, -1 });
    xTouchMix.bindings.add({ "transport", "stop", "transport_stop", "momentary", -1, -1, -1 });
    xTouchMix.bindings.add({ "transport", "record", "transport_record", "momentary", -1, -1, -1 });
    xTouchMix.bindings.add({ "bank", "step", "bank_left", "momentary", -1, -8, -1 });
    xTouchMix.bindings.add({ "bank", "step", "bank_right", "momentary", -1, 8, -1 });
    store.addProfile(std::move(xTouchMix));

    Profile behringerStudio;
    behringerStudio.id = "behringer-studio";
    behringerStudio.displayName = "Behringer Studio";
    behringerStudio.devicePattern = "x-touch,xtouch,x touch,bcr2000,b-control rotary,b-control,mackie";
    behringerStudio.usage = "*";
    behringerStudio.description = "Studio-wide preset for the X-Touch, BCR2000, voice toggles, and transport.";
    store.addProfile(std::move(behringerStudio));

    Profile bcrMix;
    bcrMix.id = "bcr2000-mix";
    bcrMix.displayName = "BCR2000 Mix";
    bcrMix.devicePattern = "bcr2000,b-control rotary,b-control";
    bcrMix.usage = "mix";
    bcrMix.description = "Rotary control for selected tracks, modules, and macros.";
    bcrMix.bindings.add({ "encoder", "track_gain", "selected_track", "absolute", 1, 1, -1 });
    bcrMix.bindings.add({ "encoder", "track_pan", "selected_track", "absolute", 1, 2, -1 });
    bcrMix.bindings.add({ "button", "track_mute", "selected_track", "toggle", 1, 1, -1 });
    bcrMix.bindings.add({ "button", "track_solo", "selected_track", "toggle", 1, 2, -1 });
    store.addProfile(std::move(bcrMix));

    Profile bcrSignal;
    bcrSignal.id = "bcr2000-signal";
    bcrSignal.displayName = "BCR2000 Signal";
    bcrSignal.devicePattern = "bcr2000,b-control rotary,b-control";
    bcrSignal.usage = "signal";
    bcrSignal.description = "Signal Lab and sound design knobs.";
    bcrSignal.bindings.add({ "encoder", "osc_frequency", "signal_lab", "absolute", 1, 1, -1 });
    bcrSignal.bindings.add({ "encoder", "osc_mix", "signal_lab", "absolute", 1, 2, -1 });
    bcrSignal.bindings.add({ "encoder", "filter_cutoff", "signal_lab", "absolute", 1, 3, -1 });
    bcrSignal.bindings.add({ "encoder", "filter_resonance", "signal_lab", "absolute", 1, 4, -1 });
    store.addProfile(std::move(bcrSignal));

    Profile bcrVoice;
    bcrVoice.id = "bcr2000-voice";
    bcrVoice.displayName = "BCR2000 Voice";
    bcrVoice.devicePattern = "bcr2000,b-control rotary,b-control";
    bcrVoice.usage = "voice";
    bcrVoice.description = "Hands-free voice toggle and talkback controls.";
    bcrVoice.bindings.add({ "footswitch", "voice_toggle", "voice", "toggle", 1, 1, -1 });
    bcrVoice.bindings.add({ "footswitch", "talkback", "voice", "toggle", 1, 2, -1 });
    bcrVoice.bindings.add({ "footswitch", "capture_note", "voice", "toggle", 1, 3, -1 });
    store.addProfile(std::move(bcrVoice));

    return store;
}
