#include "VstPluginCatalog.h"

void VstPluginCatalog::setSearchPaths(const juce::StringArray& newPaths)
{
    searchPaths = newPaths;
    searchPaths.trim();
    searchPaths.removeEmptyStrings();
    searchPaths.removeDuplicates(false);
}

void VstPluginCatalog::rescan()
{
    entries.clear();

    juce::Array<juce::File> discoveredFiles;

    for (const auto& path : searchPaths)
    {
        auto directory = juce::File(path.trim());
        if (! directory.isDirectory())
            continue;

        // VST2 (.dll) plugins are intentionally excluded: this build only compiles in VST3
        // hosting (JUCE_PLUGINHOST_VST3=1, no legacy JUCE_PLUGINHOST_VST support), so a .dll
        // entry would list a plugin that fails the moment someone tries to load it. Many
        // plugins ship both formats - point a search path at the VST3 build instead.
        directory.findChildFiles(discoveredFiles, juce::File::findFilesAndDirectories, true, "*.vst3");
    }

    juce::StringArray seenPaths;
    for (const auto& file : discoveredFiles)
    {
        auto fullPath = file.getFullPathName();
        if (seenPaths.contains(fullPath))
            continue;

        seenPaths.add(fullPath);
        entries.add({ makeDisplayName(file), file });
    }

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b)
    {
        return a.name.compareIgnoreCase(b.name) < 0;
    });
}

juce::String VstPluginCatalog::describeSummary() const
{
    if (entries.isEmpty())
        return searchPaths.isEmpty() ? "No VST folders configured." : "No VST plugins found in configured folders.";

    return juce::String(entries.size()) + " VST plugin(s) indexed.";
}

juce::String VstPluginCatalog::makeDisplayName(const juce::File& file)
{
    auto name = file.getFileNameWithoutExtension();
    if (name.isEmpty())
        name = file.getFileName();

    return name;
}
