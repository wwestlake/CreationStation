#include "SamplePlayerLibrarySettings.h"

namespace
{
juce::File getSettingsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("CreationStation")
        .getChildFile("sample-player-library.xml");
}
}

juce::File SamplePlayerLibrarySettings::getLibraryPath()
{
    auto file = getSettingsFile();
    if (! file.existsAsFile())
        return {};

    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (xml == nullptr)
        return {};

    return juce::File(xml->getStringAttribute("libraryPath"));
}

void SamplePlayerLibrarySettings::setLibraryPath(const juce::File& folder)
{
    auto file = getSettingsFile();
    file.getParentDirectory().createDirectory();

    juce::XmlElement xml("SamplePlayerLibrary");
    xml.setAttribute("libraryPath", folder.getFullPathName());
    xml.writeTo(file);
}
