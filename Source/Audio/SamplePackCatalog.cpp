#include "SamplePackCatalog.h"
#include <algorithm>

void SamplePackCatalog::setLibraryPath(const juce::File& folder)
{
    libraryPath = folder;
}

void SamplePackCatalog::rescan()
{
    entries.clear();

    if (! libraryPath.isDirectory())
        return;

    juce::Array<juce::File> subFolders;
    libraryPath.findChildFiles(subFolders, juce::File::findDirectories, false);

    for (const auto& folder : subFolders)
    {
        juce::Array<juce::File> noteFiles;
        folder.findChildFiles(noteFiles, juce::File::findFiles, false, "Note_*.wav");

        if (noteFiles.isEmpty())
            continue;

        Entry entry;
        entry.name = folder.getFileName();
        entry.folder = folder;
        entry.noteCount = noteFiles.size();
        entries.add(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b)
    {
        return a.name < b.name;
    });
}
