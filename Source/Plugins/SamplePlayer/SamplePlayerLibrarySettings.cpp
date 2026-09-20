#include "SamplePlayerLibrarySettings.h"

#include <creation/services/SuiteVfsJsonStore.h>

// The sample library folder is a suite setting: it lives in the VFS with the rest, never in the OS user-data folder.
namespace
{
constexpr const char* kSettingsPath = "sample-player-library.json";
}

juce::File SamplePlayerLibrarySettings::getLibraryPath()
{
    juce::String error;
    const auto value = creation::services::SuiteVfsJsonStore::loadJson(kSettingsPath, error);
    const auto path = value.getProperty("libraryPath", juce::var()).toString();
    return path.isNotEmpty() ? juce::File(path) : juce::File();
}

void SamplePlayerLibrarySettings::setLibraryPath(const juce::File& folder)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("libraryPath", folder.getFullPathName());

    juce::String error;
    creation::services::SuiteVfsJsonStore::saveJson(kSettingsPath, juce::var(object), error);
}
