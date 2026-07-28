#include "creation/assets/VirtualFileSystem.h"

#include <algorithm>

namespace creation::assets
{
juce::String VirtualFileSystem::normalizePath(const juce::String& virtualPath)
{
    auto normalized = virtualPath.replaceCharacter('\\', '/');
    while (normalized.startsWithChar('/'))
        normalized = normalized.substring(1);
    return normalized;
}

bool VirtualFileSystem::mount(const juce::File& zipFile, int priority)
{
    if (! zipFile.existsAsFile())
        return false;

    auto zip = std::make_unique<juce::ZipFile>(zipFile);
    if (zip->getNumEntries() <= 0)
        return false;

    const juce::ScopedLock scopedLock(lock);
    mounts.push_back(MountedArchive { std::move(zip), priority, zipFile });
    std::stable_sort(mounts.begin(),
                     mounts.end(),
                     [](const MountedArchive& left, const MountedArchive& right)
                     {
                         return left.priority > right.priority;
                     });
    return true;
}

void VirtualFileSystem::unmountAll()
{
    const juce::ScopedLock scopedLock(lock);
    mounts.clear();
}

bool VirtualFileSystem::exists(const juce::String& virtualPath) const
{
    const auto normalized = normalizePath(virtualPath);

    const juce::ScopedLock scopedLock(lock);
    for (const auto& mount : mounts)
        if (mount.zip->getEntry(normalized) != nullptr)
            return true;

    return false;
}

bool VirtualFileSystem::readFile(const juce::String& virtualPath, juce::MemoryBlock& outData) const
{
    const auto normalized = normalizePath(virtualPath);

    const juce::ScopedLock scopedLock(lock);
    for (const auto& mount : mounts)
    {
        const auto* entry = mount.zip->getEntry(normalized);
        if (entry == nullptr)
            continue;

        std::unique_ptr<juce::InputStream> stream(mount.zip->createStreamForEntry(*entry));
        if (stream == nullptr)
            continue;

        outData.reset();
        stream->readIntoMemoryBlock(outData);
        return true;
    }

    return false;
}
}
